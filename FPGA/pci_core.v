//============================================================================
// pci_core.v — Minimal PCI 2.2 Target + Master, pure Verilog
//
// Type 0 config header with 2 BARs (I/O + Memory).
// Single-DWORD transfers only (sufficient for ISA bridge).
// No burst, no 64-bit, no capabilities list.
//
// Vendor config space (offsets 0x40+) forwarded to external logic
// via the vcfg interface for DDMA registers, DMA buffer config, etc.
//
// Target flow:
//   Host starts cycle → we decode address → assert DEVSEL# →
//   wait for backend (tgt_rdy) → assert TRDY# → transfer → done
//
// Master flow:
//   Backend pulses mst_start → we assert REQ# → get GNT# →
//   drive address phase → data phase → capture/drive data → mst_done
//============================================================================

module pci_core #(
    parameter [15:0] VENDOR_ID   = 16'hDFFF,
    parameter [15:0] DEVICE_ID   = 16'h8888,
    parameter [7:0]  REVISION_ID = 8'h01,
    parameter [23:0] CLASS_CODE  = 24'h060100,  // PCI-to-ISA bridge
    parameter [15:0] SUBSYS_VID  = 16'hDFFF,
    parameter [15:0] SUBSYS_DID  = 16'h0001,
    parameter        BAR0_BITS   = 8,            // I/O BAR: 2^8 = 256 ports
    parameter        BAR1_BITS   = 12            // Mem BAR: 2^12 = 4K
)(
    // ==================== PCI Bus Pins ====================
    input  wire        pci_clk,
    input  wire        pci_rst_n,

    inout  wire [31:0] pci_ad,
    inout  wire [3:0]  pci_cbe_n,
    inout  wire        pci_frame_n,
    inout  wire        pci_irdy_n,
    output wire        pci_trdy_n,
    output wire        pci_stop_n,
    output wire        pci_devsel_n,
    input  wire        pci_idsel,
    inout  wire        pci_par,
    output wire        pci_perr_n,
    output wire        pci_serr_n,
    output wire        pci_req_n,
    input  wire        pci_gnt_n,
    output wire        pci_inta_n,

    // ==================== Target Interface ====================
    output reg  [31:0] tgt_addr,       // latched PCI address
    output reg  [3:0]  tgt_be,         // byte enables (active HIGH)
    output reg  [31:0] tgt_wdata,      // write data from host
    input  wire [31:0] tgt_rdata,      // read data to host
    output reg         tgt_io_hit,     // address in BAR0 (I/O)
    output reg         tgt_mem_hit,    // address in BAR1 (Memory)
    output reg         tgt_is_read,
    output reg         tgt_is_write,
    output reg         tgt_start,      // cycle starting (1-clk pulse)
    input  wire        tgt_rdy,        // backend ready (controls TRDY#)

    // ==================== Master Interface ====================
    input  wire        mst_start,      // pulse to begin
    output wire        mst_busy,       // master FSM active
    input  wire [31:0] mst_addr,
    input  wire [3:0]  mst_be,         // byte enables (active HIGH)
    input  wire [31:0] mst_wdata,
    output reg  [31:0] mst_rdata,
    input  wire        mst_is_read,
    input  wire        mst_is_write,
    input  wire        mst_is_mem,     // 1=memory, 0=I/O
    output reg         mst_done,       // cycle complete (pulse)
    output reg         mst_abort,      // master abort (no DEVSEL#)

    // ==================== Vendor Config (0x40+) ====================
    output wire [7:0]  vcfg_offset,    // register byte offset within config
    output wire [31:0] vcfg_wdata,     // write data
    output wire [3:0]  vcfg_wbe,       // write byte enables (active HIGH)
    input  wire [31:0] vcfg_rdata,     // read data (directly combinational)
    output reg         vcfg_wr,        // write strobe (1-clk pulse)

    // ==================== Interrupt ====================
    input  wire        irq_active      // assert INTA# (active low, open-drain)
);

    // ==========================================================
    // PCI Command Encodings (on CBE during address phase)
    // ==========================================================
    localparam [3:0] CMD_IO_RD  = 4'b0010,
                     CMD_IO_WR  = 4'b0011,
                     CMD_MEM_RD = 4'b0110,
                     CMD_MEM_WR = 4'b0111,
                     CMD_CFG_RD = 4'b1010,
                     CMD_CFG_WR = 4'b1011;

    // ==========================================================
    // Tri-State Handling
    // ==========================================================
    reg  [31:0] ad_out;
    reg         ad_oe;
    wire [31:0] ad_in = pci_ad;
    assign pci_ad = ad_oe ? ad_out : 32'hZZZZ_ZZZZ;

    reg  [3:0]  cbe_out;
    reg         cbe_oe;
    wire [3:0]  cbe_in = pci_cbe_n;
    assign pci_cbe_n = cbe_oe ? cbe_out : 4'hZ;

    reg         frame_n_out;
    reg         frame_oe;
    wire        frame_n_in = pci_frame_n;
    assign pci_frame_n = frame_oe ? frame_n_out : 1'bZ;

    reg         irdy_n_out;
    reg         irdy_oe;
    wire        irdy_n_in = pci_irdy_n;
    assign pci_irdy_n = irdy_oe ? irdy_n_out : 1'bZ;

    // Target-only outputs (directly driven, active-low)
    reg devsel_n_r, trdy_n_r, stop_n_r;
    assign pci_devsel_n = devsel_n_r;
    assign pci_trdy_n   = trdy_n_r;
    assign pci_stop_n   = stop_n_r;

    // Master-only output
    reg req_n_r;
    assign pci_req_n = req_n_r;

    // Parity: even parity over AD[31:0] and CBE[3:0], delayed 1 clock
    reg par_d, par_oe_d;
    wire par_now = ^{pci_ad, pci_cbe_n};
    always @(posedge pci_clk or negedge pci_rst_n) begin
        if (!pci_rst_n) begin
            par_d    <= 1'b0;
            par_oe_d <= 1'b0;
        end else begin
            par_d    <= par_now;
            par_oe_d <= ad_oe;
        end
    end
    assign pci_par = par_oe_d ? par_d : 1'bZ;

    // Error pins (not implemented in minimal core)
    assign pci_perr_n = 1'bZ;
    assign pci_serr_n = 1'bZ;

    // IRQ (open-drain)
    assign pci_inta_n = irq_active ? 1'b0 : 1'bZ;

    // ==========================================================
    // Configuration Space
    // ==========================================================
    reg [15:0] cfg_command;
    reg [15:0] cfg_status;
    reg [31:BAR0_BITS] bar0_hi;   // I/O BAR writable bits
    reg [31:BAR1_BITS] bar1_hi;   // Mem BAR writable bits
    reg [7:0]  cfg_int_line;
    reg [7:0]  cfg_lat_timer;
    reg [7:0]  cfg_cache_line;

    // Reconstruct full BAR values
    wire [31:0] bar0_val = {bar0_hi, {(BAR0_BITS-1){1'b0}}, 1'b1};  // bit 0 = I/O
    wire [31:0] bar1_val = {bar1_hi, {BAR1_BITS{1'b0}}};             // bit 0 = mem

    // BAR base addresses for decode
    wire [31:0] bar0_base = {bar0_hi, {BAR0_BITS{1'b0}}};
    wire [31:0] bar1_base = {bar1_hi, {BAR1_BITS{1'b0}}};

    wire io_space_en  = cfg_command[0];
    wire mem_space_en = cfg_command[1];
    wire bus_mst_en   = cfg_command[2];

    // Config register offset from latched address
    reg [31:0] addr_lat;
    wire [5:0] cfg_dword_addr = addr_lat[7:2];

    // Config space read mux
    reg [31:0] cfg_rdata;
    always @(*) begin
        cfg_rdata = 32'h0;
        case (cfg_dword_addr)
            6'h00: cfg_rdata = {DEVICE_ID, VENDOR_ID};
            6'h01: cfg_rdata = {cfg_status, cfg_command};
            6'h02: cfg_rdata = {CLASS_CODE, REVISION_ID};
            6'h03: cfg_rdata = {8'h00, 8'h00, cfg_lat_timer, cfg_cache_line};
            6'h04: cfg_rdata = bar0_val;
            6'h05: cfg_rdata = bar1_val;
            6'h0B: cfg_rdata = {SUBSYS_DID, SUBSYS_VID};
            6'h0F: cfg_rdata = {8'h00, 8'h00, 8'h01, cfg_int_line}; // INT pin A
            default: begin
                // Offsets 0x40+ (dword 0x10+): vendor config space
                if (cfg_dword_addr >= 6'h10)
                    cfg_rdata = vcfg_rdata;
                else
                    cfg_rdata = 32'h0;
            end
        endcase
    end

    // ==========================================================
    // Vendor Config Pass-Through
    // ==========================================================
    assign vcfg_offset = addr_lat[7:0];
    assign vcfg_wdata  = tgt_wdata;
    assign vcfg_wbe    = tgt_be;

    // ==========================================================
    // Address Decode
    // ==========================================================
    reg [3:0]  cmd_lat;
    reg        idsel_lat;

    // BAR hit detection (combinational, uses latched address)
    wire bar0_hit = io_space_en  &&
                    (addr_lat[31:BAR0_BITS] == bar0_base[31:BAR0_BITS]) &&
                    (cmd_lat == CMD_IO_RD || cmd_lat == CMD_IO_WR);

    wire bar1_hit = mem_space_en &&
                    (addr_lat[31:BAR1_BITS] == bar1_base[31:BAR1_BITS]) &&
                    (cmd_lat == CMD_MEM_RD || cmd_lat == CMD_MEM_WR);

    // Config cycle: Type 0, IDSEL asserted, function 0
    wire cfg_hit = idsel_lat &&
                   (addr_lat[1:0] == 2'b00) &&      // Type 0
                   (addr_lat[10:8] == 3'b000) &&     // Function 0
                   (cmd_lat == CMD_CFG_RD || cmd_lat == CMD_CFG_WR);

    wire any_hit = bar0_hit | bar1_hit | cfg_hit;

    wire cmd_is_read  = (cmd_lat == CMD_IO_RD)  || (cmd_lat == CMD_MEM_RD) ||
                        (cmd_lat == CMD_CFG_RD);
    wire cmd_is_write = (cmd_lat == CMD_IO_WR)  || (cmd_lat == CMD_MEM_WR) ||
                        (cmd_lat == CMD_CFG_WR);

    // ==========================================================
    // FRAME# Falling Edge Detector
    // ==========================================================
    reg frame_n_prev;
    always @(posedge pci_clk or negedge pci_rst_n) begin
        if (!pci_rst_n) frame_n_prev <= 1'b1;
        else            frame_n_prev <= frame_n_in;
    end
    wire frame_fall = frame_n_prev & ~frame_n_in;

    // ==========================================================
    // Target FSM
    // ==========================================================
    localparam [2:0]
        T_IDLE   = 3'd0,
        T_DECODE = 3'd1,
        T_TURN   = 3'd2,   // turnaround clock before read data
        T_RDATA  = 3'd3,   // driving read data, waiting IRDY#
        T_WDATA  = 3'd4,   // receiving write data
        T_DONE   = 3'd5;

    reg [2:0] tgt_state;
    reg       cfg_write_pending;
    reg       vcfg_write_pending;

    always @(posedge pci_clk or negedge pci_rst_n) begin
        if (!pci_rst_n) begin
            tgt_state     <= T_IDLE;
            devsel_n_r    <= 1'b1;
            trdy_n_r      <= 1'b1;
            stop_n_r      <= 1'b1;
            ad_oe         <= 1'b0;
            ad_out        <= 32'h0;
            addr_lat      <= 32'h0;
            cmd_lat       <= 4'h0;
            idsel_lat     <= 1'b0;
            tgt_start     <= 1'b0;
            tgt_addr      <= 32'h0;
            tgt_be        <= 4'h0;
            tgt_wdata     <= 32'h0;
            tgt_io_hit    <= 1'b0;
            tgt_mem_hit   <= 1'b0;
            tgt_is_read   <= 1'b0;
            tgt_is_write  <= 1'b0;
            cfg_write_pending  <= 1'b0;
            vcfg_write_pending <= 1'b0;
            vcfg_wr       <= 1'b0;
            // Config defaults
            cfg_command   <= 16'h0000;
            cfg_status    <= 16'h0200; // DEVSEL medium timing
            bar0_hi       <= {(32-BAR0_BITS){1'b0}};
            bar1_hi       <= {(32-BAR1_BITS){1'b0}};
            cfg_int_line  <= 8'hFF;
            cfg_lat_timer <= 8'h00;
            cfg_cache_line<= 8'h00;
        end else begin
            // Clear single-cycle pulses
            tgt_start          <= 1'b0;
            cfg_write_pending  <= 1'b0;
            vcfg_write_pending <= 1'b0;
            vcfg_wr            <= 1'b0;

            case (tgt_state)

            // ----------------------------------------------------------
            T_IDLE: begin
                devsel_n_r <= 1'b1;
                trdy_n_r   <= 1'b1;
                stop_n_r   <= 1'b1;
                ad_oe      <= 1'b0;

                // Detect address phase on FRAME# falling edge only
                if (frame_fall) begin
                    addr_lat  <= ad_in;
                    cmd_lat   <= cbe_in;
                    idsel_lat <= pci_idsel;
                    tgt_state <= T_DECODE;
                end
            end

            // ----------------------------------------------------------
            T_DECODE: begin
                // 1 clock decode time (medium DEVSEL)
                if (any_hit) begin
                    devsel_n_r <= 1'b0;   // claim the cycle

                    // Expose to backend
                    tgt_addr     <= addr_lat;
                    tgt_io_hit   <= bar0_hit;
                    tgt_mem_hit  <= bar1_hit;
                    tgt_is_read  <= cmd_is_read;
                    tgt_is_write <= cmd_is_write;

                    if (cmd_is_read) begin
                        tgt_state <= T_TURN;
                        if (bar0_hit || bar1_hit)
                            tgt_start <= 1'b1;  // notify bridge
                    end else begin
                        tgt_state <= T_WDATA;
                    end
                end else begin
                    // Not for us
                    tgt_state <= T_IDLE;
                end
            end

            // ----------------------------------------------------------
            T_TURN: begin
                // Turnaround cycle: prepare to drive AD
                if (cfg_hit) begin
                    // Config reads are immediate
                    ad_out   <= cfg_rdata;
                    ad_oe    <= 1'b1;
                    trdy_n_r <= 1'b0;
                    tgt_state <= T_RDATA;
                end else begin
                    // BAR read: wait for backend to supply data
                    if (tgt_rdy) begin
                        ad_out   <= tgt_rdata;
                        ad_oe    <= 1'b1;
                        trdy_n_r <= 1'b0;
                        tgt_state <= T_RDATA;
                    end
                    // else stay in T_TURN, inserting wait states
                end
            end

            // ----------------------------------------------------------
            T_RDATA: begin
                // Read data on bus, DEVSEL# and TRDY# asserted
                // Transfer occurs when IRDY# is also asserted
                if (!irdy_n_in) begin
                    tgt_state <= T_DONE;
                end
            end

            // ----------------------------------------------------------
            T_WDATA: begin
                // Write: master drives AD with data, CBE with byte enables
                if (!irdy_n_in) begin
                    tgt_be    <= ~cbe_in;  // CBE# active-low → BE active-high
                    tgt_wdata <= ad_in;

                    if (cfg_hit) begin
                        if (cfg_dword_addr >= 6'h10) begin
                            // Vendor config write
                            vcfg_write_pending <= 1'b1;
                        end else begin
                            // Standard config write
                            cfg_write_pending <= 1'b1;
                        end
                        trdy_n_r  <= 1'b0;
                        tgt_state <= T_DONE;
                    end else if (tgt_rdy) begin
                        tgt_start <= 1'b1;  // notify bridge
                        trdy_n_r  <= 1'b0;
                        tgt_state <= T_DONE;
                    end
                    // else: hold TRDY# deasserted (wait state)
                end
            end

            // ----------------------------------------------------------
            T_DONE: begin
                devsel_n_r <= 1'b1;
                trdy_n_r   <= 1'b1;
                stop_n_r   <= 1'b1;
                ad_oe      <= 1'b0;

                // Process standard config writes (offsets 0x00-0x3F)
                if (cfg_write_pending) begin
                    case (cfg_dword_addr)
                        6'h01: begin // Command/Status
                            if (tgt_be[0]) cfg_command[7:0]  <= tgt_wdata[7:0]  & 8'h47;
                            if (tgt_be[1]) cfg_command[15:8] <= tgt_wdata[15:8] & 8'h01;
                            if (tgt_be[2]) cfg_status[7:0]   <= cfg_status[7:0]  & ~tgt_wdata[23:16];
                            if (tgt_be[3]) cfg_status[15:8]  <= cfg_status[15:8] & ~tgt_wdata[31:24];
                        end
                        6'h03: begin // Cache Line / Latency Timer
                            if (tgt_be[0]) cfg_cache_line <= tgt_wdata[7:0];
                            if (tgt_be[1]) cfg_lat_timer  <= tgt_wdata[15:8];
                        end
                        6'h04: begin // BAR0
                            if (tgt_be[1]) bar0_hi[15:BAR0_BITS] <= tgt_wdata[15:BAR0_BITS];
                            if (tgt_be[2]) bar0_hi[23:16]        <= tgt_wdata[23:16];
                            if (tgt_be[3]) bar0_hi[31:24]        <= tgt_wdata[31:24];
                        end
                        6'h05: begin // BAR1
                            if (tgt_be[1] && BAR1_BITS <= 16)
                                bar1_hi[15:BAR1_BITS] <= tgt_wdata[15:BAR1_BITS];
                            if (tgt_be[2]) bar1_hi[23:16] <= tgt_wdata[23:16];
                            if (tgt_be[3]) bar1_hi[31:24] <= tgt_wdata[31:24];
                        end
                        6'h0F: begin // Interrupt Line
                            if (tgt_be[0]) cfg_int_line <= tgt_wdata[7:0];
                        end
                    endcase
                end

                // Process vendor config writes (offsets 0x40+)
                if (vcfg_write_pending) begin
                    vcfg_wr <= 1'b1;
                end

                tgt_state <= T_IDLE;
            end

            default: tgt_state <= T_IDLE;

            endcase
        end
    end

    // ==========================================================
    // Master FSM
    // ==========================================================
    localparam [2:0]
        M_IDLE    = 3'd0,
        M_REQ     = 3'd1,   // REQ# asserted, waiting GNT#
        M_ADDR    = 3'd2,   // address phase
        M_DATA    = 3'd3,   // data phase (write)
        M_TURN_R  = 3'd4,   // read turnaround, waiting TRDY#
        M_DONE    = 3'd5;

    reg [2:0]  mst_state;
    reg [31:0] mst_addr_r;
    reg [31:0] mst_wdata_r;
    reg [3:0]  mst_be_r;
    reg        mst_rd_r, mst_wr_r, mst_mem_r;
    reg [3:0]  mst_timeout;

    assign mst_busy = (mst_state != M_IDLE);

    always @(posedge pci_clk or negedge pci_rst_n) begin
        if (!pci_rst_n) begin
            mst_state   <= M_IDLE;
            req_n_r     <= 1'b1;
            frame_oe    <= 1'b0;
            irdy_oe     <= 1'b0;
            cbe_oe      <= 1'b0;
            frame_n_out <= 1'b1;
            irdy_n_out  <= 1'b1;
            cbe_out     <= 4'hF;
            mst_done    <= 1'b0;
            mst_abort   <= 1'b0;
            mst_rdata   <= 32'h0;
            mst_timeout <= 4'h0;
            mst_addr_r  <= 32'h0;
            mst_wdata_r <= 32'h0;
            mst_be_r    <= 4'h0;
            mst_rd_r    <= 1'b0;
            mst_wr_r    <= 1'b0;
            mst_mem_r   <= 1'b0;
        end else begin
            mst_done  <= 1'b0;
            mst_abort <= 1'b0;

            case (mst_state)

            M_IDLE: begin
                req_n_r  <= 1'b1;
                frame_oe <= 1'b0;
                irdy_oe  <= 1'b0;
                cbe_oe   <= 1'b0;

                if (mst_start && bus_mst_en) begin
                    mst_addr_r  <= mst_addr;
                    mst_wdata_r <= mst_wdata;
                    mst_be_r    <= mst_be;
                    mst_rd_r    <= mst_is_read;
                    mst_wr_r    <= mst_is_write;
                    mst_mem_r   <= mst_is_mem;
                    req_n_r     <= 1'b0;  // assert REQ#
                    mst_state   <= M_REQ;
                end
            end

            M_REQ: begin
                // Wait for GNT# AND bus idle
                if (!pci_gnt_n && frame_n_in && irdy_n_in) begin
                    frame_oe    <= 1'b1;
                    frame_n_out <= 1'b0;   // assert FRAME#
                    ad_oe       <= 1'b1;
                    ad_out      <= mst_addr_r;
                    cbe_oe      <= 1'b1;
                    if (mst_rd_r)
                        cbe_out <= mst_mem_r ? CMD_MEM_RD : CMD_IO_RD;
                    else
                        cbe_out <= mst_mem_r ? CMD_MEM_WR : CMD_IO_WR;
                    mst_state   <= M_ADDR;
                end
            end

            M_ADDR: begin
                // Address phase done. Start data phase.
                frame_n_out <= 1'b1;   // deassert FRAME# (single DWORD)
                req_n_r     <= 1'b1;   // release REQ#
                irdy_oe     <= 1'b1;
                irdy_n_out  <= 1'b0;   // assert IRDY#
                cbe_out     <= ~mst_be_r; // byte enables (active-low)
                mst_timeout <= 4'h0;

                if (mst_wr_r) begin
                    ad_out    <= mst_wdata_r;
                    mst_state <= M_DATA;
                end else begin
                    ad_oe     <= 1'b0;
                    mst_state <= M_TURN_R;
                end
            end

            M_TURN_R: begin
                // Read: wait for target DEVSEL# + TRDY#
                if (!pci_devsel_n && !pci_trdy_n) begin
                    mst_rdata <= ad_in;
                    mst_state <= M_DONE;
                end else if (mst_timeout == 4'hF) begin
                    mst_abort <= 1'b1;
                    mst_state <= M_DONE;
                end else begin
                    mst_timeout <= mst_timeout + 4'h1;
                end
            end

            M_DATA: begin
                // Write: wait for target DEVSEL# + TRDY#
                if (!pci_devsel_n && !pci_trdy_n) begin
                    mst_state <= M_DONE;
                end else if (mst_timeout == 4'hF) begin
                    mst_abort <= 1'b1;
                    mst_state <= M_DONE;
                end else begin
                    mst_timeout <= mst_timeout + 4'h1;
                end
            end

            M_DONE: begin
                irdy_n_out <= 1'b1;
                irdy_oe    <= 1'b0;
                frame_oe   <= 1'b0;
                ad_oe      <= 1'b0;
                cbe_oe     <= 1'b0;
                mst_done   <= 1'b1;
                mst_state  <= M_IDLE;
            end

            default: mst_state <= M_IDLE;

            endcase
        end
    end

endmodule