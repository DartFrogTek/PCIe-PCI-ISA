#!/usr/bin/env python

from chipsec.module_common import BaseModule, ModuleResult
import time
import os

# Traditional 8237A DMA controller registers
DMA1_BASE = 0x00  # First DMA controller (8-bit channels)
DMA2_BASE = 0xC0  # Second DMA controller (16-bit channels)

# Register offsets for each DMA controller
DMA_REG_STATUS = 0x08  # Status register (read)
DMA_REG_COMMAND = 0x08  # Command register (write)
DMA_REG_REQUEST = 0x09  # Request register (write)
DMA_REG_MASK_BIT = 0x0A  # Single channel mask register (write)
DMA_REG_MODE = 0x0B  # Mode register (write)
DMA_REG_CLEAR_FLIP_FLOP = 0x0C  # Clear flip-flop (write)
DMA_REG_MASTER_CLEAR = 0x0D  # Master clear (write)
DMA_REG_CLEAR_MASK = 0x0E  # Clear mask register (write)
DMA_REG_MASK_ALL = 0x0F  # Write all mask bits (write)

# Channel registers for specific channels
DMA1_CHAN0_ADDR = 0x00
DMA1_CHAN0_COUNT = 0x01
DMA1_CHAN1_ADDR = 0x02
DMA1_CHAN1_COUNT = 0x03
DMA1_CHAN2_ADDR = 0x04
DMA1_CHAN2_COUNT = 0x05
DMA1_CHAN3_ADDR = 0x06
DMA1_CHAN3_COUNT = 0x07

# DMA page registers
DMA1_PAGE_CHAN0 = 0x87
DMA1_PAGE_CHAN1 = 0x83
DMA1_PAGE_CHAN2 = 0x81
DMA1_PAGE_CHAN3 = 0x82

# DMA mode bits
DMA_MODE_DEMAND = 0x00
DMA_MODE_SINGLE = 0x40
DMA_MODE_BLOCK = 0x80
DMA_MODE_VERIFY = 0x00
DMA_MODE_WRITE = 0x04  # Write (memory to device)
DMA_MODE_READ = 0x08  # Read (device to memory)
DMA_MODE_AUTO = 0x10  # Autoinit

# LPC Controller registers (Z390 specific)
LPC_BUS = 0
LPC_DEV = 0x1F
LPC_FUN = 0

# H81 (8-series) PCH - Potential hidden DMA registers
# These might be still present but undocumented in Z390
H81_LPC_GEN_DMA_CTRL = 0xD0  # General DMA Control
H81_LPC_GEN_DMA_STAT = 0xD4  # General DMA Status
H81_LPC_GEN_DMA_TC = 0xD8  # General DMA Transfer Count
H81_LPC_GEN_DMA_ADDR = 0xDC  # General DMA Address
H81_LPC_GEN_DMA_DESC = 0xE0  # Additional DMA descriptor/control


