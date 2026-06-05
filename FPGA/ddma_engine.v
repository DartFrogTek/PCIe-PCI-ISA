//============================================================================
// ddma_engine.v — Distributed DMA Controller
//
// Implements 8 DMA channels (0-3 = 8-bit, 5-7 = 16-bit, 4 = cascade/unused).
// Register layout per channel matches IT8888 DDMA windows:
//
//   base+0x0..0x3 : 32-bit DMA address (4 bytes, little-endian)
//   base+0x4..0x5 : 16-bit transfer count (count+1 = actual transfers)
//   base+0x8      : command/status
//   base+0x9      : software request
//   base+0xB      : mode (8237-compatible)
//   base+0xD      : master clear
//   base+0xF      : single-channel mask
//
// DDMA window base addresses are configured via cfg_ch01..cfg_ch67
// (mirrors IT8888 PCI config registers 0x40-0x4C).
//
// Transfer flow (single mode, ISA→RAM):
//   ISA DREQ → assert DACK + IOR# → capture SD → PCI master write → update
//
// Transfer flow (single mode, RAM→ISA):
//   ISA DREQ → PCI master read → drive SD + assert DACK + IOW# → update
//============================================================================

module ddma_engine (
    input  wire        clk,          // PCI clock (33 MHz)
    input  wire        rst_n,

    // ===== DDMA Window Configuration =====
    // Directly mirrors IT8888 cfg40-cfg4C
    // [15:4] = ch N base, bit[0] = ch N enable
    // [31:20] = ch N+1 base, bit[16] = ch N+1 enable
    input  wire [31:0] cfg_ch01,
    input  wire [31:0] cfg_ch23,
    input  wire [31:0] cfg_ch45,
    input  wire [31:0] cfg_ch67,

    // ===== Register Access (from PCI target I/O path) =====
    input  wire [15:0] reg_addr,
    input  wire [7:0]  reg_wdata,
    output reg  [7:0]  reg_rdata,
    input  wire        reg_wr,
    input  wire        reg_rd,
    output wire        reg_hit,      // address is a DDMA window

    // ===== DMA Buffer (driver-allocated contiguous physical memory) =====
    input  wire [31:0] dma_buf_base, // physical base address
    input  wire [31:0] dma_buf_size, // size in bytes

    // ===== PCI Master Interface =====
    output reg         pci_mst_start,
    input  wire        pci_mst_busy,
    output reg  [31:0] pci_mst_addr,
    output reg  [3:0]  pci_mst_be,
    output reg  [31:0] pci_mst_wdata,
    input  wire [31:0] pci_mst_rdata,
    output reg         pci_mst_is_read,
    output reg         pci_mst_is_write,
    input  wire        pci_mst_done,
    input  wire        pci_mst_abort,

    // ===== ISA DMA Bus (directly muxed at bridge top) =====
    input  wire [7:0]  isa_dreq,     // from ISA devices (active high)
    output reg  [7:0]  isa_dack_n,   // to ISA devices (active low)
    output reg         isa_tc_out,   // terminal count
    output reg  [15:0] isa_sd_out,   // data to ISA device
    input  wire [15:0] isa_sd_in,    // data from ISA device
    output reg         isa_sd_oe,    // drive SD bus
    output reg         isa_ior_n_out,// I/O read strobe
    output reg         isa_iow_n_out,// I/O write strobe
    output wire        dma_active    // 1 = DMA owns ISA bus (for top-level mux)
);

    integer i;  // loop variable for resets

    // ==========================================================
    // DDMA Window Base Addresses + Enable Bits
    // ==========================================================
    wire [15:0] ch_base [0:7];
    wire [7:0]  ch_en;

    assign ch_base[0] = {cfg_ch01[15:4],  4'h0};
    assign ch_base[1] = {cfg_ch01[31:20], 4'h0};
    assign ch_base[2] = {cfg_ch23[15:4],  4'h0};
    assign ch_base[3] = {cfg_ch23[31:20], 4'h0};
    assign ch_base[4] = {cfg_ch45[15:4],  4'h0};  // cascade, typically 0
    assign ch_base[5] = {cfg_ch45[31:20], 4'h0};
    assign ch_base[6] = {cfg_ch67[15:4],  4'h0};
    assign ch_base[7] = {cfg_ch67[31:20], 4'h0};

    assign ch_en[0] = cfg_ch01[0];
    assign ch_en[1] = cfg_ch01[16];
    assign ch_en[2] = cfg_ch23[0];
    assign ch_en[3] = cfg_ch23[16];
    assign ch_en[4] = cfg_ch45[0];
    assign ch_en[5] = cfg_ch45[16];
    assign ch_en[6] = cfg_ch67[0];
    assign ch_en[7] = cfg_ch67[16];

    // ==========================================================
    // Per-Channel Register File
    // ==========================================================
    reg [31:0] ch_base_addr [0:7];   // programmed 32-bit DMA address
    reg [15:0] ch_base_count[0:7];   // programmed transfer count
    reg [31:0] ch_cur_addr  [0:7];   // working address (incremented)
    reg [15:0] ch_cur_count [0:7];   // working count (decremented)
    reg [7:0]  ch_mode      [0:7];   // 8237 mode register
    reg [7:0]  ch_mask;              // per-channel mask (1=masked/disabled)
    reg [7:0]  ch_status;            // bit[n] = TC reached on channel n
    reg [7:0]  ch_request;           // software DMA request
    reg        ff0;                  // byte flip-flop for ch 0-3
    reg        ff1;                  // byte flip-flop for ch 5-7

    // Mode register field extraction
    // [1:0] = channel select (within the register write)
    // [3:2] = transfer direction: 01=ISA→RAM(verify), 10=ISA→RAM, 01=RAM→ISA
    //         Actually 8237: 00=verify, 01=write(dev→mem), 10=read(mem→dev)
    //         For DDMA: 01=ISA device→PCI RAM, 10=PCI RAM→ISA device
    // [5:4] = transfer mode: 00=demand, 01=single, 10=block, 11=cascade
    // [6]   = address direction: 0=increment, 1=decrement
    // [7]   = auto-init: 1=reload base addr/count after TC

    wire [2:0] active_ch;  // forward declaration for mode access
    wire [1:0] xfer_dir   = ch_mode[active_ch][3:2];
    wire [1:0] xfer_mode  = ch_mode[active_ch][5:4];
    wire       addr_dec   = ch_mode[active_ch][6];
    wire       auto_init  = ch_mode[active_ch][7];
    wire       is_16bit_ch= (active_ch >= 3'd5);  // channels 5-7 are 16-bit

    // Direction decode
    wire dir_isa_to_ram = (xfer_dir == 2'b01);  // 8237 "write" = device→memory
    wire dir_ram_to_isa = (xfer_dir == 2'b10);  // 8237 "read"  = memory→device

    // ==========================================================
    // DDMA Window Address Decode
    // ==========================================================
    reg [2:0]  hit_ch;
    reg [3:0]  hit_off;
    reg        hit_valid;

    always @(*) begin
        hit_valid = 1'b0;
        hit_ch    = 3'd0;
        hit_off   = reg_addr[3:0];

        if      (ch_en[0] && reg_addr[15:4] == ch_base[0][15:4]) begin hit_valid=1; hit_ch=3'd0; end
        else if (ch_en[1] && reg_addr[15:4] == ch_base[1][15:4]) begin hit_valid=1; hit_ch=3'd1; end
        else if (ch_en[2] && reg_addr[15:4] == ch_base[2][15:4]) begin hit_valid=1; hit_ch=3'd2; end
        else if (ch_en[3] && reg_addr[15:4] == ch_base[3][15:4]) begin hit_valid=1; hit_ch=3'd3; end
        else if (ch_en[5] && reg_addr[15:4] == ch_base[5][15:4]) begin hit_valid=1; hit_ch=3'd5; end
        else if (ch_en[6] && reg_addr[15:4] == ch_base[6][15:4]) begin hit_valid=1; hit_ch=3'd6; end
        else if (ch_en[7] && reg_addr[15:4] == ch_base[7][15:4]) begin hit_valid=1; hit_ch=3'd7; end
    end

    assign reg_hit = hit_valid;

    // Register offset constants (within 16-byte channel window)
    localparam [3:0]
        OFF_ADDR0   = 4'h0, OFF_ADDR1   = 4'h1,
        OFF_ADDR2   = 4'h2, OFF_ADDR3   = 4'h3,
        OFF_COUNT0  = 4'h4, OFF_COUNT1  = 4'h5,
        OFF_CMDSTAT = 4'h8, OFF_REQUEST = 4'h9,
        OFF_MODE    = 4'hB, OFF_MSTRCLR = 4'hD,
        OFF_MASK    = 4'hF;

    // ==========================================================
    // Register Read
    // ==========================================================
    always @(*) begin
        reg_rdata = 8'h00;
        if (hit_valid && reg_rd) begin
            case (hit_off)
                OFF_ADDR0:   reg_rdata = ch_cur_addr[hit_ch][7:0];
                OFF_ADDR1:   reg_rdata = ch_cur_addr[hit_ch][15:8];
                OFF_ADDR2:   reg_rdata = ch_cur_addr[hit_ch][23:16];
                OFF_ADDR3:   reg_rdata = ch_cur_addr[hit_ch][31:24];
                OFF_COUNT0:  reg_rdata = ch_cur_count[hit_ch][7:0];
                OFF_COUNT1:  reg_rdata = ch_cur_count[hit_ch][15:8];
                OFF_CMDSTAT: reg_rdata = ch_status;
                OFF_MODE:    reg_rdata = ch_mode[hit_ch];
                OFF_MASK:    reg_rdata = ch_mask;
                default:     reg_rdata = 8'h00;
            endcase
        end
    end

    // ==========================================================
    // Register Write
    // ==========================================================
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            for (i = 0; i < 8; i = i + 1) begin
                ch_base_addr[i]  <= 32'h0;
                ch_base_count[i] <= 16'h0;
                ch_cur_addr[i]   <= 32'h0;
                ch_cur_count[i]  <= 16'h0;
                ch_mode[i]       <= 8'h0;
            end
            ch_mask    <= 8'hFF;  // all channels masked on reset
            ch_status  <= 8'h00;
            ch_request <= 8'h00;
            ff0        <= 1'b0;
            ff1        <= 1'b0;
        end else if (hit_valid && reg_wr) begin
            case (hit_off)

            OFF_ADDR0: begin
                ch_base_addr[hit_ch][7:0]  <= reg_wdata;
                ch_cur_addr[hit_ch][7:0]   <= reg_wdata;
            end
            OFF_ADDR1: begin
                ch_base_addr[hit_ch][15:8] <= reg_wdata;
                ch_cur_addr[hit_ch][15:8]  <= reg_wdata;
            end
            OFF_ADDR2: begin
                ch_base_addr[hit_ch][23:16] <= reg_wdata;
                ch_cur_addr[hit_ch][23:16]  <= reg_wdata;
            end
            OFF_ADDR3: begin
                ch_base_addr[hit_ch][31:24] <= reg_wdata;
                ch_cur_addr[hit_ch][31:24]  <= reg_wdata;
            end
            OFF_COUNT0: begin
                ch_base_count[hit_ch][7:0] <= reg_wdata;
                ch_cur_count[hit_ch][7:0]  <= reg_wdata;
            end
            OFF_COUNT1: begin
                ch_base_count[hit_ch][15:8] <= reg_wdata;
                ch_cur_count[hit_ch][15:8]  <= reg_wdata;
            end
            OFF_CMDSTAT: begin
                // Write to command (reads as status)
                // Bit 2: disable controller — we'll just clear status on write
                ch_status <= ch_status & ~reg_wdata;
            end
            OFF_REQUEST: begin
                // Software DMA request
                ch_request[hit_ch] <= reg_wdata[2]; // bit 2 = set request
            end
            OFF_MODE: begin
                ch_mode[hit_ch] <= reg_wdata;
            end
            OFF_MSTRCLR: begin
                // Master clear: reset this channel
                ch_base_addr[hit_ch]  <= 32'h0;
                ch_base_count[hit_ch] <= 16'h0;
                ch_cur_addr[hit_ch]   <= 32'h0;
                ch_cur_count[hit_ch]  <= 16'h0;
                ch_mode[hit_ch]       <= 8'h0;
                ch_mask[hit_ch]       <= 1'b1;  // mask channel
                ch_status[hit_ch]     <= 1'b0;
                ch_request[hit_ch]    <= 1'b0;
                if (hit_ch < 4) ff0 <= 1'b0;
                else            ff1 <= 1'b0;
            end
            OFF_MASK: begin
                // Single channel mask
                ch_mask[hit_ch] <= reg_wdata[2]; // bit 2 = mask bit
            end

            endcase
        end
    end

    // ==========================================================
    // DREQ Synchronization (async ISA → PCI clock domain)
    // ==========================================================
    reg [7:0] dreq_s1, dreq_s2;
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            dreq_s1 <= 8'h00;
            dreq_s2 <= 8'h00;
        end else begin
            dreq_s1 <= isa_dreq;
            dreq_s2 <= dreq_s1;
        end
    end

    // Active (unmasked) requests: hardware DREQ or software request
    wire [7:0] pending = (dreq_s2 | ch_request) & ~ch_mask;

    // ==========================================================
    // DMA Channel Arbitration (fixed priority, ch 0 highest)
    // ==========================================================
    reg [2:0] arb_ch;
    reg       arb_valid;

    always @(*) begin
        arb_valid = 1'b0;
        arb_ch    = 3'd0;
        if      (pending[0]) begin arb_valid=1; arb_ch=3'd0; end
        else if (pending[1]) begin arb_valid=1; arb_ch=3'd1; end
        else if (pending[2]) begin arb_valid=1; arb_ch=3'd2; end
        else if (pending[3]) begin arb_valid=1; arb_ch=3'd3; end
        else if (pending[5]) begin arb_valid=1; arb_ch=3'd5; end
        else if (pending[6]) begin arb_valid=1; arb_ch=3'd6; end
        else if (pending[7]) begin arb_valid=1; arb_ch=3'd7; end
    end

    // ==========================================================
    // DMA Transfer FSM
    // ==========================================================
    localparam [3:0]
        D_IDLE       = 4'd0,
        D_LATCH_CH   = 4'd1,   // latch active channel parameters
        // ISA→RAM path
        D_ISA_RD_SET = 4'd2,   // assert DACK, setup IOR#
        D_ISA_RD_STB = 4'd3,   // IOR# active, wait ISA timing
        D_ISA_RD_CAP = 4'd4,   // capture SD, release IOR#
        D_PCI_WR     = 4'd5,   // PCI master write to RAM
        D_PCI_WR_W   = 4'd6,   // wait for PCI completion
        // RAM→ISA path
        D_PCI_RD     = 4'd7,   // PCI master read from RAM
        D_PCI_RD_W   = 4'd8,   // wait for PCI completion
        D_ISA_WR_SET = 4'd9,   // assert DACK, drive SD, setup IOW#
        D_ISA_WR_STB = 4'd10,  // IOW# active, wait ISA timing
        D_ISA_WR_REL = 4'd11,  // release IOW#
        // Common completion
        D_UPDATE     = 4'd12,  // update address/count
        D_TC         = 4'd13;  // terminal count reached

    reg [3:0]  dma_state;
    reg [2:0]  dma_ch;           // currently active channel
    reg [15:0] dma_data;         // data being transferred
    reg [7:0]  isa_timer;        // ISA timing counter (counts PCI clocks)
    reg        tc_reached;       // terminal count flag for current transfer

    assign dma_active = (dma_state != D_IDLE);
    assign active_ch  = dma_ch;

    // ISA timing thresholds (in PCI clocks, PCI=33MHz, ISA=~8MHz)
    // These are conservative; tune for your actual ISA clock derivation
    localparam [7:0]
        T_DACK_SETUP = 8'd4,    // DACK asserted before strobe
        T_STROBE_ON  = 8'd8,    // strobe held active (after setup)
        T_STROBE_END = 8'd12,   // strobe released, data hold
        T_CLEANUP    = 8'd16;   // DACK released

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            dma_state     <= D_IDLE;
            dma_ch        <= 3'd0;
            dma_data      <= 16'h0;
            isa_timer     <= 8'h0;
            tc_reached    <= 1'b0;
            isa_dack_n    <= 8'hFF;
            isa_tc_out    <= 1'b0;
            isa_sd_out    <= 16'h0;
            isa_sd_oe     <= 1'b0;
            isa_ior_n_out <= 1'b1;
            isa_iow_n_out <= 1'b1;
            pci_mst_start <= 1'b0;
            pci_mst_addr  <= 32'h0;
            pci_mst_be    <= 4'h0;
            pci_mst_wdata <= 32'h0;
            pci_mst_is_read  <= 1'b0;
            pci_mst_is_write <= 1'b0;
        end else begin
            // Default: clear pulses
            pci_mst_start <= 1'b0;
            isa_tc_out    <= 1'b0;

            case (dma_state)

            // --------------------------------------------------
            D_IDLE: begin
                isa_dack_n    <= 8'hFF;
                isa_sd_oe     <= 1'b0;
                isa_ior_n_out <= 1'b1;
                isa_iow_n_out <= 1'b1;

                if (arb_valid) begin
                    dma_ch    <= arb_ch;
                    dma_state <= D_LATCH_CH;
                end
            end

            // --------------------------------------------------
            D_LATCH_CH: begin
                // Latch channel and decide direction
                tc_reached <= 1'b0;
                isa_timer  <= 8'h0;

                if (dir_isa_to_ram)
                    dma_state <= D_ISA_RD_SET;
                else if (dir_ram_to_isa)
                    dma_state <= D_PCI_RD;
                else
                    dma_state <= D_IDLE;  // verify mode = no transfer
            end

            // ==================================================
            // ISA → RAM: Read from ISA device, write to PCI RAM
            // ==================================================

            D_ISA_RD_SET: begin
                // Assert DACK for active channel
                isa_dack_n    <= ~(8'h01 << dma_ch);
                isa_ior_n_out <= 1'b1;   // not yet
                isa_sd_oe     <= 1'b0;   // device drives SD
                isa_timer     <= isa_timer + 8'd1;

                if (isa_timer >= T_DACK_SETUP) begin
                    isa_ior_n_out <= 1'b0;  // assert IOR#
                    isa_timer     <= 8'd0;
                    dma_state     <= D_ISA_RD_STB;
                end
            end

            D_ISA_RD_STB: begin
                // Hold IOR# active
                isa_timer <= isa_timer + 8'd1;
                if (isa_timer >= T_STROBE_ON) begin
                    dma_state <= D_ISA_RD_CAP;
                end
            end

            D_ISA_RD_CAP: begin
                // Capture data from ISA device
                dma_data      <= is_16bit_ch ? isa_sd_in : {8'h00, isa_sd_in[7:0]};
                isa_ior_n_out <= 1'b1;   // release IOR#
                isa_dack_n    <= 8'hFF;  // release DACK
                isa_timer     <= 8'd0;

                // Now write this data to PCI RAM
                dma_state <= D_PCI_WR;
            end

            D_PCI_WR: begin
                if (!pci_mst_busy) begin
                    pci_mst_addr     <= dma_buf_base + ch_cur_addr[dma_ch];
                    pci_mst_is_read  <= 1'b0;
                    pci_mst_is_write <= 1'b1;
                    pci_mst_start    <= 1'b1;

                    // Place data in correct PCI byte lanes
                    if (is_16bit_ch) begin
                        case (ch_cur_addr[dma_ch][1:0])
                            2'b00: begin pci_mst_wdata <= {16'h0, dma_data};       pci_mst_be <= 4'b0011; end
                            2'b10: begin pci_mst_wdata <= {dma_data, 16'h0};       pci_mst_be <= 4'b1100; end
                            default: begin pci_mst_wdata <= {16'h0, dma_data};     pci_mst_be <= 4'b0011; end
                        endcase
                    end else begin
                        case (ch_cur_addr[dma_ch][1:0])
                            2'b00: begin pci_mst_wdata <= {24'h0, dma_data[7:0]};       pci_mst_be <= 4'b0001; end
                            2'b01: begin pci_mst_wdata <= {16'h0, dma_data[7:0], 8'h0};  pci_mst_be <= 4'b0010; end
                            2'b10: begin pci_mst_wdata <= {8'h0, dma_data[7:0], 16'h0};  pci_mst_be <= 4'b0100; end
                            2'b11: begin pci_mst_wdata <= {dma_data[7:0], 24'h0};        pci_mst_be <= 4'b1000; end
                        endcase
                    end

                    dma_state <= D_PCI_WR_W;
                end
            end

            D_PCI_WR_W: begin
                if (pci_mst_done || pci_mst_abort)
                    dma_state <= D_UPDATE;
            end

            // ==================================================
            // RAM → ISA: Read from PCI RAM, write to ISA device
            // ==================================================

            D_PCI_RD: begin
                if (!pci_mst_busy) begin
                    pci_mst_addr     <= dma_buf_base + ch_cur_addr[dma_ch];
                    pci_mst_is_read  <= 1'b1;
                    pci_mst_is_write <= 1'b0;
                    pci_mst_start    <= 1'b1;
                    pci_mst_be       <= is_16bit_ch ? 4'b0011 : 4'b0001;

                    // Adjust byte enables for address alignment
                    if (is_16bit_ch) begin
                        pci_mst_be <= (ch_cur_addr[dma_ch][1]) ? 4'b1100 : 4'b0011;
                    end else begin
                        case (ch_cur_addr[dma_ch][1:0])
                            2'b00: pci_mst_be <= 4'b0001;
                            2'b01: pci_mst_be <= 4'b0010;
                            2'b10: pci_mst_be <= 4'b0100;
                            2'b11: pci_mst_be <= 4'b1000;
                        endcase
                    end

                    dma_state <= D_PCI_RD_W;
                end
            end

            D_PCI_RD_W: begin
                if (pci_mst_done) begin
                    // Extract data from correct PCI byte lanes
                    if (is_16bit_ch)
                        dma_data <= (ch_cur_addr[dma_ch][1]) ?
                            pci_mst_rdata[31:16] : pci_mst_rdata[15:0];
                    else begin
                        case (ch_cur_addr[dma_ch][1:0])
                            2'b00: dma_data <= {8'h0, pci_mst_rdata[7:0]};
                            2'b01: dma_data <= {8'h0, pci_mst_rdata[15:8]};
                            2'b10: dma_data <= {8'h0, pci_mst_rdata[23:16]};
                            2'b11: dma_data <= {8'h0, pci_mst_rdata[31:24]};
                        endcase
                    end
                    isa_timer <= 8'd0;
                    dma_state <= D_ISA_WR_SET;
                end else if (pci_mst_abort) begin
                    dma_state <= D_IDLE;  // abort this transfer
                end
            end

            D_ISA_WR_SET: begin
                // Assert DACK, drive data onto SD, prepare IOW#
                isa_dack_n <= ~(8'h01 << dma_ch);
                isa_sd_out <= dma_data;
                isa_sd_oe  <= 1'b1;
                isa_timer  <= isa_timer + 8'd1;

                if (isa_timer >= T_DACK_SETUP) begin
                    isa_iow_n_out <= 1'b0;  // assert IOW#
                    isa_timer     <= 8'd0;
                    dma_state     <= D_ISA_WR_STB;
                end
            end

            D_ISA_WR_STB: begin
                isa_timer <= isa_timer + 8'd1;
                if (isa_timer >= T_STROBE_ON) begin
                    dma_state <= D_ISA_WR_REL;
                end
            end

            D_ISA_WR_REL: begin
                isa_iow_n_out <= 1'b1;   // release IOW#
                isa_sd_oe     <= 1'b0;
                isa_dack_n    <= 8'hFF;
                isa_timer     <= 8'd0;
                dma_state     <= D_UPDATE;
            end

            // ==================================================
            // Update address and count
            // ==================================================

            D_UPDATE: begin
                // Increment or decrement address
                if (is_16bit_ch) begin
                    if (addr_dec)
                        ch_cur_addr[dma_ch] <= ch_cur_addr[dma_ch] - 32'd2;
                    else
                        ch_cur_addr[dma_ch] <= ch_cur_addr[dma_ch] + 32'd2;
                end else begin
                    if (addr_dec)
                        ch_cur_addr[dma_ch] <= ch_cur_addr[dma_ch] - 32'd1;
                    else
                        ch_cur_addr[dma_ch] <= ch_cur_addr[dma_ch] + 32'd1;
                end

                // Decrement count
                if (ch_cur_count[dma_ch] == 16'h0000) begin
                    // Terminal count — count wraps from 0 to FFFF
                    tc_reached <= 1'b1;
                    dma_state  <= D_TC;
                end else begin
                    ch_cur_count[dma_ch] <= ch_cur_count[dma_ch] - 16'd1;

                    // For single mode, return to idle after each byte/word
                    // (device must re-assert DREQ for next transfer)
                    if (xfer_mode == 2'b01)
                        dma_state <= D_IDLE;
                    // For demand mode, continue while DREQ is held
                    else if (xfer_mode == 2'b00) begin
                        if (dreq_s2[dma_ch])
                            dma_state <= D_LATCH_CH;  // continue
                        else
                            dma_state <= D_IDLE;       // DREQ dropped
                    end
                    // For block mode, continue until TC
                    else if (xfer_mode == 2'b10)
                        dma_state <= D_LATCH_CH;
                    else
                        dma_state <= D_IDLE;
                end
            end

            D_TC: begin
                // Terminal count reached
                ch_status[dma_ch] <= 1'b1;
                ch_mask[dma_ch]   <= 1'b1;  // mask channel
                ch_request[dma_ch]<= 1'b0;  // clear software request
                isa_tc_out        <= 1'b1;   // pulse TC on ISA bus

                // Auto-init: reload base address and count
                if (auto_init) begin
                    ch_cur_addr[dma_ch]  <= ch_base_addr[dma_ch];
                    ch_cur_count[dma_ch] <= ch_base_count[dma_ch];
                    ch_mask[dma_ch]      <= 1'b0;  // leave unmasked
                end

                dma_state <= D_IDLE;
            end

            default: dma_state <= D_IDLE;

            endcase
        end
    end

endmodule