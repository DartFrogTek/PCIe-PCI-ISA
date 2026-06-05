//============================================================================
// isa_master.v — ISA Bus Master Controller
//
// Bus acquisition sequence:
//   1. User pulses 'start' → DREQ asserted
//   2. DMA controller grants → DACK# goes low
//   3. We assert MASTER# (active low) → now we own the bus
//   4. User requests cycles via do_ior/do_iow/do_memr/do_memw
//   5. FSM drives address, data, and strobes with ISA timing
//   6. 'done' pulses when cycle completes, rdata valid for reads
//   7. User pulses 'release' → MASTER# and DREQ released
//
// Timing at ~8.33 MHz (120ns per clock):
//   Clock 0: Drive address on SA, setup SBHE#
//   Clock 1: Assert strobe (IOR#/IOW#/etc), drive write data
//   Clock 2: Hold strobe, check IOCHRDY for wait states
//   Clock 3: Deassert strobe, capture read data → done
//============================================================================

module isa_master (
    input  wire        clk,
    input  wire        rst,

    // DMA arbitration
    output reg         dreq,
    input  wire        dack_n,
    input  wire        tc,
    output reg         master_n,     // active low
    output wire        bus_master,   // 1 = we own the bus

    // Driven bus signals (directly to tri-state layer)
    output reg  [19:0] sa_out,
    output reg  [15:0] sd_out,
    output reg         sd_oe,
    input  wire [15:0] sd_in,
    output reg         sbhe_n_out,
    output reg         ior_n_out,
    output reg         iow_n_out,
    output reg         memr_n_out,
    output reg         memw_n_out,
    input  wire        iochrdy_in,   // target can insert wait states

    // Internal interface
    input  wire        start,        // pulse to begin bus acquisition
    output wire        owned,        // bus is ours
    input  wire [19:0] addr,
    input  wire [15:0] wdata,
    output reg  [15:0] rdata,
    input  wire        do_ior,       // pulse: start I/O read cycle
    input  wire        do_iow,       // pulse: start I/O write cycle
    input  wire        do_memr,      // pulse: start memory read cycle
    input  wire        do_memw,      // pulse: start memory write cycle
    input  wire        wide,         // 16-bit cycle
    output reg         done,         // cycle complete pulse
    output reg         tc_seen,      // TC received during ownership
    input  wire        release       // pulse: release bus
);

    // ==================================================================
    // FSM States
    // ==================================================================
    localparam [3:0]
        S_IDLE       = 4'd0,
        S_REQUESTING = 4'd1,  // DREQ asserted, waiting for DACK#
        S_ACQUIRED   = 4'd2,  // DACK# received, MASTER# asserted, idle
        S_ADDR_SETUP = 4'd3,  // Driving address, 1 clock setup
        S_STROBE_ON  = 4'd4,  // Strobe asserted
        S_STROBE_WAIT= 4'd5,  // Waiting for IOCHRDY if target needs time
        S_STROBE_OFF = 4'd6,  // Strobe deasserted, capture data
        S_RELEASING  = 4'd7;  // Releasing bus

    reg [3:0] state;

    // Latch the cycle type when a do_* request comes in
    reg r_ior, r_iow, r_memr, r_memw;
    reg r_wide;
    reg [19:0] r_addr;
    reg [15:0] r_wdata;
    reg [7:0]  wait_cnt;  // extra wait cycles for IOCHRDY

    assign bus_master = (state >= S_ACQUIRED && state <= S_STROBE_OFF);
    assign owned      = (state == S_ACQUIRED);

    always @(posedge clk) begin
        if (rst) begin
            state       <= S_IDLE;
            dreq        <= 1'b0;
            master_n    <= 1'b1;
            sa_out      <= 20'h00000;
            sd_out      <= 16'h0000;
            sd_oe       <= 1'b0;
            sbhe_n_out  <= 1'b1;
            ior_n_out   <= 1'b1;
            iow_n_out   <= 1'b1;
            memr_n_out  <= 1'b1;
            memw_n_out  <= 1'b1;
            rdata       <= 16'h0000;
            done        <= 1'b0;
            tc_seen     <= 1'b0;
            r_ior       <= 1'b0;
            r_iow       <= 1'b0;
            r_memr      <= 1'b0;
            r_memw      <= 1'b0;
            r_wide      <= 1'b0;
            r_addr      <= 20'h00000;
            r_wdata     <= 16'h0000;
            wait_cnt    <= 2'd0;
        end else begin
            // Default: clear single-cycle pulses
            done <= 1'b0;

            // TC monitoring (sticky while we own bus)
            if (state >= S_ACQUIRED && state <= S_STROBE_OFF) begin
                if (tc)
                    tc_seen <= 1'b1;
            end

            case (state)

            S_IDLE: begin
                dreq     <= 1'b0;
                master_n <= 1'b1;
                sd_oe    <= 1'b0;
                tc_seen  <= 1'b0;
                if (start) begin
                    dreq  <= 1'b1;
                    state <= S_REQUESTING;
                end
            end

            S_REQUESTING: begin
                if (!dack_n) begin
                    // Bus granted
                    master_n <= 1'b0;  // assert MASTER#
                    state    <= S_ACQUIRED;
                end
            end

            S_ACQUIRED: begin
                // Idle with bus ownership — wait for cycle request or release
                sd_oe      <= 1'b0;
                ior_n_out  <= 1'b1;
                iow_n_out  <= 1'b1;
                memr_n_out <= 1'b1;
                memw_n_out <= 1'b1;

                if (release) begin
                    state <= S_RELEASING;
                end else if (do_ior | do_iow | do_memr | do_memw) begin
                    // Latch cycle parameters
                    r_ior   <= do_ior;
                    r_iow   <= do_iow;
                    r_memr  <= do_memr;
                    r_memw  <= do_memw;
                    r_wide  <= wide;
                    r_addr  <= addr;
                    r_wdata <= wdata;
                    state   <= S_ADDR_SETUP;
                end
            end

            S_ADDR_SETUP: begin
                // Drive address and SBHE# — 1 clock setup time
                sa_out     <= r_addr;
                sbhe_n_out <= r_wide ? 1'b0 : 1'b1;

                // For writes, drive data now (setup before strobe)
                if (r_iow | r_memw) begin
                    sd_out <= r_wdata;
                    sd_oe  <= 1'b1;
                end else begin
                    sd_oe  <= 1'b0;
                end

                state <= S_STROBE_ON;
            end

            S_STROBE_ON: begin
                // Assert the appropriate strobe
                ior_n_out  <= r_ior  ? 1'b0 : 1'b1;
                iow_n_out  <= r_iow ? 1'b0 : 1'b1;
                memr_n_out <= r_memr ? 1'b0 : 1'b1;
                memw_n_out <= r_memw ? 1'b0 : 1'b1;
                wait_cnt   <= 2'd0;
                state      <= S_STROBE_WAIT;
            end

            S_STROBE_WAIT: begin
                // Hold strobe — check IOCHRDY
                // IOCHRDY=1 (or hi-Z, read as 1) means target is ready
                // IOCHRDY=0 means insert wait state
                // Also enforce minimum 1-clock strobe width via wait_cnt
                if (wait_cnt < 2'd1) begin
                    wait_cnt <= wait_cnt + 2'd1;
                end else if (iochrdy_in) begin
                    state <= S_STROBE_OFF;
                end else begin
                    // Still waiting — IOCHRDY held low by target
                    // Timeout safety: if we've waited 255+ clocks, bail
                    if (wait_cnt == 8'hFF) begin
                        state <= S_STROBE_OFF; // give up, avoid hang
                    end else begin
                        wait_cnt <= wait_cnt + 2'd1;
                    end
                end
            end

            S_STROBE_OFF: begin
                // Deassert all strobes
                ior_n_out  <= 1'b1;
                iow_n_out  <= 1'b1;
                memr_n_out <= 1'b1;
                memw_n_out <= 1'b1;

                // Capture read data
                if (r_ior | r_memr) begin
                    rdata <= sd_in;
                end

                sd_oe <= 1'b0;
                done  <= 1'b1;

                // Clear cycle type
                r_ior  <= 1'b0;
                r_iow  <= 1'b0;
                r_memr <= 1'b0;
                r_memw <= 1'b0;

                state <= S_ACQUIRED; // ready for next cycle or release
            end

            S_RELEASING: begin
                master_n   <= 1'b1;  // release MASTER#
                dreq       <= 1'b0;  // release DREQ
                sd_oe      <= 1'b0;
                ior_n_out  <= 1'b1;
                iow_n_out  <= 1'b1;
                memr_n_out <= 1'b1;
                memw_n_out <= 1'b1;
                state      <= S_IDLE;
            end

            default: state <= S_IDLE;

            endcase
        end
    end

endmodule