class lpc_dma_h81_z390_test(BaseModule):
    def __init__(self):
        BaseModule.__init__(self)

    def is_supported(self):
        return True

    def check_lpc_controller(self):
        """
        Identify LPC controller and check if it's Z390
        """
        self.logger.log("[*] Identifying LPC controller...")

        try:
            # Get vendor/device ID of LPC controller
            lpc_vid = self.cs.pci.read_word(LPC_BUS, LPC_DEV, LPC_FUN, 0x00)
            lpc_did = self.cs.pci.read_word(LPC_BUS, LPC_DEV, LPC_FUN, 0x02)

            if lpc_vid == 0x8086:  # Intel
                self.logger.log_good(f"Found Intel LPC controller: VID=0x{lpc_vid:04X}, DID=0x{lpc_did:04X}")

                # Check if this is a Z390 chipset
                if lpc_did == 0xA305:  # Z390 LPC controller device ID
                    self.logger.log_good("Confirmed Z390 chipset")
                    return True
                else:
                    self.logger.log_warning(f"This does not appear to be a Z390 chipset (DID: 0x{lpc_did:04X})")
            else:
                self.logger.log_error(f"LPC controller not found at standard location (VID: 0x{lpc_vid:04X})")

        except Exception as e:
            self.logger.log_error(f"Error accessing LPC controller: {str(e)}")

        return False

    def test_traditional_dma_regs(self):
        """
        Test if traditional 8237A DMA registers respond
        """
        self.logger.log("[*] Testing traditional 8237A DMA registers...")

        dma_present = False

        try:
            # First test if we can read and write the DMA command register
            cmd_reg = self.cs.io.read_port_byte(DMA1_BASE + DMA_REG_COMMAND)
            self.logger.log(f"DMA1 Command Register initial value: 0x{cmd_reg:02X}")

            # Try to write and read back (with masked bits that are safe to modify)
            # Typically bit 2 (Controller Enable) is safe to toggle
            # test_val = (cmd_reg & 0xFB) | 0x04  # Toggle bit 2
            test_val = cmd_reg ^ 0x04  # XOR to toggle bit 2 regardless of its current value

            self.cs.io.write_port_byte(DMA1_BASE + DMA_REG_COMMAND, test_val)

            # Read back
            readback = self.cs.io.read_port_byte(DMA1_BASE + DMA_REG_COMMAND)
            self.logger.log(f"DMA1 Command Register after write: 0x{readback:02X}")

            # Restore original
            self.cs.io.write_port_byte(DMA1_BASE + DMA_REG_COMMAND, cmd_reg)

            if readback == test_val:
                self.logger.log_good("DMA1 Command Register responds to writes")
                dma_present = True
            else:
                self.logger.log_warning("DMA1 Command Register does not respond correctly")

            # Check DMA status register as well
            status = self.cs.io.read_port_byte(DMA1_BASE + DMA_REG_STATUS)
            self.logger.log(f"DMA1 Status Register: 0x{status:02X}")

        except Exception as e:
            self.logger.log_error(f"Error testing traditional DMA registers: {str(e)}")

        return dma_present

    def analyze_register_bits(self):
        """Analyze register bits to understand their potential meaning"""
        self.logger.log("[*] Analyzing register bit patterns...")

        # Read current values
        ctrl = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL)
        stat = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT)

        # Look for common bit patterns in DMA controllers
        # Check for potential enable bits (usually bit 0)
        self.logger.log(f"Control reg bit 0 (global enable?): {(ctrl & 0x1) != 0}")

        # Check for potential busy bits in status register (often bit 7)
        self.logger.log(f"Status reg bit 7 (busy bit?): {(stat & 0x80) != 0}")

        # Check for potential error bits in status (often upper bits)
        self.logger.log(f"Status reg bit 31 (error bit?): {(stat & 0x80000000) != 0}")

        # Check for channel select bits (often bits 8-10)
        channel = (ctrl >> 8) & 0x7
        self.logger.log(f"Potential channel select (bits 8-10): {channel}")

        # Analyze transfer count register
        tc = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC)
        self.logger.log(f"Transfer count (lower 16 bits): {tc & 0xFFFF}")

        # Analyze address register alignment
        addr = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR)
        self.logger.log(f"Address alignment: {addr & 0xF}")

    def test_h81_dma_registers(self):
        """
        Test for the presence of H81-style hidden DMA registers in Z390
        """
        self.logger.log("[*] Testing for H81-style DMA registers in Z390...")

        hidden_regs_found = False

        # List of potential DMA registers based on H81 (8-series) PCH
        h81_dma_regs = [
            (H81_LPC_GEN_DMA_CTRL, "General DMA Control"),
            (H81_LPC_GEN_DMA_STAT, "General DMA Status"),
            (H81_LPC_GEN_DMA_TC, "General DMA Transfer Count"),
            (H81_LPC_GEN_DMA_ADDR, "General DMA Address"),
            (H81_LPC_GEN_DMA_DESC, "DMA Descriptor Control")
        ]

        # Scan for H81 registers in the Z390
        for reg_offset, reg_name in h81_dma_regs:
            try:
                # Read register
                reg_val = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, reg_offset)

                # Skip if register returns all 1s or all 0s (likely non-existent)
                if reg_val != 0 and reg_val != 0xFFFFFFFF:
                    self.logger.log_warning(
                        f"Potential DMA register found: {reg_name} (0x{reg_offset:02X}) = 0x{reg_val:08X}")
                    hidden_regs_found = True

                    # Test if register is writable - only toggle the lowest bit to be safe
                    test_val = reg_val ^ 0x1
                    orig_val = reg_val

                    # Save original value
                    self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, reg_offset, test_val)

                    # Read back
                    readback = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, reg_offset)

                    # Restore original
                    self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, reg_offset, orig_val)

                    if readback == test_val:
                        self.logger.log_warning(
                            f"Register at 0x{reg_offset:02X} is WRITABLE - potential DMA control register!")
                    else:
                        self.logger.log(f"Register at 0x{reg_offset:02X} is read-only")
                else:
                    self.logger.log(f"Register at 0x{reg_offset:02X} returned 0x{reg_val:08X} - likely not used")

            except Exception as e:
                self.logger.log_error(f"Error testing register 0x{reg_offset:02X}: {str(e)}")

        return hidden_regs_found

    def try_h81_dma_activation(self):
        """
        Try to activate DMA using H81-style hidden registers
        """
        self.logger.log("[*] Attempting to activate DMA using H81-style registers...")

        try:
            # Read current values of all potential DMA registers
            ctrl_val = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL)
            stat_val = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT)
            tc_val = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC)
            addr_val = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR)

            # Log current values
            self.logger.log(f"H81 DMA Control (0x{H81_LPC_GEN_DMA_CTRL:02X}): 0x{ctrl_val:08X}")
            self.logger.log(f"H81 DMA Status (0x{H81_LPC_GEN_DMA_STAT:02X}): 0x{stat_val:08X}")
            self.logger.log(f"H81 DMA Transfer Count (0x{H81_LPC_GEN_DMA_TC:02X}): 0x{tc_val:08X}")
            self.logger.log(f"H81 DMA Address (0x{H81_LPC_GEN_DMA_ADDR:02X}): 0x{addr_val:08X}")

            # Try to activate by setting potential enable bits
            # Common enable bit patterns to try - bit 0 is often an enable bit
            # Be cautious and only try a few bit patterns to avoid system instability
            enable_patterns = [0x01, 0x80, 0x100]

            for pattern in enable_patterns:
                # Set potential enable bits
                test_ctrl = ctrl_val | pattern

                # Only proceed if we're changing the value
                if test_ctrl != ctrl_val:
                    # Original values to restore
                    orig_values = {
                        H81_LPC_GEN_DMA_CTRL: ctrl_val,
                        H81_LPC_GEN_DMA_STAT: stat_val,
                        H81_LPC_GEN_DMA_TC: tc_val,
                        H81_LPC_GEN_DMA_ADDR: addr_val
                    }

                    try:
                        # Apply test value
                        self.logger.log(f"Testing enable pattern 0x{pattern:X}...")
                        self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL, test_ctrl)

                        # Brief delay to allow potential activation
                        time.sleep(0.05)

                        # Read back status register to see if anything changed
                        status_after = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT)

                        if status_after != stat_val:
                            self.logger.log_warning(
                                f"Status register changed after setting pattern 0x{pattern:X}: 0x{status_after:08X}")

                        # Also check traditional DMA status register
                        isa_status_after = self.cs.io.read_port_byte(DMA1_BASE + DMA_REG_STATUS)
                        self.logger.log(f"Traditional DMA status after test: 0x{isa_status_after:02X}")

                    finally:
                        # Always restore original values
                        for reg, val in orig_values.items():
                            self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, reg, val)

        except Exception as e:
            self.logger.log_error(f"Error during H81 DMA activation test: {str(e)}")

        return False  # Return whether activation was successful

    def comprehensive_dma_test(self):
        """
        Perform comprehensive testing of discovered DMA functionality

        TODO: Causes Bad Pool Caller [Blue Screen of Death]
        Likely:
            phys_addr, virt_addr = self.cs.mem.alloc_physical_mem(buffer_size)
        FIX: None yet. Try Windows [AllocateContiguousMemory]
        """
        self.logger.log("[*] Performing comprehensive DMA tests...")

        # Define a safe memory buffer for DMA operations
        # For testing we'll use a small 4KB buffer
        buffer_size = 4096

        try:
            # Allocate physically contiguous memory buffer for DMA testing
            # The implementation depends on the OS and available APIs
            # This is a placeholder - would need to use proper allocation methods
            # like Windows AllocateContiguousMemory or Linux dma_alloc_coherent
            phys_addr, virt_addr = self.cs.mem.alloc_physical_mem(buffer_size)

            if not phys_addr:
                self.logger.log_error("Failed to allocate physical memory for DMA testing")
                return False

            self.logger.log(f"Allocated DMA test buffer: Physical: 0x{phys_addr:08X}, Virtual: 0x{virt_addr:08X}")

            # Fill buffer with test pattern
            test_pattern = 0xA5  # Alternating 10100101 pattern
            self.cs.mem.write_physical_mem(phys_addr, buffer_size, [test_pattern] * buffer_size)

            # Test various combinations of register settings

            # 1. Test simple memory-to-memory transfer
            self.logger.log("[*] Testing memory-to-memory DMA transfer...")

            # Setup source address
            self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR, phys_addr)

            # Setup transfer count (1KB)
            self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC, 1024)

            # Setup detailed bit patterns for control register
            # We'll try multiple combinations based on common DMA controller designs
            control_patterns = [
                # Format: [description, value]
                ["Basic enable", 0x00112233 | 0x00000001],  # Set bit 0
                ["Alternative enable", 0x00112233 | 0x80000000],  # Set highest bit
                ["Memory-to-memory mode", 0x00112233 | 0x00000003],  # Bits 0-1
                ["Read transfer", 0x00112233 | 0x00000101],  # Bit 0 + bit 8
                ["Write transfer", 0x00112233 | 0x00000201],  # Bit 0 + bit 9
                ["Channel 0 select", 0x00112233 | 0x00010001],  # Bit 0 + bit 16
                ["Channel 1 select", 0x00112233 | 0x00020001],  # Bit 0 + bit 17
            ]

            for desc, pattern in control_patterns:
                # Save original register values
                orig_ctrl = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL)
                orig_stat = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT)

                try:
                    self.logger.log(f"Testing pattern: {desc} (0x{pattern:08X})...")

                    # Apply test pattern
                    self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL, pattern)

                    # Wait for potential transfer completion
                    # This is a common pattern - the status register might have a "busy" or "done" bit
                    max_wait = 10
                    status_changed = False

                    for i in range(max_wait):
                        time.sleep(0.1)
                        curr_status = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT)

                        if curr_status != orig_stat:
                            self.logger.log_warning(f"Status changed from 0x{orig_stat:08X} to 0x{curr_status:08X}")
                            status_changed = True
                            break

                    # Check traditional DMA status as well
                    isa_status = self.cs.io.read_port_byte(DMA1_BASE + DMA_REG_STATUS)
                    self.logger.log(f"Traditional DMA status: 0x{isa_status:02X}")

                    # Check if memory content changed
                    # Read the first part of the buffer to see if anything happened
                    buffer_data = self.cs.mem.read_physical_mem(phys_addr, 16)
                    self.logger.log(f"Buffer data: {' '.join([f'{b:02X}' for b in buffer_data])}")

                    if status_changed:
                        self.logger.log_warning(f"Potential DMA activation with pattern {desc}!")

                        # If we got a status change, try a full transfer with destination
                        dest_addr = phys_addr + buffer_size // 2

                        # Setup addresses for a memory-to-memory transfer
                        self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR, phys_addr)

                        # Write to potential descriptor/additional control register
                        # This might contain the destination address
                        self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_DESC, dest_addr)

                        # Initiate transfer
                        self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL, pattern)

                        # Wait briefly
                        time.sleep(0.2)

                        # Check if data moved from source to destination
                        src_data = self.cs.mem.read_physical_mem(phys_addr, 16)
                        dst_data = self.cs.mem.read_physical_mem(dest_addr, 16)

                        self.logger.log(f"Source data: {' '.join([f'{b:02X}' for b in src_data])}")
                        self.logger.log(f"Dest data:   {' '.join([f'{b:02X}' for b in dst_data])}")

                        if src_data == dst_data and src_data[0] == test_pattern:
                            self.logger.log_warning("DMA TRANSFER SUCCESSFUL! Found working configuration.")
                            return True

                finally:
                    # Always restore original values
                    self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL, orig_ctrl)

            # 2. Test interaction with traditional DMA
            self.logger.log("[*] Testing interaction with traditional DMA...")

            # Try setting up traditional DMA channel and then activating H81 registers
            # First set up channel 1 for a memory write operation
            orig_mask = self.cs.io.read_port_byte(DMA1_BASE + DMA_REG_MASK_BIT)

            # Mask channel 1 (disable)
            self.cs.io.write_port_byte(DMA1_BASE + DMA_REG_MASK_BIT, 0x05)  # Bit 0=1 (mask), bits 1-2=01 (channel 1)

            # Clear byte pointer flip-flop
            self.cs.io.write_port_byte(DMA1_BASE + DMA_REG_CLEAR_FLIP_FLOP, 0)

            # Set address (low byte, high byte)
            addr_low = phys_addr & 0xFF
            addr_high = (phys_addr >> 8) & 0xFF
            self.cs.io.write_port_byte(DMA1_CHAN1_ADDR, addr_low)
            self.cs.io.write_port_byte(DMA1_CHAN1_ADDR, addr_high)

            # Clear flip-flop again
            self.cs.io.write_port_byte(DMA1_BASE + DMA_REG_CLEAR_FLIP_FLOP, 0)

            # Set count (low byte, high byte) - 64 bytes
            count_low = 63 & 0xFF  # Count is in bytes-1
            count_high = (63 >> 8) & 0xFF
            self.cs.io.write_port_byte(DMA1_CHAN1_COUNT, count_low)
            self.cs.io.write_port_byte(DMA1_CHAN1_COUNT, count_high)

            # Set page register
            page = (phys_addr >> 16) & 0xFF
            self.cs.io.write_port_byte(DMA1_PAGE_CHAN1, page)

            # Set mode for channel 1 (memory to I/O, single transfer)
            mode = 0x45  # 01 (channel 1), 01 (single), 01 (write)
            self.cs.io.write_port_byte(DMA1_BASE + DMA_REG_MODE, mode)

            # Unmask channel 1
            self.cs.io.write_port_byte(DMA1_BASE + DMA_REG_MASK_BIT, 0x01)  # Bit 0=0 (unmask), bits 1-2=01 (channel 1)

            # Now try activating H81 DMA with traditional DMA active
            # Use the pattern that showed most promise in the first test
            best_pattern = control_patterns[0][1]  # Default to first pattern

            self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR, phys_addr)
            self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC, 64)
            self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL, best_pattern)

            # Wait briefly
            time.sleep(0.1)

            # Check status changes
            h81_status = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT)
            isa_status = self.cs.io.read_port_byte(DMA1_BASE + DMA_REG_STATUS)

            self.logger.log(f"H81 DMA status: 0x{h81_status:08X}")
            self.logger.log(f"ISA DMA status: 0x{isa_status:02X}")

            # Restore traditional DMA mask
            self.cs.io.write_port_byte(DMA1_BASE + DMA_REG_MASK_BIT, orig_mask)

            # 3. Monitor register changes during system operations
            self.logger.log("[*] Setting up monitoring for DMA register changes...")

            # Record initial values
            initial_values = {
                "ctrl": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL),
                "stat": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT),
                "tc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC),
                "addr": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR),
                "desc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_DESC)
            }

            # Basic I/O operations to trigger potential DMA activity

            # 1. Read from disk (usually triggers DMA)
            self.logger.log("Performing disk I/O to potentially trigger DMA...")

            # Perform a disk read (this is Windows-specific)
            # To make this platform-independent, you'd need alternative approaches
            import subprocess
            subprocess.run(["powershell", "-command", "Get-Content -Path C:\\Windows\\System32\\drivers\\etc\\hosts"],
                           capture_output=True, text=True)

            # Monitor for changes after I/O
            current_values = {
                "ctrl": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL),
                "stat": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT),
                "tc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC),
                "addr": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR),
                "desc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_DESC)
            }

            # Check for changes
            changes_found = False
            for reg, value in current_values.items():
                if value != initial_values[reg]:
                    self.logger.log_warning(f"Register {reg} changed: 0x{initial_values[reg]:08X} -> 0x{value:08X}")
                    changes_found = True

            if changes_found:
                self.logger.log_warning("DMA register changes detected during system operation!")
            else:
                self.logger.log("No DMA register changes detected during disk I/O")

        except Exception as e:
            self.logger.log_error(f"Error during comprehensive DMA testing: {str(e)}")
            import traceback
            self.logger.log_error(traceback.format_exc())

        finally:
            # Free allocated memory buffer if it was successfully allocated
            if 'phys_addr' in locals() and phys_addr:
                self.cs.mem.free_physical_mem(virt_addr)

        return False

    def safer_dma_test(self):
        """
        A safer approach to test DMA functionality without risking system crashes
        """
        self.logger.log("[*] Performing safer DMA register testing...")

        # 1. Observation-only approach - don't try to perform actual DMA transfers
        self.logger.log("[*] Monitoring DMA registers for passive changes...")

        # Record initial values
        initial_values = {
            "ctrl": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL),
            "stat": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT),
            "tc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC),
            "addr": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR),
            "desc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_DESC)
        }

        self.logger.log("[*] Initial register values:")
        for reg, val in initial_values.items():
            self.logger.log(f"  {reg}: 0x{val:08X}")

        # 2. Test incremental changes to single bits rather than complex patterns
        self.logger.log("[*] Testing individual bits (safer approach)...")

        # Only modify one register (control) and only toggle one bit at a time
        # This is much less likely to cause system instability
        original_ctrl = initial_values["ctrl"]

        # Test bit positions 0 through 7 only (lower bits are typically control bits)
        for bit in range(8):
            bit_mask = 1 << bit
            test_value = original_ctrl ^ bit_mask  # Toggle just one bit

            try:
                # Save all register values before modifying
                before_values = {
                    "stat": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT),
                    "tc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC),
                    "addr": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR),
                    "desc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_DESC)
                }

                # Write the test value (only modifying one bit)
                self.logger.log(f"Testing bit {bit}: 0x{original_ctrl:08X} -> 0x{test_value:08X}")
                self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL, test_value)

                # Brief pause - much shorter than before
                time.sleep(0.01)

                # Check if other registers changed
                after_values = {
                    "stat": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT),
                    "tc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC),
                    "addr": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR),
                    "desc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_DESC)
                }

                # Look for any register changes
                for reg, before_val in before_values.items():
                    after_val = after_values[reg]
                    if before_val != after_val:
                        self.logger.log_warning(f"  {reg} changed: 0x{before_val:08X} -> 0x{after_val:08X}")

                # Check traditional DMA status register (non-destructive read)
                isa_status = self.cs.io.read_port_byte(DMA1_BASE + DMA_REG_STATUS)
                self.logger.log(f"  ISA DMA status: 0x{isa_status:02X}")

            finally:
                # ALWAYS restore the original value immediately
                self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL, original_ctrl)

        # 3. Look for register value correlations with traditional DMA activity
        self.logger.log("[*] Testing correlation with traditional DMA registers...")

        # Read initial values of all registers
        h81_before = {
            "ctrl": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL),
            "stat": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT),
            "tc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC),
            "addr": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR),
            "desc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_DESC)
        }

        # Only interact with traditional DMA registers in a non-destructive way
        # Just modify controller configuration, not actual transfers
        orig_command = self.cs.io.read_port_byte(DMA1_BASE + DMA_REG_COMMAND)

        try:
            # Toggle a bit in the DMA command register
            # Bit 2 is typically the controller enable bit
            test_cmd = orig_command ^ 0x04
            self.cs.io.write_port_byte(DMA1_BASE + DMA_REG_COMMAND, test_cmd)

            # Brief pause
            time.sleep(0.01)

            # Check if H81 registers changed in response
            h81_after = {
                "ctrl": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL),
                "stat": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT),
                "tc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC),
                "addr": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR),
                "desc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_DESC)
            }

            # Check for changes
            for reg, before_val in h81_before.items():
                after_val = h81_after[reg]
                if before_val != after_val:
                    self.logger.log_warning(
                        f"H81 {reg} responded to traditional DMA command change: 0x{before_val:08X} -> 0x{after_val:08X}")

        finally:
            # Restore original command
            self.cs.io.write_port_byte(DMA1_BASE + DMA_REG_COMMAND, orig_command)

        return True

    def safer_dma_test_two(self):
        """
        A safer approach to test potential DMA functionality without risking system stability
        """
        self.logger.log("[*] Performing safer DMA register analysis...")

        try:
            # 1. OBSERVATION PHASE - Monitor registers without modification

            # Record initial register values
            self.logger.log("Reading initial register values...")
            initial_values = {
                "ctrl": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL),
                "stat": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT),
                "tc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC),
                "addr": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR),
                "desc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_DESC)
            }

            for reg, val in initial_values.items():
                self.logger.log(f"  {reg}: 0x{val:08X}")

            # 2. BIT ANALYSIS - Analyze bit patterns without writing

            # Analyze control register
            ctrl = initial_values["ctrl"]
            self.logger.log("\nAnalyzing Control Register bit patterns:")

            # Common DMA control bit patterns
            potential_flags = [
                (0, "Enable/Start"),
                (1, "Direction (0=Read, 1=Write)"),
                (2, "Mode bit 0"),
                (3, "Mode bit 1"),
                (4, "Channel select bit 0"),
                (5, "Channel select bit 1"),
                (7, "Interrupt Enable"),
                (8, "Auto-initialize"),
                (16, "Burst mode"),
                (31, "Master Enable")
            ]

            for bit, desc in potential_flags:
                bit_state = (ctrl >> bit) & 1
                self.logger.log(f"  Bit {bit} ({desc}): {bit_state}")

            # Analyze status register
            stat = initial_values["stat"]
            self.logger.log("\nAnalyzing Status Register bit patterns:")

            potential_status = [
                (0, "Busy/Complete"),
                (1, "Transfer Complete"),
                (2, "Error"),
                (4, "Channel 0 status"),
                (5, "Channel 1 status"),
                (7, "Interrupt pending")
            ]

            for bit, desc in potential_status:
                bit_state = (stat >> bit) & 1
                self.logger.log(f"  Bit {bit} ({desc}): {bit_state}")

            # 3. CORRELATION TESTING - Trigger known DMA operations and observe

            self.logger.log("\n[*] Testing correlation with traditional DMA...")

            # Save original traditional DMA state
            dma1_cmd = self.cs.io.read_port_byte(DMA1_BASE + DMA_REG_COMMAND)
            dma1_mask = self.cs.io.read_port_byte(DMA1_BASE + DMA_REG_MASK_ALL)

            # First, disable all DMA channels (safest approach)
            self.cs.io.write_port_byte(DMA1_BASE + DMA_REG_MASK_ALL, 0x0F)  # Mask all channels

            # Now, perform operations on traditional DMA registers that won't
            # actually trigger transfers but might cause register changes

            self.logger.log("Setting traditional DMA channel 1 mode (no actual transfer)...")

            # Just change mode register for channel 1
            self.cs.io.write_port_byte(DMA1_BASE + DMA_REG_MODE, 0x45)  # Channel 1, single mode, write mode

            # Check for changes in hidden registers
            after_mode_values = {
                "ctrl": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL),
                "stat": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT),
                "tc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC),
                "addr": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR),
                "desc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_DESC)
            }

            # Check for any changes
            changes_found = False
            for reg, value in after_mode_values.items():
                if value != initial_values[reg]:
                    self.logger.log_warning(
                        f"Register {reg} changed after DMA mode set: 0x{initial_values[reg]:08X} -> 0x{value:08X}")
                    changes_found = True

            if not changes_found:
                self.logger.log("No register changes detected after DMA mode set")

            # 4. SAFE REGISTER MODIFICATION - Only modify one bit at a time

            self.logger.log("\n[*] Performing safe register modification tests...")

            # Instead of testing complex bit patterns, we'll just toggle individual bits
            # one at a time, focusing on non-destructive bits

            # List of potentially safe bits to toggle (avoid bits that might trigger operations)
            safe_bits = [8, 16, 24]  # Higher bits are often configuration flags rather than triggers

            for bit in safe_bits:
                # Save current value
                current = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL)

                # Toggle just this one bit
                test_val = current ^ (1 << bit)

                self.logger.log(f"Testing toggle of bit {bit} in control register: 0x{current:08X} -> 0x{test_val:08X}")

                # Write modified value
                self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL, test_val)

                # Brief delay
                time.sleep(0.01)

                # Read back and check for status changes
                readback = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL)
                status = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT)

                self.logger.log(f"  Readback: 0x{readback:08X}, Status: 0x{status:08X}")

                # Restore original value immediately
                self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL, current)

                # If the bit didn't change as expected or caused status changes, make note
                if readback != test_val:
                    self.logger.log_warning(f"  Bit {bit} could not be modified (read-only or sticky)")
                elif status != after_mode_values["stat"]:
                    self.logger.log_warning(f"  Bit {bit} modification caused status register change")

            # 5. REGISTER RELATIONSHIPS - Test if writing to one register affects others

            self.logger.log("\n[*] Testing register relationships...")

            # Test if writing to address register affects others
            original_addr = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR)
            test_addr = 0xAABBCCDD  # Non-zero value unlikely to be a valid memory address

            self.logger.log(f"Writing test pattern 0x{test_addr:08X} to address register...")
            self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR, test_addr)

            # Check all registers
            after_addr_write = {
                "ctrl": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL),
                "stat": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT),
                "tc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC),
                "addr": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR),
                "desc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_DESC)
            }

            # Check for any changes in other registers
            for reg, value in after_addr_write.items():
                if reg != "addr" and value != after_mode_values[reg]:
                    self.logger.log_warning(
                        f"Register {reg} changed after address write: 0x{after_mode_values[reg]:08X} -> 0x{value:08X}")

            # Check if address was written correctly
            if after_addr_write["addr"] != test_addr:
                self.logger.log_warning(
                    f"Address register didn't accept write: wrote 0x{test_addr:08X}, read 0x{after_addr_write['addr']:08X}")
            else:
                self.logger.log("Address register accepts writes")

            # Restore original address
            self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR, original_addr)

            # Similar test for transfer count register
            original_tc = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC)
            test_tc = 0x00001234  # Reasonable transfer count

            self.logger.log(f"Writing test count 0x{test_tc:08X} to transfer count register...")
            self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC, test_tc)

            # Read back
            readback_tc = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC)

            if readback_tc != test_tc:
                self.logger.log_warning(
                    f"Transfer count register behavior: wrote 0x{test_tc:08X}, read 0x{readback_tc:08X}")

                # Check if certain bits are masked
                masked_bits = test_tc ^ readback_tc
                self.logger.log(f"  Masked bits: 0x{masked_bits:08X}")
            else:
                self.logger.log("Transfer count register accepts writes normally")

            # Restore original TC
            self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC, original_tc)

            # 6. RESTORE STATE - Return everything to original state

            # Restore traditional DMA registers
            self.cs.io.write_port_byte(DMA1_BASE + DMA_REG_COMMAND, dma1_cmd)
            self.cs.io.write_port_byte(DMA1_BASE + DMA_REG_MASK_ALL, dma1_mask)

            # Restore hidden DMA registers
            for reg_name, offset in [
                ("ctrl", H81_LPC_GEN_DMA_CTRL),
                ("stat", H81_LPC_GEN_DMA_STAT),
                ("tc", H81_LPC_GEN_DMA_TC),
                ("addr", H81_LPC_GEN_DMA_ADDR),
                ("desc", H81_LPC_GEN_DMA_DESC)
            ]:
                self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, offset, initial_values[reg_name])

            # 7. CONCLUSIONS

            self.logger.log("\n[*] Analysis Conclusions:")

            # Attempt to interpret register purposes based on behavior
            self.logger.log("Likely register functions:")
            self.logger.log(f"  0x{H81_LPC_GEN_DMA_CTRL:02X}: Control register - contains mode/enable bits")
            self.logger.log(f"  0x{H81_LPC_GEN_DMA_STAT:02X}: Status register - may indicate transfer status")
            self.logger.log(f"  0x{H81_LPC_GEN_DMA_TC:02X}: Transfer count - specifies bytes to transfer")
            self.logger.log(f"  0x{H81_LPC_GEN_DMA_ADDR:02X}: DMA address - source or target memory location")
            self.logger.log(f"  0x{H81_LPC_GEN_DMA_DESC:02X}: Descriptor - may contain additional control bits")

            return True

        except Exception as e:
            self.logger.log_error(f"Error during safe DMA testing: {str(e)}")
            import traceback
            self.logger.log_error(traceback.format_exc())

        return False

    def monitor_dma_during_system_events(self):
        """
        Monitor DMA registers during normal system events that might trigger DMA
        """
        self.logger.log("[*] Setting up monitoring for DMA registers during system events...")

        # Record initial values
        initial_values = {
            "ctrl": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL),
            "stat": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT),
            "tc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC),
            "addr": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR),
            "desc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_DESC)
        }

        # Set up events to monitor (using safe system operations)
        events = [
            "Disk I/O",
            "Network activity",
            "Audio playback",
            "USB device access"
        ]

        # Monitor each type of event
        for event in events:
            self.logger.log(f"\nMonitoring during {event}...")

            # Trigger appropriate system activity
            if event == "Disk I/O":
                self.logger.log("Please open a file or folder in File Explorer...")
            elif event == "Network activity":
                self.logger.log("Please open a web page in your browser...")
            elif event == "Audio playback":
                self.logger.log("Please play an audio file...")
            elif event == "USB device access":
                self.logger.log("Please plug in or access a USB device...")

            # Wait for user to perform activity
            input("Press Enter after performing the activity...")

            # Check for register changes
            current_values = {
                "ctrl": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL),
                "stat": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT),
                "tc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC),
                "addr": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR),
                "desc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_DESC)
            }

            # Report any changes
            changes_found = False
            for reg, value in current_values.items():
                if value != initial_values[reg]:
                    self.logger.log_warning(
                        f"Register {reg} changed during {event}: 0x{initial_values[reg]:08X} -> 0x{value:08X}")
                    changes_found = True
                    # Update initial values for next comparison
                    initial_values[reg] = value

            if not changes_found:
                self.logger.log(f"No register changes detected during {event}")

        return True

    def poll_dma_registers(self, duration=30):
        """
        Poll DMA registers continuously for a period to detect any changes

        Args:
            duration: Duration to poll in seconds
        """
        self.logger.log(f"[*] Polling DMA registers for {duration} seconds...")

        # Record initial values
        initial_values = {
            "ctrl": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL),
            "stat": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT),
            "tc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC),
            "addr": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR),
            "desc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_DESC)
        }

        # Track all values seen
        value_history = {
            "ctrl": {initial_values["ctrl"]},
            "stat": {initial_values["stat"]},
            "tc": {initial_values["tc"]},
            "addr": {initial_values["addr"]},
            "desc": {initial_values["desc"]}
        }

        start_time = time.time()
        poll_count = 0
        changes_count = 0

        # Poll continuously
        try:
            while time.time() - start_time < duration:
                poll_count += 1

                # Sleep briefly to avoid excessive CPU usage
                time.sleep(0.1)

                # Read all registers
                current_values = {
                    "ctrl": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL),
                    "stat": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT),
                    "tc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC),
                    "addr": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR),
                    "desc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_DESC)
                }

                # Check for changes from the last poll
                for reg, value in current_values.items():
                    if value not in value_history[reg]:
                        changes_count += 1
                        self.logger.log(f"Register {reg} changed to: 0x{value:08X}")
                        # Add to history
                        value_history[reg].add(value)

                # Provide feedback periodically
                if poll_count % 100 == 0:
                    elapsed = time.time() - start_time
                    self.logger.log(f"Polling for {elapsed:.1f}s, {changes_count} changes detected")

        except KeyboardInterrupt:
            self.logger.log("Polling interrupted by user")

        # Final report
        self.logger.log("\n[*] Polling Results Summary:")
        self.logger.log(f"Total polls: {poll_count}")
        self.logger.log(f"Total register changes: {changes_count}")

        for reg, values in value_history.items():
            if len(values) > 1:
                self.logger.log(f"Register {reg} had {len(values)} different values:")
                for value in sorted(values):
                    self.logger.log(f"  0x{value:08X}")
            else:
                self.logger.log(f"Register {reg} remained constant at 0x{list(values)[0]:08X}")

        return True

    def run_old(self, module_argv):
        self.logger.log("##################################################")
        self.logger.log("# Z390 Undocumented DMA over LPC Test")
        self.logger.log("# Checks for H81-style hidden DMA in Z390")
        self.logger.log("##################################################")

        # Verify we're running on a Z390 chipset
        if not self.check_lpc_controller():
            self.logger.log_error("Not running on a Z390 chipset - test aborted")
            return ModuleResult.ERROR

        # Test if traditional 8237A DMA registers respond
        dma_present = self.test_traditional_dma_regs()

        # Test for presence of H81-style hidden DMA registers
        hidden_regs_found = self.test_h81_dma_registers()

        # If we found potential DMA registers, try to activate them
        if hidden_regs_found:
            self.try_h81_dma_activation()
            # causes bad pool caller
            # dma_functional = self.comprehensive_dma_test()
            self.safer_dma_test()
            self.safer_dma_test_two()

        # Final assessment
        self.logger.log("\n[*] Test Results Summary:")

        if dma_present:
            self.logger.log_warning("Traditional 8237A DMA registers appear to respond")
        else:
            self.logger.log("Traditional 8237A DMA registers do not respond")

        if hidden_regs_found:
            self.logger.log_warning("Found potential hidden H81-style DMA registers in Z390")
            return ModuleResult.WARNING
        else:
            self.logger.log_good("No hidden H81-style DMA registers detected in Z390")
            return ModuleResult.PASSED

    def monitor_dma_registers_long_term(self, log_file_path, interval=60):
        """
        Long-term monitoring of DMA registers to detect system usage

        Args:
            log_file_path: Path to write log file
            interval: Seconds between checks
        """
        try:
            with open(log_file_path, 'w') as f:
                f.write("Timestamp,ctrl,stat,tc,addr,desc\n")

                self.logger.log(f"Starting long-term monitoring, logging to {log_file_path}")
                self.logger.log(f"Press Ctrl+C to stop monitoring")

                while True:
                    # Read all registers
                    values = {
                        "ctrl": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL),
                        "stat": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT),
                        "tc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC),
                        "addr": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR),
                        "desc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_DESC)
                    }

                    # Write to log file
                    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
                    log_line = f"{timestamp},{values['ctrl']:08X},{values['stat']:08X},{values['tc']:08X},{values['addr']:08X},{values['desc']:08X}\n"
                    f.write(log_line)
                    f.flush()

                    time.sleep(interval)

        except KeyboardInterrupt:
            self.logger.log("Monitoring stopped by user")
        except Exception as e:
            self.logger.log_error(f"Error during monitoring: {str(e)}")

    def inspect_acpi_tables(self):
        """
        Inspect ACPI tables for potential DMA-related entries
        """
        self.logger.log("[*] Inspecting ACPI tables for DMA-related functionality...")

        try:
            # Look for DMA-related entries in DSDT/SSDT tables
            self.logger.log("[*] Searching DSDT for suspicious SystemMemory mappings...")

            # Get DSDT table if available through CHIPSEC
            if hasattr(self.cs, 'acpi') and hasattr(self.cs.acpi, 'get_DSDT'):
                dsdt = self.cs.acpi.get_DSDT()

                if dsdt:
                    # Search for OperationRegion declarations with SystemMemory type
                    # that might map to DMA controller address ranges
                    # This is a simplified approach - real implementation would need to parse ACPI ASL
                    memory_regions = []

                    # Look for patterns like: OperationRegion (XXXX, SystemMemory, 0xADDRESS, 0xSIZE)
                    # Particularly interested in addresses within PCI config space of LPC controller

                    # Convert DSDT to string for easier searching
                    dsdt_str = str(dsdt)

                    # Search for SystemMemory operations
                    import re
                    memory_regions = re.findall(
                        r"OperationRegion\s*\(\s*\w+\s*,\s*SystemMemory\s*,\s*(0x[0-9A-Fa-f]+)\s*,\s*(0x[0-9A-Fa-f]+)\s*\)",
                        dsdt_str)

                    for addr, size in memory_regions:
                        addr_val = int(addr, 16)
                        size_val = int(size, 16)

                        # Check if this region overlaps with known DMA register ranges
                        self.logger.log(f"Found SystemMemory mapping: {addr} size {size}")

                        # Look for suspicious ranges that might include our target registers
                        # For example, if it includes the LPC controller's configuration space
                        if (addr_val <= (
                                self.cs.pci.get_device_address(LPC_BUS, LPC_DEV, LPC_FUN) + H81_LPC_GEN_DMA_CTRL) and
                                (addr_val + size_val) >= (self.cs.pci.get_device_address(LPC_BUS, LPC_DEV,
                                                                                         LPC_FUN) + H81_LPC_GEN_DMA_DESC + 4)):
                            self.logger.log_warning(
                                f"Suspicious ACPI memory mapping includes potential DMA registers: {addr}-{hex(addr_val + size_val)}")
                else:
                    self.logger.log("Could not retrieve DSDT table")
            else:
                self.logger.log("ACPI table access not available in this CHIPSEC build")

        except Exception as e:
            self.logger.log_error(f"Error during ACPI table inspection: {str(e)}")

    def inspect_acpi_tables_safe(self):
        """
        Safely inspect ACPI tables for DMA-related information and save to files
        without printing excessive output to console
        """
        self.logger.log("[*] Safely inspecting ACPI tables...")

        try:
            import os

            # Create a directory for ACPI dumps if it doesn't exist
            acpi_dir = "acpi_dumps"
            if not os.path.exists(acpi_dir):
                os.makedirs(acpi_dir)

            self.logger.log(f"ACPI tables will be saved to {acpi_dir} directory")

            # Check if we have the ACPI module available
            if hasattr(self.cs, 'acpi'):
                # Try to get the list of available ACPI tables
                if hasattr(self.cs.acpi, 'get_ACPI_table_list'):
                    tables = self.cs.acpi.get_ACPI_table_list()

                    if tables:
                        # Just print the count and names of interest, not all tables
                        dma_related_tables = ['DSDT', 'SSDT', 'FACP', 'FACS']
                        found_tables = [t for t in tables if t in dma_related_tables]

                        self.logger.log(
                            f"Found {len(tables)} ACPI tables. Tables of interest: {', '.join(found_tables)}")

                        # Try to dump each table of interest
                        for table_name in found_tables:
                            if hasattr(self.cs.acpi, 'get_table_content'):
                                table_content = self.cs.acpi.get_table_content(table_name)
                                if table_content:
                                    # Save table to file without printing content
                                    file_path = os.path.join(acpi_dir, f"{table_name}.bin")
                                    with open(file_path, 'wb') as f:
                                        f.write(table_content)

                                    self.logger.log(
                                        f"Saved {table_name} table ({len(table_content)} bytes) to {file_path}")

                                    # For DSDT, analyze silently and save to file
                                    if table_name == 'DSDT':
                                        # Convert to string for basic analysis but don't print
                                        table_str = str(table_content)

                                        # Look for DMA-related keywords silently
                                        dma_keywords = ['DMA', 'SystemMemory', 'OperationRegion', 'LPC']
                                        keyword_counts = {}

                                        for keyword in dma_keywords:
                                            count = table_str.count(keyword)
                                            keyword_counts[keyword] = count

                                        # Just print a summary line, not each count
                                        self.logger.log(f"DSDT analysis: " +
                                                        ", ".join([f"'{k}': {v}" for k, v in keyword_counts.items()]))

                                        # Save analysis to file silently
                                        analysis_path = os.path.join(acpi_dir, "dsdt_dma_analysis.txt")
                                        with open(analysis_path, 'w') as f:
                                            f.write("DSDT DMA-Related Keyword Analysis:\n")
                                            f.write("================================\n\n")

                                            for keyword, count in keyword_counts.items():
                                                f.write(f"'{keyword}': {count} occurrences\n")

                                            f.write("\nDMA-Related Regions:\n")
                                            f.write("===================\n\n")

                                            # Extract SystemMemory regions
                                            import re
                                            memory_regions = re.findall(
                                                r"OperationRegion\s*\(\s*\w+\s*,\s*SystemMemory\s*,\s*(0x[0-9A-Fa-f]+)\s*,\s*(0x[0-9A-Fa-f]+)\s*\)",
                                                table_str)

                                            for i, (addr, size) in enumerate(memory_regions):
                                                f.write(f"Region {i + 1}: Address {addr}, Size {size}\n")

                                                # Check for LPC overlap quietly
                                                try:
                                                    addr_val = int(addr, 16)
                                                    size_val = int(size, 16)

                                                    lpc_addr = None
                                                    if hasattr(self.cs, 'pci') and hasattr(self.cs.pci,
                                                                                           'get_device_address'):
                                                        lpc_addr = self.cs.pci.get_device_address(LPC_BUS, LPC_DEV,
                                                                                                  LPC_FUN)

                                                    if lpc_addr:
                                                        if (addr_val <= lpc_addr + 0xFF) and (
                                                                addr_val + size_val >= lpc_addr):
                                                            f.write(
                                                                f"  WARNING: This region overlaps with LPC controller configuration space!\n")
                                                            f.write(
                                                                f"  LPC controller at {hex(lpc_addr)}, region covers {hex(addr_val)}-{hex(addr_val + size_val)}\n")

                                                            # Print this important warning to console
                                                            self.logger.log_warning(
                                                                f"Found ACPI region at {addr} that overlaps with LPC controller space")
                                                except Exception as e:
                                                    f.write(f"  Error analyzing address/size: {str(e)}\n")

                                        self.logger.log(f"Saved DSDT DMA analysis to {analysis_path}")
                    else:
                        self.logger.log("No ACPI tables found or accessible")
                else:
                    self.logger.log("ACPI table list function not available")

                    # Try alternative method but don't print table content
                    if hasattr(self.cs.acpi, 'get_DSDT'):
                        try:
                            dsdt = self.cs.acpi.get_DSDT()
                            if dsdt:
                                file_path = os.path.join(acpi_dir, "DSDT.bin")
                                with open(file_path, 'wb') as f:
                                    f.write(dsdt)
                                self.logger.log(f"Saved DSDT table using alternative method to {file_path}")
                        except Exception as e:
                            self.logger.log_error(f"Error getting DSDT: {str(e)}")
            else:
                self.logger.log("ACPI module not available in this CHIPSEC build")

                # Try platform-specific alternatives but suppress output
                import platform
                if platform.system() == 'Windows':
                    # Try to use Windows acpidump.exe if available
                    try:
                        import subprocess

                        # Instead of printing, redirect output to a file
                        self.logger.log("Trying Windows acpidump.exe utility...")

                        # Create temp file for output
                        acpi_out_file = os.path.join(acpi_dir, "acpidump_output.dat")

                        # Run acpidump with output redirection
                        with open(os.devnull, 'w') as devnull:
                            # Redirect both stdout and stderr to prevent console printing
                            subprocess.run(['acpidump.exe', '/o', acpi_out_file],
                                           stdout=devnull, stderr=devnull, check=True)

                        self.logger.log(f"Saved raw ACPI dump to {acpi_out_file}")

                        # Additional step: Check file size without printing content
                        if os.path.exists(acpi_out_file):
                            file_size = os.path.getsize(acpi_out_file)
                            self.logger.log(f"ACPI dump size: {file_size} bytes")
                    except Exception as e:
                        self.logger.log(f"acpidump.exe utility not available or failed: {str(e)}")

                elif platform.system() == 'Linux':
                    # Try to use Linux ACPI filesystem
                    acpi_path = '/sys/firmware/acpi/tables'
                    if os.path.exists(acpi_path):
                        # Just print a summary, not each file
                        table_count = len(os.listdir(acpi_path))
                        self.logger.log(f"Copying {table_count} ACPI tables from {acpi_path}...")

                        copied_count = 0
                        for table_name in os.listdir(acpi_path):
                            src_path = os.path.join(acpi_path, table_name)
                            dst_path = os.path.join(acpi_dir, table_name)

                            try:
                                with open(src_path, 'rb') as src, open(dst_path, 'wb') as dst:
                                    dst.write(src.read())
                                copied_count += 1
                            except Exception as e:
                                self.logger.log_error(f"Error copying {table_name}: {str(e)}")

                        # Just print summary
                        self.logger.log(f"Successfully copied {copied_count} of {table_count} ACPI tables")

            return True

        except Exception as e:
            self.logger.log_error(f"Error during safe ACPI inspection: {str(e)}")
            return False

    def analyze_acpi_dump(self, dump_path):
        """
        Analyze ACPI dump file created by inspect_acpi_tables_minimal
        """
        self.logger.log("[*] Analyzing ACPI dump for DMA-related entries...")

        try:
            import os
            import re

            if not os.path.exists(dump_path):
                self.logger.log_error(f"ACPI dump file {dump_path} not found")
                return False

            # Get file size
            file_size = os.path.getsize(dump_path)
            if file_size == 0:
                self.logger.log_error(f"ACPI dump file {dump_path} is empty")
                return False

            self.logger.log(f"Analyzing ACPI dump file: {dump_path} ({file_size} bytes)")

            # Read the dump file
            with open(dump_path, 'rb') as f:
                dump_data = f.read()

            # Look for DSDT table
            dsdt_offset = dump_data.find(b'DSDT')
            if dsdt_offset == -1:
                self.logger.log("No DSDT table found in ACPI dump")
                return False

            self.logger.log(f"Found DSDT table at offset {dsdt_offset}")

            # Convert to string for analysis
            # Only use a portion of the file to prevent memory issues with large dumps
            analysis_data = dump_data[max(0, dsdt_offset - 100):min(len(dump_data), dsdt_offset + 10000)]
            data_str = str(analysis_data)

            # Create directory for extracted tables if it doesn't exist
            extract_dir = os.path.join(os.path.dirname(dump_path), "extracted_tables")
            if not os.path.exists(extract_dir):
                os.makedirs(extract_dir)

            # Extract DSDT to a separate file
            dsdt_file = os.path.join(extract_dir, "DSDT.bin")
            with open(dsdt_file, 'wb') as f:
                # In a proper implementation, we would parse the table length
                # This is a simplified approach that just extracts a reasonable chunk
                f.write(analysis_data[:10000])

            self.logger.log(f"Extracted DSDT table to {dsdt_file}")

            # Look for SystemMemory operation regions
            memory_regions = re.findall(r"SystemMemory.*?(0x[0-9A-Fa-f]+).*?(0x[0-9A-Fa-f]+)", data_str)

            if not memory_regions:
                self.logger.log("No SystemMemory regions found in DSDT")
                return False

            self.logger.log(f"Found {len(memory_regions)} SystemMemory regions in DSDT")

            # Create analysis file
            analysis_file = os.path.join(extract_dir, "memory_regions_analysis.txt")
            with open(analysis_file, 'w') as f:
                f.write("ACPI SystemMemory Region Analysis\n")
                f.write("===============================\n\n")

                for i, (addr, size) in enumerate(memory_regions):
                    f.write(f"Region {i + 1}: Address {addr}, Size {size}\n")

                    try:
                        addr_val = int(addr, 16)
                        size_val = int(size, 16)

                        # Get LPC controller address
                        lpc_cfg_space = self.cs.pci.get_device_address(LPC_BUS, LPC_DEV, LPC_FUN)
                        if lpc_cfg_space:
                            # Check if region overlaps with LPC controller space
                            if (addr_val <= lpc_cfg_space + 0xFF) and (addr_val + size_val >= lpc_cfg_space):
                                f.write(f"  WARNING: This region overlaps with LPC controller configuration space!\n")
                                f.write(
                                    f"  LPC controller at {hex(lpc_cfg_space)}, region covers {hex(addr_val)}-{hex(addr_val + size_val)}\n")

                                # Log important finding to console
                                self.logger.log_warning(
                                    f"Found suspicious ACPI memory mapping at {addr} (size {size}) that includes LPC controller space")
                    except Exception as e:
                        f.write(f"  Error analyzing address/size: {str(e)}\n")

                # Check for other DMA-related keywords
                dma_keywords = ['DMA', 'LPC', 'DMAC', 'Legacy']
                f.write("\nOther DMA-related keywords in DSDT:\n")
                f.write("=================================\n\n")

                for keyword in dma_keywords:
                    count = data_str.count(keyword)
                    f.write(f"'{keyword}': {count} occurrences\n")

                    if count > 0:
                        self.logger.log(f"Found {count} occurrences of '{keyword}' in DSDT")

            self.logger.log(f"Saved memory regions analysis to {analysis_file}")
            return True

        except Exception as e:
            self.logger.log_error(f"Error analyzing ACPI dump: {str(e)}")
            return False

    def inspect_acpi_tables_minimal(self):
        """
        Minimal ACPI table inspection that focuses only on saving files
        without console flooding
        """
        self.logger.log("[*] Performing minimal ACPI table inspection...")

        try:
            import os
            import subprocess

            # Create directory for ACPI dumps
            acpi_dir = "acpi_dumps"
            if not os.path.exists(acpi_dir):
                os.makedirs(acpi_dir)

            self.logger.log(f"Created directory: {acpi_dir} for ACPI dumps")

            # Simple approach: Just run acpidump with output redirection
            dump_file = os.path.join(acpi_dir, "acpi_dump.dat")

            # Determine OS and use appropriate method
            import platform
            if platform.system() == 'Windows':
                try:
                    # Run acpidump.exe with complete output redirection
                    self.logger.log("Running acpidump.exe to save ACPI tables...")

                    # Use subprocess with file redirection instead of console
                    with open(dump_file, 'wb') as outfile:
                        proc = subprocess.Popen(['acpidump.exe'],
                                                stdout=outfile,
                                                stderr=subprocess.PIPE,
                                                shell=True)
                        _, stderr = proc.communicate()

                    if os.path.exists(dump_file):
                        file_size = os.path.getsize(dump_file)
                        if file_size > 0:
                            self.logger.log(f"Successfully saved ACPI dump ({file_size} bytes) to {dump_file}")
                        else:
                            self.logger.log_error("ACPI dump file was created but is empty")
                    else:
                        self.logger.log_error("Failed to create ACPI dump file")
                except Exception as e:
                    self.logger.log_error(f"Error running acpidump.exe: {str(e)}")

            elif platform.system() == 'Linux':
                try:
                    # Linux approach - use acpidump command or read from /sys
                    self.logger.log("Attempting to dump ACPI tables on Linux...")

                    # Try acpidump if available
                    try:
                        with open(dump_file, 'wb') as outfile:
                            subprocess.run(['acpidump'], stdout=outfile, stderr=subprocess.PIPE)

                        if os.path.exists(dump_file) and os.path.getsize(dump_file) > 0:
                            self.logger.log(f"Successfully saved ACPI dump to {dump_file}")
                        else:
                            raise Exception("Empty or missing output file")

                    except Exception:
                        # Fallback to copying from /sys/firmware/acpi/tables
                        acpi_path = '/sys/firmware/acpi/tables'
                        if os.path.exists(acpi_path):
                            self.logger.log(f"Copying ACPI tables from {acpi_path}...")

                            copied = 0
                            for table in os.listdir(acpi_path):
                                src = os.path.join(acpi_path, table)
                                dst = os.path.join(acpi_dir, table)

                                try:
                                    with open(src, 'rb') as s, open(dst, 'wb') as d:
                                        d.write(s.read())
                                    copied += 1
                                except Exception as e:
                                    self.logger.log_error(f"Error copying {table}: {str(e)}")

                            self.logger.log(f"Copied {copied} ACPI tables to {acpi_dir}")
                        else:
                            self.logger.log_error("Could not access ACPI tables on this Linux system")

                except Exception as e:
                    self.logger.log_error(f"Error dumping ACPI tables on Linux: {str(e)}")

            # If we have a CHIPSEC ACPI module, try to use it in a minimal way
            if hasattr(self.cs, 'acpi'):
                try:
                    # Try to get DSDT specifically and save it
                    if hasattr(self.cs.acpi, 'get_DSDT'):
                        dsdt = self.cs.acpi.get_DSDT()
                        if dsdt:
                            dsdt_file = os.path.join(acpi_dir, "DSDT_chipsec.bin")
                            with open(dsdt_file, 'wb') as f:
                                f.write(dsdt)
                            self.logger.log(f"Saved DSDT from CHIPSEC to {dsdt_file}")
                except Exception as e:
                    self.logger.log_error(f"Error using CHIPSEC ACPI module: {str(e)}")

            return True

        except Exception as e:
            self.logger.log_error(f"Error in ACPI inspection: {str(e)}")
            return False

    def inspect_smi_handlers(self):
        """
        Look for potential SMI handlers that might be accessing DMA functionality
        """
        self.logger.log("[*] Inspecting SMI handlers...")

        try:
            # Check if SMI handler inspection is available
            if hasattr(self.cs, 'smi') and hasattr(self.cs.smi, 'get_SMI_handlers'):
                # Get SMI handlers
                handlers = self.cs.smi.get_SMI_handlers()

                if handlers:
                    self.logger.log(f"Found {len(handlers)} SMI handlers")

                    # Look for handlers that might access LPC controller registers
                    for handler_id, handler_info in enumerate(handlers):
                        # The actual handler inspection would depend on the specific
                        # CHIPSEC implementation and capabilities

                        self.logger.log(
                            f"SMI Handler {handler_id}: Base={handler_info.get('base', 'Unknown')}, Size={handler_info.get('size', 'Unknown')}")

                        # If we can get the actual handler code, we could scan for
                        # specific LPC register access patterns
                        if 'code' in handler_info:
                            handler_code = handler_info['code']

                            # Look for signatures that might indicate LPC DMA access
                            # This is very implementation-specific and would need to be
                            # customized based on actual analysis of known handlers

                            # Example: Look for byte sequences that might represent access
                            # to the LPC controller configuration space via port IO
                            # (This is a simplified example)
                            if b'\x89\xDF\x1F\x00' in handler_code:  # Just an example pattern
                                self.logger.log_warning(
                                    f"SMI Handler {handler_id} contains suspicious pattern that may access LPC controller")
                else:
                    self.logger.log("No SMI handlers found or accessible")
            else:
                self.logger.log("SMI handler inspection not available in this CHIPSEC build")

            # Additionally, check if there are suspicious SMI configurations in ACPI tables
            # This would involve looking for _GPE methods that trigger SMIs

            # Check for SMI ports being configured
            if hasattr(self.cs, 'io'):
                # Check common SMI trigger ports
                smi_port = 0xB2  # Common SMI trigger port
                smi_data = self.cs.io.read_port_byte(smi_port)
                self.logger.log(f"SMI port 0xB2 current value: 0x{smi_data:02X}")

                # Check if writing to SMI port affects our DMA registers
                # Save original register values
                original_values = {
                    "ctrl": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL),
                    "stat": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT)
                }

                # Try a few common SMI commands that might be related to DMA or platform config
                for test_cmd in [0x4C, 0x4D, 0x51, 0x52]:  # Common SMI command values to test
                    try:
                        self.logger.log(f"Testing SMI command 0x{test_cmd:02X}...")
                        self.cs.io.write_port_byte(smi_port, test_cmd)

                        # Brief pause to allow SMI to execute
                        time.sleep(0.1)

                        # Check if DMA registers changed
                        new_values = {
                            "ctrl": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL),
                            "stat": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT)
                        }

                        for reg, val in new_values.items():
                            if val != original_values[reg]:
                                self.logger.log_warning(
                                    f"SMI command 0x{test_cmd:02X} changed {reg} register: 0x{original_values[reg]:08X} -> 0x{val:08X}")

                    except Exception as e:
                        self.logger.log_error(f"Error testing SMI command 0x{test_cmd:02X}: {str(e)}")

        except Exception as e:
            self.logger.log_error(f"Error during SMI handler inspection: {str(e)}")

    def inspect_smi_handlers_direct(self):
        """
        Use direct memory scanning to look for potential SMI handlers

        TODO: Causes total system freeze.
        FIX: None yet.
        """
        self.logger.log("[*] Using direct memory scanning for SMI handlers...")

        try:
            # Get SMRAM base from chipset registers
            smram_base = None

            # Check common SMRAM control registers
            # Intel 8 series chipsets typically have this at PMC LPC offset B0h
            smram_ctrl = self.cs.pci.read_dword(0, 0x1F, 0, 0xB0)

            self.logger.log(f"SMRAM control register: 0x{smram_ctrl:08X}")

            # SMRAM base might be at bits 20:12 (older chipsets)
            # or configured elsewhere - this is chipset specific
            if smram_ctrl != 0xFFFFFFFF:
                if (smram_ctrl & 0x40) != 0:  # Check if D_LCK bit is set
                    self.logger.log("SMRAM appears to be locked (D_LCK bit set)")

                # Try to determine SMRAM base - this is very chipset specific
                # On older chipsets, SMRAM base is often:
                smram_base_raw = (smram_ctrl >> 12) & 0x1FF
                smram_base = smram_base_raw << 12

                if smram_base:
                    self.logger.log(f"Possible SMRAM base detected at: 0x{smram_base:08X}")

                    # We can't access actual SMI handlers if SMRAM is locked
                    # But we can scan for SMI trigger mechanisms

            # Check SMI_EN register in ACPI controller
            smi_en = self.cs.pci.read_dword(0, 0x1F, 0, 0x30)
            self.logger.log(f"SMI_EN register: 0x{smi_en:08X}")

            # Check which SMI sources are enabled
            if smi_en != 0xFFFFFFFF:
                smi_sources = []
                if (smi_en & 0x00000001) != 0: smi_sources.append("Global SMI Enable")
                if (smi_en & 0x00000002) != 0: smi_sources.append("SLP_SMI")
                if (smi_en & 0x00000004) != 0: smi_sources.append("APM")
                if (smi_en & 0x00000008) != 0: smi_sources.append("SWSMI_TMR")
                # Add other common sources based on chipset

                if smi_sources:
                    self.logger.log(f"Enabled SMI sources: {', '.join(smi_sources)}")

            # Scan I/O port 0xB2 (APM SMI) usage
            # Test a range of values to see if they trigger readable effects
            original_ctrl = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL)
            original_stat = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT)

            self.logger.log("Testing APM SMI commands to detect handlers that might configure DMA...")

            # Test a wider range of SMI commands
            for cmd in range(0x00, 0xFF, 0x10):  # Test every 16th value to cover range efficiently
                try:
                    self.logger.log(f"Testing SMI command 0x{cmd:02X}...")
                    self.cs.io.write_port_byte(0xB2, cmd)

                    # Give SMI handler time to execute
                    time.sleep(0.01)

                    # Check if our registers changed
                    new_ctrl = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL)
                    new_stat = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT)

                    if new_ctrl != original_ctrl or new_stat != original_stat:
                        self.logger.log_warning(f"SMI command 0x{cmd:02X} affected DMA registers!")
                        self.logger.log_warning(f"  CTRL: 0x{original_ctrl:08X} -> 0x{new_ctrl:08X}")
                        self.logger.log_warning(f"  STAT: 0x{original_stat:08X} -> 0x{new_stat:08X}")

                        # Now test neighboring values to narrow down the specific command
                        for subcmd in range(cmd, cmd + 16):
                            if subcmd != cmd:  # Skip the one we already tested
                                try:
                                    self.cs.io.write_port_byte(0xB2, subcmd)
                                    time.sleep(0.01)

                                    sub_ctrl = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL)
                                    sub_stat = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT)

                                    if sub_ctrl != original_ctrl or sub_stat != original_stat:
                                        self.logger.log_warning(
                                            f"Additional SMI command 0x{subcmd:02X} affects DMA registers")
                                except:
                                    pass
                except:
                    self.logger.log(f"Error testing SMI command 0x{cmd:02X}")

            # Scan firmware volumes for SMI handler signatures
            if hasattr(self.cs, 'uefi') and hasattr(self.cs.uefi, 'get_EFI_System_Table'):
                try:
                    self.logger.log("Scanning firmware volumes for SMI-related modules...")
                    # This part is very platform-specific and would need adaptation
                    # to the specific CHIPSEC version and capabilities
                except:
                    self.logger.log("UEFI scanning not available")

        except Exception as e:
            self.logger.log_error(f"Error during direct SMI handler inspection: {str(e)}")

    def inspect_smi_handlers_safe(self):
        """
        Safer inspection of SMI-related registers without triggering actual SMI execution
        """
        self.logger.log("[*] Safely inspecting SMI-related configuration (read-only)...")

        try:
            # Only read SMI configuration registers, don't write to port 0xB2

            # Check SMRAM control register
            smram_ctrl = self.cs.pci.read_dword(0, 0x1F, 0, 0xB0)
            if smram_ctrl != 0xFFFFFFFF:
                self.logger.log(f"SMRAM control register: 0x{smram_ctrl:08X}")

                # Analyze without modifying
                if (smram_ctrl & 0x40) != 0:
                    self.logger.log("SMRAM is locked (D_LCK bit set)")
                if (smram_ctrl & 0x10) != 0:
                    self.logger.log("SMRAM is closed (D_CLS bit set)")
                if (smram_ctrl & 0x08) != 0:
                    self.logger.log("SMRAM is enabled (D_EN bit set)")

            # Check SMI_EN register in ACPI controller (read-only)
            smi_en = self.cs.pci.read_dword(0, 0x1F, 0, 0x30)
            if smi_en != 0xFFFFFFFF:
                self.logger.log(f"SMI_EN register: 0x{smi_en:08X}")

                # Decode enabled SMI sources
                smi_sources = []
                if (smi_en & 0x00000001) != 0: smi_sources.append("Global SMI Enable")
                if (smi_en & 0x00000002) != 0: smi_sources.append("SLP_SMI")
                if (smi_en & 0x00000004) != 0: smi_sources.append("APM")
                if (smi_en & 0x00000008) != 0: smi_sources.append("SWSMI_TMR")

                if smi_sources:
                    self.logger.log(f"Enabled SMI sources: {', '.join(smi_sources)}")

            # Check SMI_STS register (SMI status)
            smi_sts = self.cs.pci.read_dword(0, 0x1F, 0, 0x34)
            if smi_sts != 0xFFFFFFFF:
                self.logger.log(f"SMI_STS register: 0x{smi_sts:08X}")

            # Check APM port status (without triggering SMI)
            apm_status = self.cs.io.read_port_byte(0xB3)  # B3 is often status, B2 is command
            self.logger.log(f"APM status port (0xB3): 0x{apm_status:02X}")

            # Look for ACPI-related firmware SMI handlers in our target registers
            # Read the H81-style DMA registers to see if they appear to be related to SMI
            # This is read-only and shouldn't trigger any system instability

            dma_regs = {
                "ctrl": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_CTRL),
                "stat": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_STAT),
                "tc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_TC),
                "addr": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_ADDR),
                "desc": self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, H81_LPC_GEN_DMA_DESC)
            }

            # Look for bit patterns that might indicate SMI functionality
            # For example, if bits match known SMI status/control bits

            # Example: If LPC DMA control register has similar bit layout to SMI_EN
            if (dma_regs["ctrl"] & 0x00000001) == (smi_en & 0x00000001):
                self.logger.log_warning(f"DMA control register bit 0 matches SMI_EN Global Enable bit")

            # Example: If status register has similar pending bits to SMI_STS
            if (dma_regs["stat"] & 0x00000001) == (smi_sts & 0x00000001):
                self.logger.log_warning(f"DMA status register bit 0 matches SMI_STS bit 0")

            self.logger.log("SMI-related inspection completed safely (read-only)")
            return True

        except Exception as e:
            self.logger.log_error(f"Error during safe SMI inspection: {str(e)}")
            return False

    def scan_platform_specific_features(self):
        """
        Scan for platform-specific features that might enable DMA
        """
        self.logger.log("[*] Scanning for platform-specific DMA activation methods...")

        # Check for vendor-specific registers that might enable/disable legacy features
        vendor_specific_regs = [
            # Format: (bus, device, function, offset, name)
            (0, 0, 0, 0xE0, "Root Complex Register"),
            (0, 0, 0, 0xF0, "Feature Control"),
            (0, 31, 0, 0xE0, "LPC Feature Control"),
            (0, 31, 0, 0xF0, "LPC Power Management Control")
        ]

        for bus, dev, func, offset, name in vendor_specific_regs:
            try:
                val = self.cs.pci.read_dword(bus, dev, func, offset)
                if val != 0 and val != 0xFFFFFFFF:
                    self.logger.log(f"Found potential {name} register: 0x{val:08X}")

                    # Check for specific bit patterns that might indicate DMA capabilities
                    if (val & 0x00000100) != 0:  # Example: Bit 8 set
                        self.logger.log_warning(
                            f"Register {name} has bit 8 set, which might indicate legacy DMA support")
            except:
                pass  # Skip if register doesn't exist

    def scan_platform_specific_features_enhanced(self):
        """
        Enhanced scanning for platform-specific features that might enable DMA
        """
        self.logger.log("[*] Enhanced scan for platform-specific DMA activation methods...")

        # Wider scan of PCI configuration space for interesting registers
        interesting_devices = [
            # Format: (bus, device, function, "description")
            (0, 0, 0, "Host Bridge"),
            (0, 1, 0, "PCI Express"),
            (0, 2, 0, "Graphics"),
            (0, 31, 0, "LPC Bridge"),
            (0, 31, 3, "SMBus Controller"),
            (0, 22, 0, "EHCI Controller")
        ]

        for bus, dev, func, desc in interesting_devices:
            try:
                # Read device ID information
                vid = self.cs.pci.read_word(bus, dev, func, 0x00)
                did = self.cs.pci.read_word(bus, dev, func, 0x02)

                if vid != 0xFFFF:  # Valid device
                    self.logger.log(f"Scanning {desc}: VID=0x{vid:04X}, DID=0x{did:04X}")

                    # Scan for non-standard registers in extended config space
                    # Standard config is 0x00-0x3F, but interesting stuff is often in 0x40-0xFF
                    for offset in range(0x40, 0x100, 4):
                        val = self.cs.pci.read_dword(bus, dev, func, offset)

                        # Skip registers that are all 0 or all 1 (likely unused)
                        if val != 0 and val != 0xFFFFFFFF:
                            self.logger.log(f"  Interesting register at offset 0x{offset:02X}: 0x{val:08X}")

                            # For LPC bridge, examine more closely
                            if bus == 0 and dev == 31 and func == 0:
                                # Look for bit patterns that might indicate legacy DMA support
                                if (offset >= 0xD0 and offset <= 0xE0):
                                    self.logger.log_warning(
                                        f"  Potential DMA control register in LPC bridge: 0x{offset:02X} = 0x{val:08X}")
            except:
                pass

        # Check Super I/O chips which might control legacy DMA
        self.logger.log("Scanning for Super I/O chips...")

        # Common Super I/O config ports
        sio_ports = [(0x2E, 0x2F), (0x4E, 0x4F)]

        for idx_port, data_port in sio_ports:
            try:
                # Super I/O detection sequence
                # Enter config mode
                self.cs.io.write_port_byte(idx_port, 0x87)
                self.cs.io.write_port_byte(idx_port, 0x01)
                self.cs.io.write_port_byte(idx_port, 0x55)
                self.cs.io.write_port_byte(idx_port, 0x55)

                # Read chip ID
                self.cs.io.write_port_byte(idx_port, 0x20)  # Select chip ID register
                chip_id_high = self.cs.io.read_port_byte(data_port)

                self.cs.io.write_port_byte(idx_port, 0x21)  # Select chip ID register + 1
                chip_id_low = self.cs.io.read_port_byte(data_port)

                chip_id = (chip_id_high << 8) | chip_id_low

                if chip_id != 0xFF and chip_id != 0x00:
                    self.logger.log(f"Found Super I/O chip: ID=0x{chip_id:04X}")

                    # Check for DMA configuration registers
                    # Specific registers depend on the chip
                    logical_devices = 8  # Most Super I/O chips have up to 8 logical devices

                    for ldn in range(logical_devices):
                        # Select logical device
                        self.cs.io.write_port_byte(idx_port, 0x07)
                        self.cs.io.write_port_byte(data_port, ldn)

                        # Read device ID
                        self.cs.io.write_port_byte(idx_port, 0x30)
                        dev_id = self.cs.io.read_port_byte(data_port)

                        if dev_id != 0:
                            self.logger.log(f"  Logical device {ldn}: ID=0x{dev_id:02X}")

                            # Check for DMA channel assignment
                            self.cs.io.write_port_byte(idx_port, 0x74)  # Common DMA channel register
                            dma_chan = self.cs.io.read_port_byte(data_port)

                            if dma_chan != 0:
                                self.logger.log_warning(f"  Logical device {ldn} is using DMA channel {dma_chan}")

                # Exit config mode
                self.cs.io.write_port_byte(idx_port, 0x02)
                self.cs.io.write_port_byte(data_port, 0x02)

            except Exception as e:
                self.logger.log(f"Error accessing Super I/O at port 0x{idx_port:02X}: {str(e)}")

        # Check legacy DMA controller I/O ports
        self.logger.log("Testing legacy DMA controller I/O ports...")

        # Test if full register set is accessible (beyond the basic ones we already tested)
        dma_regs = [
            (DMA1_BASE + DMA_REG_STATUS, "DMA1 Status"),
            (DMA1_BASE + DMA_REG_COMMAND, "DMA1 Command"),
            (DMA1_BASE + DMA_REG_REQUEST, "DMA1 Request"),
            (DMA1_BASE + DMA_REG_MASK_BIT, "DMA1 Mask Bit"),
            (DMA1_BASE + DMA_REG_MODE, "DMA1 Mode"),
            (DMA1_BASE + DMA_REG_CLEAR_FLIP_FLOP, "DMA1 Clear Flip-Flop"),
            (DMA1_BASE + DMA_REG_MASTER_CLEAR, "DMA1 Master Clear"),
            (DMA1_BASE + DMA_REG_CLEAR_MASK, "DMA1 Clear Mask"),
            (DMA1_BASE + DMA_REG_MASK_ALL, "DMA1 Mask All"),
            (DMA2_BASE + DMA_REG_STATUS, "DMA2 Status"),
            (DMA2_BASE + DMA_REG_COMMAND, "DMA2 Command"),
            (DMA2_BASE + DMA_REG_REQUEST, "DMA2 Request"),
            (DMA2_BASE + DMA_REG_MASK_BIT, "DMA2 Mask Bit"),
            (DMA2_BASE + DMA_REG_MODE, "DMA2 Mode"),
            (DMA2_BASE + DMA_REG_CLEAR_FLIP_FLOP, "DMA2 Clear Flip-Flop"),
            (DMA2_BASE + DMA_REG_MASTER_CLEAR, "DMA2 Master Clear"),
            (DMA2_BASE + DMA_REG_CLEAR_MASK, "DMA2 Clear Mask"),
            (DMA2_BASE + DMA_REG_MASK_ALL, "DMA2 Mask All")
        ]

        for port, name in dma_regs:
            try:
                val = self.cs.io.read_port_byte(port)
                self.logger.log(f"  {name} (port 0x{port:02X}): 0x{val:02X}")
            except:
                self.logger.log(f"  {name} (port 0x{port:02X}): Not accessible")

        # Check DMA page registers
        page_regs = [
            (0x87, "DMA Page Channel 0"),
            (0x83, "DMA Page Channel 1"),
            (0x81, "DMA Page Channel 2"),
            (0x82, "DMA Page Channel 3"),
            (0x8F, "DMA Page Channel 4"),
            (0x8B, "DMA Page Channel 5"),
            (0x89, "DMA Page Channel 6"),
            (0x8A, "DMA Page Channel 7")
        ]

        for port, name in page_regs:
            try:
                val = self.cs.io.read_port_byte(port)
                self.logger.log(f"  {name} (port 0x{port:02X}): 0x{val:02X}")
            except:
                self.logger.log(f"  {name} (port 0x{port:02X}): Not accessible")

        return True

    def run(self, module_argv):
        self.logger.log("##################################################")
        self.logger.log("# Check for Undocumented DMA over LPC Test")
        self.logger.log("##################################################")

        # Verify we're running on a Z390 chipset
        if not self.check_lpc_controller():
            self.logger.log_error("Not running on a Z390 chipset")
            self.logger.log_error("Not running on a Z390 chipset")
            self.logger.log_error("Not running on a Z390 chipset")
            # return ModuleResult.ERROR

        self.logger.log("##################################################")
        self.logger.log(f"TESTING --- test_traditional_dma_regs")
        self.logger.log("##################################################")
        # Test if traditional 8237A DMA registers respond
        dma_present = self.test_traditional_dma_regs()

        self.logger.log("##################################################")
        self.logger.log(f"TESTING --- test_h81_dma_registers")
        self.logger.log("##################################################")
        # Test for presence of H81-style hidden DMA registers
        hidden_regs_found = self.test_h81_dma_registers()

        self.logger.log("##################################################")
        self.logger.log(f"TESTING --- inspect_acpi_tables_minimal")
        self.logger.log("##################################################")
        # Inspect ACPI tables for DMA-related entries
        # self.inspect_acpi_tables()
        # self.inspect_acpi_tables_safe()
        # Use simplified ACPI inspection that won't flood console
        self.inspect_acpi_tables_minimal()

        # Then analyze the generated dump file
        self.logger.log("##################################################")
        self.logger.log("# Analyzing ACPI dump for DMA-related entries")
        self.logger.log("##################################################")
        dump_path = os.path.join("acpi_dumps", "acpi_dump.dat")
        self.analyze_acpi_dump(dump_path)

        self.logger.log("##################################################")
        self.logger.log(f"TESTING --- inspect_smi_handlers")
        self.logger.log("##################################################")
        # Inspect SMI handlers for potential DMA activity
        self.inspect_smi_handlers()

        self.logger.log("##################################################")
        self.logger.log(f"TESTING --- scan_platform_specific_features")
        self.logger.log("##################################################")
        # Scan for platform-specific features
        self.scan_platform_specific_features()

        self.logger.log("##################################################")
        self.logger.log(f"TESTING --- inspect_smi_handlers_safe")
        self.logger.log("##################################################")
        # Use enhanced SMI handler inspection
        # Causes total system freeze
        # self.inspect_smi_handlers_direct()
        self.inspect_smi_handlers_safe()

        self.logger.log("##################################################")
        self.logger.log(f"TESTING --- scan_platform_specific_features_enhanced")
        self.logger.log("##################################################")
        # Use enhanced platform-specific feature detection
        self.scan_platform_specific_features_enhanced()

        # If we found potential DMA registers, analyze them safely
        if hidden_regs_found:
            self.logger.log("\n[*] WARNING: Potential hidden DMA registers found.")
            self.logger.log("    Proceeding with non-invasive analysis only to avoid system instability.")

            # Use safer methods instead of the original try_h81_dma_activation

            self.logger.log("##################################################")
            self.logger.log(f"TESTING --- safer_dma_test")
            self.logger.log("##################################################")
            self.safer_dma_test()

            self.logger.log("##################################################")
            self.logger.log(f"TESTING --- safer_dma_test_two")
            self.logger.log("##################################################")
            self.safer_dma_test_two()

            self.logger.log("##################################################")
            self.logger.log(f"TESTING --- monitor_dma_during_system_events")
            self.logger.log("##################################################")
            # Optional: Monitor system events if user wants
            # if '-monitor' in module_argv:
            self.monitor_dma_during_system_events()

            # Optional: Poll registers if user wants
            # if '-poll' in module_argv:
            poll_duration = 30  # Default 30 seconds
            for arg in module_argv:
                if arg.startswith('-poll='):
                    try:
                        poll_duration = int(arg.split('=')[1])
                    except:
                        pass
            self.poll_dma_registers(duration=poll_duration)

            self.logger.log("##################################################")
            self.logger.log(f"TESTING --- monitor_dma_registers_long_term")
            self.logger.log("##################################################")
            self.monitor_dma_registers_long_term("monitor_dma_registers_long_term.log")

        # Final assessment
        self.logger.log("\n[*] Test Results Summary:")

        if dma_present:
            self.logger.log_warning("Traditional 8237A DMA registers appear to respond")
        else:
            self.logger.log("Traditional 8237A DMA registers do not respond")

        if hidden_regs_found:
            self.logger.log_warning("Found potential hidden H81-style DMA registers in Z390")
            self.logger.log_warning("Safe testing completed without attempting actual DMA transfers")
            self.logger.log_warning("See log for detailed register analysis")
            return ModuleResult.WARNING
        else:
            self.logger.log_good("No hidden H81-style DMA registers detected in Z390")
            return ModuleResult.PASSED
