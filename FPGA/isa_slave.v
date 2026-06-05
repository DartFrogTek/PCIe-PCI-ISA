//============================================================================
// isa_slave.v — ISA Bus Slave Controller
//
// Combinational address decode drives IOCS16#/MEMCS16# early in the cycle.
// Read data (dout) is driven onto the bus whenever a read strobe is active
// and address matches. Write data is captured on the trailing (rising) edge
// of the write strobe.
//
// Internal interface signals:
//   io_rd, io_wr, mem_rd, mem_wr  — ACTIVE-HIGH LEVELS mirroring bus state
//   io_wr_done, mem_wr_done       — 1-clock PULSES on write completion
//   cs                            — combinational, address decoded
//   addr                          — directly reflects SA during the cycle
//   din                           — bus data, valid during write strobes
//   dout                          — you drive this; directly onto SD during reads
//   rdy                           — tie high if no wait states needed
//============================================================================

module isa_slave #(
    parameter [15:0] IO_BASE   = 16'h0220,
    parameter [15:0] IO_MASK   = 16'hFFF0,
    parameter [19:0] MEM_BASE  = 20'hD0000,
    parameter [19:0] MEM_MASK  = 20'hFF000,
    parameter        IS_16BIT  = 1
)(
    input  wire        clk,
    input  wire        rst,
    input  wire        bus_master,    // 1 = we own the bus, don't respond as slave

    // ISA bus signals (directly directly from tri-state layer)
    input  wire [19:0] sa,
    input  wire [23:17]la_latched,
    input  wire [15:0] sd_in,
    output reg  [15:0] sd_out,
    output reg         sd_oe,
    input  wire        aen,
    input  wire        sbhe_n,
    input  wire        ior_n,
    input  wire        iow_n,
    input  wire        memr_n,
    input  wire        memw_n,
    output wire        iochrdy,
    output wire        iocs16_n,
    output wire        memcs16_n,

    // Internal interface
    output wire [19:0] addr,
    output wire [23:0] addr_full,
    output wire [15:0] din,
    input  wire [15:0] dout,
    output wire        io_rd,
    output wire        io_wr,
    output wire        mem_rd,
    output wire        mem_wr,
    output wire        io_wr_done,
    output wire        mem_wr_done,
    output wire        cs,
    output wire        wide,
    input  wire        rdy
);

    // ==================================================================
    // Address Decode (combinational — must be fast for IOCS16#/MEMCS16#)
    // ==================================================================
    wire cpu_cycle = ~aen & ~bus_master;  // not a DMA cycle, not bus master

    wire io_hit  = cpu_cycle && ((sa[15:0] & IO_MASK)  == (IO_BASE  & IO_MASK));
    wire mem_hit = cpu_cycle && ((sa        & MEM_MASK) == (MEM_BASE & MEM_MASK));
    wire hit     = io_hit | mem_hit;

    // ==================================================================
    // 16-bit Decode (active-low, open-drain: low when claimed, hi-Z otherwise)
    // Must be combinational and early — host samples during address phase.
    // ==================================================================
    assign iocs16_n  = (IS_16BIT && io_hit)  ? 1'b0 : 1'bZ;
    assign memcs16_n = (IS_16BIT && mem_hit) ? 1'b0 : 1'bZ;

    // ==================================================================
    // Read/Write qualification (active-high levels)
    // ==================================================================
    wire reading_io  = io_hit  & ~ior_n;
    wire reading_mem = mem_hit & ~memr_n;
    wire writing_io  = io_hit  & ~iow_n;
    wire writing_mem = mem_hit & ~memw_n;
    wire reading     = reading_io | reading_mem;

    // ==================================================================
    // Edge detection for write completion pulses
    // ==================================================================
    reg iow_n_prev, memw_n_prev;
    always @(posedge clk) begin
        if (rst) begin
            iow_n_prev  <= 1'b1;
            memw_n_prev <= 1'b1;
        end else begin
            iow_n_prev  <= iow_n;
            memw_n_prev <= memw_n;
        end
    end

    // Rising edge of write strobe = write cycle complete, data is stable
    wire iow_rise  = ~iow_n_prev  & iow_n;
    wire memw_rise = ~memw_n_prev & memw_n;

    // Qualify with address decode (use registered hit for pulse timing)
    reg io_hit_r, mem_hit_r;
    always @(posedge clk) begin
        if (rst) begin
            io_hit_r  <= 1'b0;
            mem_hit_r <= 1'b0;
        end else begin
            io_hit_r  <= io_hit;
            mem_hit_r <= mem_hit;
        end
    end

    // ==================================================================
    // Data path
    // ==================================================================

    // Slave read: drive data onto bus when read strobe is active
    always @(*) begin
        sd_oe  = 1'b0;
        sd_out = 16'h0000;
        if (reading) begin
            sd_oe  = 1'b1;
            sd_out = dout;
        end
    end

    // ==================================================================
    // Wait-state generation
    // When we're being accessed and the user logic isn't ready,
    // pull IOCHRDY low to insert wait states.
    // When not being accessed, IOCHRDY is hi-Z (other devices may drive it).
    // ==================================================================
    assign iochrdy = hit ? rdy : 1'bZ;

    // ==================================================================
    // Internal interface outputs
    // ==================================================================
    assign addr      = sa;
    assign addr_full = {la_latched, sa[16:0]};
    assign din       = sd_in;
    assign cs        = hit;
    assign io_rd     = reading_io;
    assign io_wr     = writing_io;
    assign mem_rd    = reading_mem;
    assign mem_wr    = writing_mem;
    assign io_wr_done  = io_hit_r  & iow_rise;
    assign mem_wr_done = mem_hit_r & memw_rise;
    assign wide      = IS_16BIT[0] & ~sbhe_n;

endmodule