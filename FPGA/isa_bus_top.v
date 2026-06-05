//============================================================================
// isa_bus_top.v — Generic ISA Bus IP, pure Verilog, no vendor primitives
// Master + Slave, I/O + Memory, DMA, IRQ, 8/16-bit
//
// Directly drives FPGA I/O pins via inout ports.
// Synthesis infers IOBUFs on all major vendors (Xilinx, Intel, Lattice, Gowin).
// If your tool requires explicit IOBUF instantiation, wrap this module and
// replace the inout assigns — all internal signals are unidirectional.
//============================================================================

module isa_bus_top #(
    parameter [15:0] IO_BASE   = 16'h0220,  // I/O base address
    parameter [15:0] IO_MASK   = 16'hFFF0,  // I/O decode mask (1=compare, 0=ignore)
    parameter [19:0] MEM_BASE  = 20'hD0000, // Memory base address
    parameter [19:0] MEM_MASK  = 20'hFF000, // Memory decode mask
    parameter        IS_16BIT  = 1,          // 1 = 16-bit device, 0 = 8-bit
    parameter [2:0]  DMA_CHAN  = 3'd1        // DMA channel (0-3 = 8-bit, 5-7 = 16-bit)
)(
    // ========== ISA Bus Pins ==========
    input  wire        isa_clk,        // ~8.33 MHz ISA clock
    input  wire        isa_rst,        // Active HIGH reset (directly from ISA RESET)

    inout  wire [19:0] isa_sa,         // System Address
    input  wire [23:17]isa_la,         // Latchable Address (active during BALE)
    inout  wire [15:0] isa_sd,         // System Data
    input  wire        isa_bale,       // Address Latch Enable
    input  wire        isa_aen,        // Address Enable (1=DMA cycle, 0=CPU)
    inout  wire        isa_sbhe_n,     // System Byte High Enable

    inout  wire        isa_ior_n,      // I/O Read strobe
    inout  wire        isa_iow_n,      // I/O Write strobe
    inout  wire        isa_memr_n,     // Memory Read strobe
    inout  wire        isa_memw_n,     // Memory Write strobe

    output wire        isa_iochrdy,    // I/O Channel Ready (0=wait)
    output wire        isa_iocs16_n,   // 16-bit I/O decode
    output wire        isa_memcs16_n,  // 16-bit Memory decode

    output wire        isa_dreq,       // DMA Request
    input  wire        isa_dack_n,     // DMA Acknowledge (active low)
    input  wire        isa_tc,         // Terminal Count
    output wire        isa_irq,        // Interrupt Request
    output wire        isa_master_n,   // Bus Master (active low)

    // ========== Internal Slave Interface ==========
    output wire [19:0] slv_addr,       // Decoded address (directly reflects SA)
    output wire [23:0] slv_addr_full,  // Full 24-bit addr (with latched LA)
    output wire [15:0] slv_din,        // Write data from ISA host
    input  wire [15:0] slv_dout,       // Read data to ISA host
    output wire        slv_io_rd,      // I/O read active (level)
    output wire        slv_io_wr,      // I/O write active (level)
    output wire        slv_mem_rd,     // Memory read active (level)
    output wire        slv_mem_wr,     // Memory write active (level)
    output wire        slv_io_wr_done, // I/O write complete (1-clk pulse, latch data here)
    output wire        slv_mem_wr_done,// Mem write complete (1-clk pulse)
    output wire        slv_cs,         // Chip select (address decoded)
    output wire        slv_wide,       // 16-bit cycle active
    input  wire        slv_rdy,        // 1=data ready, 0=insert wait states

    // ========== Internal Master Interface ==========
    input  wire        mst_start,      // Pulse: request bus ownership
    output wire        mst_owned,      // Bus is ours (MASTER# asserted)
    input  wire [19:0] mst_addr,       // Address to drive
    input  wire [15:0] mst_wdata,      // Write data
    output wire [15:0] mst_rdata,      // Read data (valid on mst_done)
    input  wire        mst_do_ior,     // Request I/O read cycle
    input  wire        mst_do_iow,     // Request I/O write cycle
    input  wire        mst_do_memr,    // Request memory read cycle
    input  wire        mst_do_memw,    // Request memory write cycle
    input  wire        mst_wide,       // 16-bit master cycle
    output wire        mst_done,       // Cycle complete (pulse)
    output wire        mst_tc_seen,    // Terminal count received
    input  wire        mst_release,    // Release bus ownership

    // ========== IRQ Control ==========
    input  wire        irq_set,        // Pulse: assert IRQ
    input  wire        irq_clr         // Pulse: de-assert IRQ
);

    // ==================================================================
    // Bus ownership
    // ==================================================================
    wire bus_master;  // 1 = we own the bus, driving address/data/strobes

    // ==================================================================
    // Tri-state: Data Bus
    // ==================================================================
    wire [15:0] sd_from_slv;
    wire        sd_oe_slv;
    wire [15:0] sd_from_mst;
    wire        sd_oe_mst;

    wire [15:0] sd_out = bus_master ? sd_from_mst : sd_from_slv;
    wire        sd_oe  = bus_master ? sd_oe_mst   : sd_oe_slv;

    assign isa_sd = sd_oe ? sd_out : 16'hZZZZ;
    wire [15:0] sd_in = isa_sd;

    // ==================================================================
    // Tri-state: Address Bus (only driven as master)
    // ==================================================================
    wire [19:0] sa_from_mst;
    assign isa_sa = bus_master ? sa_from_mst : 20'hZZZZZ;
    wire [19:0] sa_in = isa_sa;

    // ==================================================================
    // Tri-state: Control Strobes (only driven as master)
    // ==================================================================
    wire ior_n_from_mst, iow_n_from_mst, memr_n_from_mst, memw_n_from_mst;
    assign isa_ior_n  = bus_master ? ior_n_from_mst  : 1'bZ;
    assign isa_iow_n  = bus_master ? iow_n_from_mst  : 1'bZ;
    assign isa_memr_n = bus_master ? memr_n_from_mst : 1'bZ;
    assign isa_memw_n = bus_master ? memw_n_from_mst : 1'bZ;

    wire ior_n_in  = isa_ior_n;
    wire iow_n_in  = isa_iow_n;
    wire memr_n_in = isa_memr_n;
    wire memw_n_in = isa_memw_n;

    // ==================================================================
    // Tri-state: SBHE# (only driven as master)
    // ==================================================================
    wire sbhe_n_from_mst;
    assign isa_sbhe_n = bus_master ? sbhe_n_from_mst : 1'bZ;
    wire sbhe_n_in = isa_sbhe_n;

    // ==================================================================
    // Address Latch (LA[23:17] latched on BALE falling edge)
    // ==================================================================
    reg [23:17] la_latched;
    reg bale_prev;
    always @(posedge isa_clk) begin
        bale_prev <= isa_bale;
        if (bale_prev && !isa_bale)  // falling edge of BALE
            la_latched <= isa_la;
    end

    // ==================================================================
    // Slave Controller
    // ==================================================================
    isa_slave #(
        .IO_BASE  (IO_BASE),
        .IO_MASK  (IO_MASK),
        .MEM_BASE (MEM_BASE),
        .MEM_MASK (MEM_MASK),
        .IS_16BIT (IS_16BIT)
    ) u_slave (
        .clk           (isa_clk),
        .rst           (isa_rst),
        .bus_master     (bus_master),
        // ISA signals (directly directly directly directly directly directly directly from pins, active-low)
        .sa            (sa_in),
        .la_latched    (la_latched),
        .sd_in         (sd_in),
        .sd_out        (sd_from_slv),
        .sd_oe         (sd_oe_slv),
        .aen           (isa_aen),
        .sbhe_n        (sbhe_n_in),
        .ior_n         (ior_n_in),
        .iow_n         (iow_n_in),
        .memr_n        (memr_n_in),
        .memw_n        (memw_n_in),
        .iochrdy       (isa_iochrdy),
        .iocs16_n      (isa_iocs16_n),
        .memcs16_n     (isa_memcs16_n),
        // Internal interface
        .addr          (slv_addr),
        .addr_full     (slv_addr_full),
        .din           (slv_din),
        .dout          (slv_dout),
        .io_rd         (slv_io_rd),
        .io_wr         (slv_io_wr),
        .mem_rd        (slv_mem_rd),
        .mem_wr        (slv_mem_wr),
        .io_wr_done    (slv_io_wr_done),
        .mem_wr_done   (slv_mem_wr_done),
        .cs            (slv_cs),
        .wide          (slv_wide),
        .rdy           (slv_rdy)
    );

    // ==================================================================
    // Master Controller
    // ==================================================================
    isa_master u_master (
        .clk           (isa_clk),
        .rst           (isa_rst),
        // Bus arbitration
        .dreq          (isa_dreq),
        .dack_n        (isa_dack_n),
        .tc            (isa_tc),
        .master_n      (isa_master_n),
        .bus_master    (bus_master),
        // Driven bus signals
        .sa_out        (sa_from_mst),
        .sd_out        (sd_from_mst),
        .sd_oe         (sd_oe_mst),
        .sd_in         (sd_in),
        .sbhe_n_out    (sbhe_n_from_mst),
        .ior_n_out     (ior_n_from_mst),
        .iow_n_out     (iow_n_from_mst),
        .memr_n_out    (memr_n_from_mst),
        .memw_n_out    (memw_n_from_mst),
        .iochrdy_in    (isa_iochrdy),
        // Internal interface
        .start         (mst_start),
        .owned         (mst_owned),
        .addr          (mst_addr),
        .wdata         (mst_wdata),
        .rdata         (mst_rdata),
        .do_ior        (mst_do_ior),
        .do_iow        (mst_do_iow),
        .do_memr       (mst_do_memr),
        .do_memw       (mst_do_memw),
        .wide          (mst_wide),
        .done          (mst_done),
        .tc_seen       (mst_tc_seen),
        .release       (mst_release)
    );

    // ==================================================================
    // IRQ Flip-Flop
    // ==================================================================
    reg irq_reg;
    always @(posedge isa_clk) begin
        if (isa_rst)
            irq_reg <= 1'b0;
        else if (irq_clr)
            irq_reg <= 1'b0;
        else if (irq_set)
            irq_reg <= 1'b1;
    end
    assign isa_irq = irq_reg;

endmodule