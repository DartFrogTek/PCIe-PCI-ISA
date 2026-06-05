//============================================================================
// pci_isa_bridge.v — PCI-to-ISA Bridge with DDMA
//
// Architecture:
//   PCI Bus ←→ pci_core ←→ bridge logic ←→ isa_master ←→ ISA Bus
//                              ↕
//                        ddma_engine ←→ ISA DMA pins
//                              ↕
//                        pci_core master → system RAM
//
// PIO path: CPU I/O access → PCI target → CDC handshake → ISA master cycle
// DMA path: ISA DREQ → DDMA engine → PCI bus master to RAM ↔ ISA DACK+data
//
// ISA clock derived from PCI clock / 4 (~8.25 MHz).
// Bridge owns all ISA bus pins and muxes between PIO and DMA.
//============================================================================

module pci_isa_bridge #(
    parameter [15:0] VENDOR_ID   = 16'hDFFF,
    parameter [15:0] DEVICE_ID   = 16'h8888,
    parameter [7:0]  REVISION_ID = 8'h01,
    parameter [15:0] ISA_IO_BASE = 16'h0000   // ISA I/O base added to BAR0 offset
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

    // ==================== ISA Bus Pins ====================
    output wire        isa_clk_out,    // generated ~8.25 MHz
    output wire        isa_rst_out,    // active high

    inout  wire [19:0] isa_sa,         // system address
    inout  wire [15:0] isa_sd,         // system data
    inout  wire        isa_sbhe_n,     // byte high enable
    inout  wire        isa_ior_n,      // I/O read
    inout  wire        isa_iow_n,      // I/O write
    inout  wire        isa_memr_n,     // memory read
    inout  wire        isa_memw_n,     // memory write
    output wire        isa_aen,        // address enable (high during DMA)
    input  wire        isa_iochrdy,    // wait state from device

    // ISA DMA (directly between bridge and ISA devices)
    input  wire [7:0]  isa_dreq_in,    // DRQ from ISA devices (active high)
    output wire [7:0]  isa_dack_out_n, // DACK to ISA devices (active low)
    output wire        isa_tc_out,     // terminal count

    // ISA IRQ (directly from ISA cards)
    input  wire        isa_irq_in      // directly per-line as needed
);

    // ==========================================================
    // Clock Generation: PCI_CLK / 4 → ISA_CLK
    // ==========================================================
    reg [1:0] clk_div;
    always @(posedge pci_clk or negedge pci_rst_n) begin
        if (!pci_rst_n) clk_div <= 2'b00;
        else            clk_div <= clk_div + 2'b01;
    end
    wire isa_clk = clk_div[1];
    assign isa_clk_out = isa_clk;
    assign isa_rst_out = ~pci_rst_n;

    // ==========================================================
    // PCI Core Instance
    // ==========================================================
    wire [31:0] tgt_addr, tgt_wdata;
    wire [3:0]  tgt_be;
    reg  [31:0] tgt_rdata_r;
    wire        tgt_io_hit, tgt_mem_hit;
    wire        tgt_is_read, tgt_is_write;
    wire        tgt_start;
    reg         tgt_rdy_r;

    // PCI master signals (directly for DDMA engine)
    wire        pci_mst_busy, pci_mst_done, pci_mst_abort;
    wire [31:0] pci_mst_rdata;
    reg         pci_mst_start;
    reg  [31:0] pci_mst_addr, pci_mst_wdata;
    reg  [3:0]  pci_mst_be;
    reg         pci_mst_is_read, pci_mst_is_write, pci_mst_is_mem;

    // Vendor config interface
    wire [7:0]  vcfg_offset;
    wire [31:0] vcfg_wdata_w;
    wire [3:0]  vcfg_wbe;
    reg  [31:0] vcfg_rdata_r;
    wire        vcfg_wr;

    wire        irq_to_pci;

    pci_core #(
        .VENDOR_ID   (VENDOR_ID),
        .DEVICE_ID   (DEVICE_ID),
        .REVISION_ID (REVISION_ID),
        .BAR0_BITS   (8),
        .BAR1_BITS   (12)
    ) u_pci (
        .pci_clk      (pci_clk),
        .pci_rst_n    (pci_rst_n),
        .pci_ad       (pci_ad),
        .pci_cbe_n    (pci_cbe_n),
        .pci_frame_n  (pci_frame_n),
        .pci_irdy_n   (pci_irdy_n),
        .pci_trdy_n   (pci_trdy_n),
        .pci_stop_n   (pci_stop_n),
        .pci_devsel_n (pci_devsel_n),
        .pci_idsel    (pci_idsel),
        .pci_par      (pci_par),
        .pci_perr_n   (pci_perr_n),
        .pci_serr_n   (pci_serr_n),
        .pci_req_n    (pci_req_n),
        .pci_gnt_n    (pci_gnt_n),
        .pci_inta_n   (pci_inta_n),
        // Target
        .tgt_addr     (tgt_addr),
        .tgt_be       (tgt_be),
        .tgt_wdata    (tgt_wdata),
        .tgt_rdata    (tgt_rdata_r),
        .tgt_io_hit   (tgt_io_hit),
        .tgt_mem_hit  (tgt_mem_hit),
        .tgt_is_read  (tgt_is_read),
        .tgt_is_write (tgt_is_write),
        .tgt_start    (tgt_start),
        .tgt_rdy      (tgt_rdy_r),
        // Master
        .mst_start    (pci_mst_start),
        .mst_busy     (pci_mst_busy),
        .mst_addr     (pci_mst_addr),
        .mst_be       (pci_mst_be),
        .mst_wdata    (pci_mst_wdata),
        .mst_rdata    (pci_mst_rdata),
        .mst_is_read  (pci_mst_is_read),
        .mst_is_write (pci_mst_is_write),
        .mst_is_mem   (pci_mst_is_mem),
        .mst_done     (pci_mst_done),
        .mst_abort    (pci_mst_abort),
        // Vendor config
        .vcfg_offset  (vcfg_offset),
        .vcfg_wdata   (vcfg_wdata_w),
        .vcfg_wbe     (vcfg_wbe),
        .vcfg_rdata   (vcfg_rdata_r),
        .vcfg_wr      (vcfg_wr),
        // IRQ
        .irq_active   (irq_to_pci)
    );

    // ==========================================================
    // Vendor Config Registers (PCI offsets 0x40 - 0x5C)
    // Matches IT8888 layout for driver compatibility
    // ==========================================================
    reg [31:0] cfg_ch01;      // 0x40: ch0/ch1 DDMA bases
    reg [31:0] cfg_ch23;      // 0x44: ch2/ch3 DDMA bases
    reg [31:0] cfg_ch45;      // 0x48: ch4(cascade)/ch5 DDMA bases
    reg [31:0] cfg_ch67;      // 0x4C: ch6/ch7 DDMA bases
    reg [31:0] cfg_ctrl_50;   // 0x50: control register
    reg [31:0] cfg_ctrl_54;   // 0x54: control register
    reg [31:0] cfg_dma_base;  // 0x58: DMA buffer physical base address
    reg [31:0] cfg_dma_size;  // 0x5C: DMA buffer size

    wire [5:0] vcfg_dword = vcfg_offset[7:2];

    // Vendor config read mux
    always @(*) begin
        case (vcfg_dword)
            6'h10: vcfg_rdata_r = cfg_ch01;
            6'h11: vcfg_rdata_r = cfg_ch23;
            6'h12: vcfg_rdata_r = cfg_ch45;
            6'h13: vcfg_rdata_r = cfg_ch67;
            6'h14: vcfg_rdata_r = cfg_ctrl_50;
            6'h15: vcfg_rdata_r = cfg_ctrl_54;
            6'h16: vcfg_rdata_r = cfg_dma_base;
            6'h17: vcfg_rdata_r = cfg_dma_size;
            default: vcfg_rdata_r = 32'h0;
        endcase
    end

    // Vendor config write
    always @(posedge pci_clk or negedge pci_rst_n) begin
        if (!pci_rst_n) begin
            cfg_ch01     <= 32'h0;
            cfg_ch23     <= 32'h0;
            cfg_ch45     <= 32'h0;
            cfg_ch67     <= 32'h0;
            cfg_ctrl_50  <= 32'h0;
            cfg_ctrl_54  <= 32'h0;
            cfg_dma_base <= 32'h0;
            cfg_dma_size <= 32'h0;
        end else if (vcfg_wr) begin
            case (vcfg_dword)
                6'h10: begin
                    if (vcfg_wbe[0]) cfg_ch01[7:0]   <= vcfg_wdata_w[7:0];
                    if (vcfg_wbe[1]) cfg_ch01[15:8]  <= vcfg_wdata_w[15:8];
                    if (vcfg_wbe[2]) cfg_ch01[23:16] <= vcfg_wdata_w[23:16];
                    if (vcfg_wbe[3]) cfg_ch01[31:24] <= vcfg_wdata_w[31:24];
                end
                6'h11: begin
                    if (vcfg_wbe[0]) cfg_ch23[7:0]   <= vcfg_wdata_w[7:0];
                    if (vcfg_wbe[1]) cfg_ch23[15:8]  <= vcfg_wdata_w[15:8];
                    if (vcfg_wbe[2]) cfg_ch23[23:16] <= vcfg_wdata_w[23:16];
                    if (vcfg_wbe[3]) cfg_ch23[31:24] <= vcfg_wdata_w[31:24];
                end
                6'h12: begin
                    if (vcfg_wbe[0]) cfg_ch45[7:0]   <= vcfg_wdata_w[7:0];
                    if (vcfg_wbe[1]) cfg_ch45[15:8]  <= vcfg_wdata_w[15:8];
                    if (vcfg_wbe[2]) cfg_ch45[23:16] <= vcfg_wdata_w[23:16];
                    if (vcfg_wbe[3]) cfg_ch45[31:24] <= vcfg_wdata_w[31:24];
                end
                6'h13: begin
                    if (vcfg_wbe[0]) cfg_ch67[7:0]   <= vcfg_wdata_w[7:0];
                    if (vcfg_wbe[1]) cfg_ch67[15:8]  <= vcfg_wdata_w[15:8];
                    if (vcfg_wbe[2]) cfg_ch67[23:16] <= vcfg_wdata_w[23:16];
                    if (vcfg_wbe[3]) cfg_ch67[31:24] <= vcfg_wdata_w[31:24];
                end
                6'h14: begin
                    if (vcfg_wbe[0]) cfg_ctrl_50[7:0]   <= vcfg_wdata_w[7:0];
                    if (vcfg_wbe[1]) cfg_ctrl_50[15:8]  <= vcfg_wdata_w[15:8];
                    if (vcfg_wbe[2]) cfg_ctrl_50[23:16] <= vcfg_wdata_w[23:16];
                    if (vcfg_wbe[3]) cfg_ctrl_50[31:24] <= vcfg_wdata_w[31:24];
                end
                6'h15: begin
                    if (vcfg_wbe[0]) cfg_ctrl_54[7:0]   <= vcfg_wdata_w[7:0];
                    if (vcfg_wbe[1]) cfg_ctrl_54[15:8]  <= vcfg_wdata_w[15:8];
                    if (vcfg_wbe[2]) cfg_ctrl_54[23:16] <= vcfg_wdata_w[23:16];
                    if (vcfg_wbe[3]) cfg_ctrl_54[31:24] <= vcfg_wdata_w[31:24];
                end
                6'h16: begin
                    if (vcfg_wbe[0]) cfg_dma_base[7:0]   <= vcfg_wdata_w[7:0];
                    if (vcfg_wbe[1]) cfg_dma_base[15:8]  <= vcfg_wdata_w[15:8];
                    if (vcfg_wbe[2]) cfg_dma_base[23:16] <= vcfg_wdata_w[23:16];
                    if (vcfg_wbe[3]) cfg_dma_base[31:24] <= vcfg_wdata_w[31:24];
                end
                6'h17: begin
                    if (vcfg_wbe[0]) cfg_dma_size[7:0]   <= vcfg_wdata_w[7:0];
                    if (vcfg_wbe[1]) cfg_dma_size[15:8]  <= vcfg_wdata_w[15:8];
                    if (vcfg_wbe[2]) cfg_dma_size[23:16] <= vcfg_wdata_w[23:16];
                    if (vcfg_wbe[3]) cfg_dma_size[31:24] <= vcfg_wdata_w[31:24];
                end
            endcase
        end
    end

    // ==========================================================
    // DDMA Engine Instance
    // ==========================================================
    wire        ddma_reg_hit;
    wire [7:0]  ddma_reg_rdata;
    wire        ddma_dma_active;

    // DDMA ISA bus signals
    wire [7:0]  ddma_dack_n;
    wire        ddma_tc;
    wire [15:0] ddma_sd_out;
    wire        ddma_sd_oe;
    wire        ddma_ior_n;
    wire        ddma_iow_n;

    // DDMA PCI master signals
    wire        ddma_mst_start;
    wire [31:0] ddma_mst_addr;
    wire [3:0]  ddma_mst_be;
    wire [31:0] ddma_mst_wdata;
    wire        ddma_mst_is_read;
    wire        ddma_mst_is_write;

    // DDMA register access from PCI I/O path
    reg         ddma_reg_wr, ddma_reg_rd;
    reg  [15:0] ddma_reg_addr_r;
    reg  [7:0]  ddma_reg_wdata_r;

    ddma_engine u_ddma (
        .clk            (pci_clk),
        .rst_n          (pci_rst_n),
        // Window config
        .cfg_ch01       (cfg_ch01),
        .cfg_ch23       (cfg_ch23),
        .cfg_ch45       (cfg_ch45),
        .cfg_ch67       (cfg_ch67),
        // Register access
        .reg_addr       (ddma_reg_addr_r),
        .reg_wdata      (ddma_reg_wdata_r),
        .reg_rdata      (ddma_reg_rdata),
        .reg_wr         (ddma_reg_wr),
        .reg_rd         (ddma_reg_rd),
        .reg_hit        (ddma_reg_hit),
        // DMA buffer
        .dma_buf_base   (cfg_dma_base),
        .dma_buf_size   (cfg_dma_size),
        // PCI master
        .pci_mst_start  (ddma_mst_start),
        .pci_mst_busy   (pci_mst_busy),
        .pci_mst_addr   (ddma_mst_addr),
        .pci_mst_be     (ddma_mst_be),
        .pci_mst_wdata  (ddma_mst_wdata),
        .pci_mst_rdata  (pci_mst_rdata),
        .pci_mst_is_read(ddma_mst_is_read),
        .pci_mst_is_write(ddma_mst_is_write),
        .pci_mst_done   (pci_mst_done),
        .pci_mst_abort  (pci_mst_abort),
        // ISA DMA bus
        .isa_dreq       (isa_dreq_sync),
        .isa_dack_n     (ddma_dack_n),
        .isa_tc_out     (ddma_tc),
        .isa_sd_out     (ddma_sd_out),
        .isa_sd_in      (sd_pin_in),
        .isa_sd_oe      (ddma_sd_oe),
        .isa_ior_n_out  (ddma_ior_n),
        .isa_iow_n_out  (ddma_iow_n),
        .dma_active     (ddma_dma_active)
    );

    // DREQ synchronizer (ISA async → PCI domain)
    reg [7:0] dreq_meta, isa_dreq_sync;
    always @(posedge pci_clk or negedge pci_rst_n) begin
        if (!pci_rst_n) begin
            dreq_meta     <= 8'h0;
            isa_dreq_sync <= 8'h0;
        end else begin
            dreq_meta     <= isa_dreq_in;
            isa_dreq_sync <= dreq_meta;
        end
    end

    // PCI master driven exclusively by DDMA engine
    always @(*) begin
        pci_mst_start    = ddma_mst_start;
        pci_mst_addr     = ddma_mst_addr;
        pci_mst_be       = ddma_mst_be;
        pci_mst_wdata    = ddma_mst_wdata;
        pci_mst_is_read  = ddma_mst_is_read;
        pci_mst_is_write = ddma_mst_is_write;
        pci_mst_is_mem   = 1'b1;  // DMA always targets memory
    end

    // ==========================================================
    // ISA Master Instance (for PIO forwarding)
    // Used directly — not through isa_bus_top — so bridge
    // owns all tri-state and muxing.
    // ==========================================================
    wire [19:0] pio_sa_out;
    wire [15:0] pio_sd_out;
    wire        pio_sd_oe;
    wire [15:0] sd_pin_in;      // shared SD input
    wire        pio_sbhe_n_out;
    wire        pio_ior_n_out;
    wire        pio_iow_n_out;
    wire        pio_memr_n_out;
    wire        pio_memw_n_out;
    wire        pio_master_n;
    wire        pio_bus_master;  // ISA master owns bus
    wire        pio_mst_owned;
    wire        pio_mst_done;
    wire        pio_mst_tc_seen;
    wire [15:0] pio_mst_rdata;

    reg         pio_start;
    reg  [19:0] pio_addr;
    reg  [15:0] pio_wdata;
    reg         pio_do_ior, pio_do_iow;
    reg         pio_do_memr, pio_do_memw;
    reg         pio_wide;
    reg         pio_release;

    // ISA master — tie dack_n=0 so it immediately acquires on start
    isa_master u_isa_mst (
        .clk           (isa_clk),
        .rst           (isa_rst_out),
        .dreq          (),              // unused in bridge mode
        .dack_n        (1'b0),          // always granted
        .tc            (1'b0),          // no external TC for PIO
        .master_n      (pio_master_n),  // unused, we own the bus
        .bus_master    (pio_bus_master),
        .sa_out        (pio_sa_out),
        .sd_out        (pio_sd_out),
        .sd_oe         (pio_sd_oe),
        .sd_in         (sd_pin_in),
        .sbhe_n_out    (pio_sbhe_n_out),
        .ior_n_out     (pio_ior_n_out),
        .iow_n_out     (pio_iow_n_out),
        .memr_n_out    (pio_memr_n_out),
        .memw_n_out    (pio_memw_n_out),
        .iochrdy_in    (isa_iochrdy),
        .start         (pio_start),
        .owned         (pio_mst_owned),
        .addr          (pio_addr),
        .wdata         (pio_wdata),
        .rdata         (pio_mst_rdata),
        .do_ior        (pio_do_ior),
        .do_iow        (pio_do_iow),
        .do_memr       (pio_do_memr),
        .do_memw       (pio_do_memw),
        .wide          (pio_wide),
        .done          (pio_mst_done),
        .tc_seen       (pio_mst_tc_seen),
        .release       (pio_release)
    );

    // ==========================================================
    // ISA Bus Pin Muxing: PIO vs DMA
    //
    // PIO (isa_master) and DMA (ddma_engine) never active
    // simultaneously. dma_active selects which drives the pins.
    // ==========================================================
    wire pio_active = pio_bus_master & ~ddma_dma_active;

    // SD bus (bidirectional)
    wire [15:0] sd_mux_out = ddma_dma_active ? ddma_sd_out : pio_sd_out;
    wire        sd_mux_oe  = ddma_dma_active ? ddma_sd_oe  : (pio_active & pio_sd_oe);
    assign isa_sd  = sd_mux_oe ? sd_mux_out : 16'hZZZZ;
    assign sd_pin_in = isa_sd;

    // SA bus (only PIO drives address; DMA uses DACK, no address)
    assign isa_sa = (pio_active) ? pio_sa_out : 20'hZZZZZ;

    // IOR#
    wire ior_mux = ddma_dma_active ? ddma_ior_n :
                   pio_active      ? pio_ior_n_out : 1'b1;
    assign isa_ior_n = (ddma_dma_active | pio_active) ? ior_mux : 1'bZ;

    // IOW#
    wire iow_mux = ddma_dma_active ? ddma_iow_n :
                   pio_active      ? pio_iow_n_out : 1'b1;
    assign isa_iow_n = (ddma_dma_active | pio_active) ? iow_mux : 1'bZ;

    // MEMR#, MEMW# (PIO only, not used during DMA)
    assign isa_memr_n = pio_active ? pio_memr_n_out : 1'bZ;
    assign isa_memw_n = pio_active ? pio_memw_n_out : 1'bZ;

    // SBHE# (PIO only)
    assign isa_sbhe_n = pio_active ? pio_sbhe_n_out : 1'bZ;

    // AEN: high during DMA, low during PIO
    assign isa_aen = ddma_dma_active ? 1'b1 : 1'b0;

    // DACK (directly from DDMA engine)
    assign isa_dack_out_n = ddma_dack_n;

    // TC
    assign isa_tc_out = ddma_tc;

    // ==========================================================
    // PCI Target I/O Routing
    //
    // When a PCI I/O access hits BAR0, check DDMA windows first.
    // If DDMA claims it → handle as register access (immediate).
    // If not → forward to ISA master via CDC handshake.
    // ==========================================================

    // Probe DDMA decode with the BAR0 address
    // The DDMA engine checks its configured window bases
    wire [15:0] io_port_addr = ISA_IO_BASE + tgt_addr[7:0];

    // DDMA address probe (directly combinational, always active)
    always @(*) begin
        ddma_reg_addr_r = io_port_addr;
        ddma_reg_wdata_r = tgt_wdata[7:0];  // ISA registers are 8-bit
    end

    // Detect if this I/O access is a DDMA register
    wire is_ddma_access = tgt_io_hit & ddma_reg_hit;
    wire is_isa_access  = (tgt_io_hit & ~ddma_reg_hit) | tgt_mem_hit;

    // ==========================================================
    // DDMA Register Access Path (immediate, no ISA cycle needed)
    // ==========================================================
    localparam [1:0]
        DD_IDLE = 2'd0,
        DD_ACK  = 2'd1;

    reg [1:0] dd_state;

    always @(posedge pci_clk or negedge pci_rst_n) begin
        if (!pci_rst_n) begin
            dd_state   <= DD_IDLE;
            ddma_reg_wr <= 1'b0;
            ddma_reg_rd <= 1'b0;
        end else begin
            ddma_reg_wr <= 1'b0;
            ddma_reg_rd <= 1'b0;

            case (dd_state)
            DD_IDLE: begin
                if (tgt_start && is_ddma_access) begin
                    if (tgt_is_write) ddma_reg_wr <= 1'b1;
                    if (tgt_is_read)  ddma_reg_rd <= 1'b1;
                    dd_state <= DD_ACK;
                end
            end
            DD_ACK: begin
                dd_state <= DD_IDLE;
            end
            endcase
        end
    end

    // ==========================================================
    // PCI → ISA PIO Forwarding (CDC handshake)
    //
    // PCI domain sets request, ISA domain executes cycle,
    // ISA domain sets ack, PCI domain completes.
    // ==========================================================

    // --- PCI domain state ---
    localparam [1:0]
        P_IDLE = 2'd0,
        P_WAIT = 2'd1,
        P_DONE = 2'd2;

    reg [1:0]  brg_pci_state;
    reg        brg_req;
    reg [19:0] brg_isa_addr;
    reg [15:0] brg_isa_wdata;
    reg        brg_is_io, brg_is_read, brg_is_wide;
    reg [15:0] brg_isa_rdata;

    // --- ISA domain state ---
    localparam [2:0]
        I_IDLE     = 3'd0,
        I_ACQUIRE  = 3'd1,
        I_WAIT_OWN = 3'd2,
        I_CYCLE    = 3'd3,
        I_WAIT_DONE= 3'd4,
        I_COMPLETE = 3'd5;

    reg [2:0]  brg_isa_state;
    reg        brg_ack;
    reg [15:0] brg_rdata_isa;

    // --- CDC: PCI→ISA (req) ---
    reg brg_req_s1, brg_req_s2;
    always @(posedge isa_clk) begin
        if (isa_rst_out) begin brg_req_s1 <= 0; brg_req_s2 <= 0; end
        else begin brg_req_s1 <= brg_req; brg_req_s2 <= brg_req_s1; end
    end

    // --- CDC: ISA→PCI (ack) ---
    reg brg_ack_s1, brg_ack_s2;
    always @(posedge pci_clk or negedge pci_rst_n) begin
        if (!pci_rst_n) begin brg_ack_s1 <= 0; brg_ack_s2 <= 0; end
        else begin brg_ack_s1 <= brg_ack; brg_ack_s2 <= brg_ack_s1; end
    end

    // Byte lane: extract ISA data from PCI DWORD
    wire [1:0] byte_lane = tgt_be[0] ? 2'd0 :
                           tgt_be[1] ? 2'd1 :
                           tgt_be[2] ? 2'd2 : 2'd3;
    wire wide_access = (tgt_be[1:0] == 2'b11) || (tgt_be[3:2] == 2'b11);

    wire [15:0] pio_wr_data = byte_lane[1] ?
                    {tgt_wdata[31:24], tgt_wdata[23:16]} :
                    {tgt_wdata[15:8],  tgt_wdata[7:0]};

    // --- PCI domain FSM ---
    always @(posedge pci_clk or negedge pci_rst_n) begin
        if (!pci_rst_n) begin
            brg_pci_state <= P_IDLE;
            brg_req       <= 1'b0;
            brg_isa_rdata <= 16'h0;
        end else begin
            case (brg_pci_state)
            P_IDLE: begin
                brg_req <= 1'b0;
                if (tgt_start && is_isa_access) begin
                    brg_isa_addr  <= tgt_io_hit ?
                        (ISA_IO_BASE + tgt_addr[7:0]) :
                        tgt_addr[19:0];
                    brg_isa_wdata <= pio_wr_data;
                    brg_is_io     <= tgt_io_hit;
                    brg_is_read   <= tgt_is_read;
                    brg_is_wide   <= wide_access;
                    brg_req       <= 1'b1;
                    brg_pci_state <= P_WAIT;
                end
            end
            P_WAIT: begin
                if (brg_ack_s2) begin
                    brg_isa_rdata <= brg_rdata_isa;
                    brg_req       <= 1'b0;
                    brg_pci_state <= P_DONE;
                end
            end
            P_DONE: begin
                if (!brg_ack_s2)
                    brg_pci_state <= P_IDLE;
            end
            endcase
        end
    end

    // --- ISA domain FSM ---
    always @(posedge isa_clk) begin
        if (isa_rst_out) begin
            brg_isa_state <= I_IDLE;
            brg_ack       <= 1'b0;
            brg_rdata_isa <= 16'h0;
            pio_start     <= 1'b0;
            pio_do_ior    <= 1'b0;
            pio_do_iow    <= 1'b0;
            pio_do_memr   <= 1'b0;
            pio_do_memw   <= 1'b0;
            pio_release   <= 1'b0;
            pio_addr      <= 20'h0;
            pio_wdata     <= 16'h0;
            pio_wide      <= 1'b0;
        end else begin
            pio_start   <= 1'b0;
            pio_do_ior  <= 1'b0;
            pio_do_iow  <= 1'b0;
            pio_do_memr <= 1'b0;
            pio_do_memw <= 1'b0;
            pio_release <= 1'b0;

            case (brg_isa_state)
            I_IDLE: begin
                brg_ack <= 1'b0;
                if (brg_req_s2) begin
                    pio_start <= 1'b1;
                    pio_addr  <= brg_isa_addr;
                    pio_wdata <= brg_isa_wdata;
                    pio_wide  <= brg_is_wide;
                    brg_isa_state <= I_WAIT_OWN;
                end
            end
            I_WAIT_OWN: begin
                if (pio_mst_owned) begin
                    if (brg_is_io) begin
                        if (brg_is_read) pio_do_ior <= 1'b1;
                        else             pio_do_iow <= 1'b1;
                    end else begin
                        if (brg_is_read) pio_do_memr <= 1'b1;
                        else             pio_do_memw <= 1'b1;
                    end
                    brg_isa_state <= I_WAIT_DONE;
                end
            end
            I_WAIT_DONE: begin
                if (pio_mst_done) begin
                    brg_rdata_isa <= pio_mst_rdata;
                    pio_release   <= 1'b1;
                    brg_ack       <= 1'b1;
                    brg_isa_state <= I_COMPLETE;
                end
            end
            I_COMPLETE: begin
                if (!brg_req_s2) begin
                    brg_ack       <= 1'b0;
                    brg_isa_state <= I_IDLE;
                end
            end
            default: brg_isa_state <= I_IDLE;
            endcase
        end
    end

    // ==========================================================
    // PCI Target Ready + Read Data Mux
    //
    // Three sources can satisfy a PCI target access:
    //   1. DDMA register (immediate)
    //   2. ISA PIO forwarding (after CDC round-trip)
    //   3. Config space (handled inside pci_core)
    // ==========================================================
    always @(*) begin
        tgt_rdy_r   = 1'b0;
        tgt_rdata_r = 32'h0;

        // DDMA register access — ready immediately after 1-clock decode
        if (dd_state == DD_ACK) begin
            tgt_rdy_r   = 1'b1;
            tgt_rdata_r = {24'h0, ddma_reg_rdata};  // 8-bit register in low byte
        end

        // ISA PIO forwarding — ready when CDC round-trip completes
        if (brg_pci_state == P_DONE) begin
            tgt_rdy_r = 1'b1;
            // Place ISA read data into correct PCI byte lanes
            if (byte_lane[1])
                tgt_rdata_r = {brg_isa_rdata, 16'h0000};
            else
                tgt_rdata_r = {16'h0000, brg_isa_rdata};
        end
    end

    // ==========================================================
    // IRQ Forwarding: ISA → PCI INTA#
    // ==========================================================
    reg irq_s1, irq_s2;
    always @(posedge pci_clk or negedge pci_rst_n) begin
        if (!pci_rst_n) begin irq_s1 <= 0; irq_s2 <= 0; end
        else begin irq_s1 <= isa_irq_in; irq_s2 <= irq_s1; end
    end
    assign irq_to_pci = irq_s2;

endmodule