#!/usr/bin/env python

from chipsec.module_common import BaseModule, ModuleResult
import time

# Intel 8237 DMA Controller Registers
DMA1_BASE = 0x00  # First DMA controller (8-bit channels)
DMA2_BASE = 0xC0  # Second DMA controller (16-bit channels)

# Register offsets for each DMA controller
DMA_REG_STATUS = 0x08  # Status register (read)
DMA_REG_COMMAND = 0x08  # Command register (write)
DMA_REG_REQUEST = 0x09  # Request register (write)
DMA_REG_MASK_BIT = 0x0A  # Mask bit register (write)
DMA_REG_MODE = 0x0B  # Mode register (write)
DMA_REG_CLEAR_FLIP_FLOP = 0x0C  # Clear flip-flop (write)
DMA_REG_MASTER_CLEAR = 0x0D  # Master clear (write)
DMA_REG_CLEAR_MASK = 0x0E  # Clear mask register (write)
DMA_REG_MASK_ALL = 0x0F  # Write all mask bits (write)

# Channel registers
DMA1_CHAN0_ADDR = 0x00
DMA1_CHAN0_COUNT = 0x01
DMA1_CHAN1_ADDR = 0x02
DMA1_CHAN1_COUNT = 0x03
DMA1_CHAN2_ADDR = 0x04
DMA1_CHAN2_COUNT = 0x05
DMA1_CHAN3_ADDR = 0x06
DMA1_CHAN3_COUNT = 0x07

DMA2_CHAN4_ADDR = 0xC0  # Not usable (cascade)
DMA2_CHAN4_COUNT = 0xC1  # Not usable (cascade)
DMA2_CHAN5_ADDR = 0xC4
DMA2_CHAN5_COUNT = 0xC5
DMA2_CHAN6_ADDR = 0xC8
DMA2_CHAN6_COUNT = 0xC9
DMA2_CHAN7_ADDR = 0xCC
DMA2_CHAN7_COUNT = 0xCD

# DMA1 Page registers
DMA1_PAGE_CHAN0 = 0x87
DMA1_PAGE_CHAN1 = 0x83
DMA1_PAGE_CHAN2 = 0x81
DMA1_PAGE_CHAN3 = 0x82

# DMA2 Page registers
DMA2_PAGE_CHAN5 = 0x8B
DMA2_PAGE_CHAN6 = 0x89
DMA2_PAGE_CHAN7 = 0x8A

# DMA mode bits
DMA_MODE_DEMAND = 0x00
DMA_MODE_SINGLE = 0x40
DMA_MODE_BLOCK = 0x80
DMA_MODE_CASCADE = 0xC0
DMA_MODE_VERIFY = 0x00  # Verify transfer
DMA_MODE_WRITE = 0x04  # Write (memory to device)
DMA_MODE_READ = 0x08  # Read (device to memory)
DMA_MODE_AUTO = 0x10  # Autoinit enabled
DMA_MODE_DOWN = 0x20  # Address decrement

# LPC Controller registers (Z390 specific)
LPC_BUS = 0
LPC_DEV = 0x1F
LPC_FUN = 0

# Potential LPC DMA configuration registers
LPC_REG_GEN_DMACTRL = 0xD0  # General DMA Control
LPC_REG_GEN_DMASTAT = 0xD4  # General DMA Status
LPC_REG_GEN_DMATC = 0xD8  # General DMA Transfer Count
LPC_REG_GEN_DMAADR = 0xDC  # General DMA Address


class lpc_dma_z390_test(BaseModule):
    def __init__(self):
        BaseModule.__init__(self)

    def is_supported(self):
        return True

    def check_for_dma_residue(self, channel):
        """
        Check if DMA channel has residue (indicates previous activity)
        """
        self.logger.log("[*] Checking for DMA residue on channel {}...".format(channel))

        if channel <= 3:
            base = DMA1_BASE
            count_reg = DMA1_BASE + channel * 2 + 1
        elif channel >= 5 and channel <= 7:
            base = DMA2_BASE
            count_reg = DMA2_BASE + (channel - 4) * 4 + 1
        else:
            return False

        try:
            # Clear flip-flop
            self.cs.io.write_port_byte(base + DMA_REG_CLEAR_FLIP_FLOP, 0)

            # Read count
            count_low = self.cs.io.read_port_byte(count_reg)
            count_high = self.cs.io.read_port_byte(count_reg)
            count = (count_high << 8) | count_low

            # Read status register
            status = self.cs.io.read_port_byte(base + DMA_REG_STATUS)

            # If count is not 0xFFFF (default), there might be residue
            if count != 0xFFFF:
                self.logger.log_warning("DMA channel {} has non-default count: 0x{:04X}, might have been used".format(
                    channel, count))
                return True

            self.logger.log("DMA channel {} count: 0x{:04X}, status: 0x{:02X}".format(channel, count, status))

        except Exception as e:
            self.logger.log_error("Error checking DMA residue: {}".format(str(e)))

        return False

    def test_lpc_dma_specific(self):
        """
        Test Z390-specific LPC DMA capabilities
        """
        self.logger.log("[*] Testing Z390 LPC controller for undocumented DMA features...")

        # Attempt to locate LPC controller registers that might control DMA
        is_lpc_found = False
        try:
            # Get vendor/device ID of LPC controller
            lpc_vid = self.cs.pci.read_word(LPC_BUS, LPC_DEV, LPC_FUN, 0x00)
            lpc_did = self.cs.pci.read_word(LPC_BUS, LPC_DEV, LPC_FUN, 0x02)

            if lpc_vid == 0x8086:  # Intel
                is_lpc_found = True
                self.logger.log_good("Found Intel LPC controller: VID=0x{:04X}, DID=0x{:04X}".format(lpc_vid, lpc_did))

                # Check if this is a Z390 chipset
                if lpc_did == 0xA305:  # Z390 LPC controller device ID
                    self.logger.log_good("Confirmed Z390 chipset")
                else:
                    self.logger.log_warning("This does not appear to be a Z390 chipset (DID: 0x{:04X})".format(lpc_did))
            else:
                self.logger.log_error("LPC controller not found at standard location (VID: 0x{:04X})".format(lpc_vid))

        except Exception as e:
            self.logger.log_error("Error accessing LPC controller: {}".format(str(e)))

        if not is_lpc_found:
            return False

        # Look for potential LPC DMA registers (focusing on D0-E0 range)
        found_suspicious_regs = False
        for reg_offset in range(0xD0, 0xE8, 4):
            try:
                reg_val = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, reg_offset)
                if reg_val != 0 and reg_val != 0xFFFFFFFF:
                    self.logger.log_warning("Potential DMA register found at 0x{:02X} = 0x{:08X}".format(
                        reg_offset, reg_val))
                    found_suspicious_regs = True

                    # Try to modify the register to see if it's writable
                    # Be careful - only toggle the low bit to avoid damaging the system
                    test_val = reg_val ^ 0x1  # Toggle the lowest bit only
                    old_val = reg_val

                    self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, reg_offset, test_val)
                    # Read back to see if the change took effect
                    readback = self.cs.pci.read_dword(LPC_BUS, LPC_DEV, LPC_FUN, reg_offset)

                    # Restore original value
                    self.cs.pci.write_dword(LPC_BUS, LPC_DEV, LPC_FUN, reg_offset, old_val)

                    if readback == test_val:
                        self.logger.log_bad("Register at 0x{:02X} is writable! Possible DMA control register.".format(
                            reg_offset))
            except Exception as e:
                self.logger.log_error("Error testing register 0x{:02X}: {}".format(reg_offset, str(e)))

        return found_suspicious_regs

    def try_simple_dma_operation(self, channel):
        """
        Try to perform a simple DMA operation on a specific channel
        """
        self.logger.log("[*] Attempting to initiate DMA on channel {}...".format(channel))

        if channel <= 3:
            base = DMA1_BASE
        elif channel >= 5 and channel <= 7:
            base = DMA2_BASE
        else:
            return False

        try:
            # Just try to program the DMA controller without actual memory
            # This is a minimal test to see if the controller responds

            # Mask the channel (disable)
            self.cs.io.write_port_byte(base + DMA_REG_MASK_BIT, 0x04 | channel)

            # Clear flip-flop
            self.cs.io.write_port_byte(base + DMA_REG_CLEAR_FLIP_FLOP, 0)

            # Read current status
            initial_status = self.cs.io.read_port_byte(base + DMA_REG_STATUS)

            # Set mode (single transfer, read)
            mode = DMA_MODE_SINGLE | DMA_MODE_READ | channel
            self.cs.io.write_port_byte(base + DMA_REG_MODE, mode)

            # Unmask the channel (enable)
            self.cs.io.write_port_byte(base + DMA_REG_MASK_BIT, channel)

            # Attempt to request a transfer
            self.cs.io.write_port_byte(base + DMA_REG_REQUEST, 0x04 | channel)

            # Small delay
            time.sleep(0.01)

            # Read status again
            final_status = self.cs.io.read_port_byte(base + DMA_REG_STATUS)

            # Mask the channel again when done
            self.cs.io.write_port_byte(base + DMA_REG_MASK_BIT, 0x04 | channel)

            # Check if status changed
            if initial_status != final_status:
                self.logger.log_warning("DMA channel {} status changed: 0x{:02X} -> 0x{:02X}".format(
                    channel, initial_status, final_status))
                return True

            self.logger.log("DMA channel {} status unchanged: 0x{:02X}".format(channel, final_status))

        except Exception as e:
            self.logger.log_error("Error testing DMA operation: {}".format(str(e)))

        return False

    def run(self, module_argv):
        self.logger.log("##################################################")
        self.logger.log("# LPC DMA Testing Module")
        self.logger.log("# Checks for undocumented DMA capabilities over LPC")
        self.logger.log("##################################################")

        # Check which channels might have been active (have residue)
        has_dma_residue = False
        for channel in [0, 1, 2, 3, 5, 6, 7]:  # Skip channel 4 (cascade)
            if self.check_for_dma_residue(channel):
                has_dma_residue = True

        # Test specific LPC DMA features
        found_lpc_dma = self.test_lpc_dma_specific()

        # Attempt to test standard DMA channels used by LPC
        dma_responded = False
        for channel in [1, 3]:  # Commonly used for floppy (2) and LPT (3)
            if self.try_simple_dma_operation(channel):
                dma_responded = True

        # Report findings
        self.logger.log("\n[*] Test Results Summary:")

        if has_dma_residue:
            self.logger.log_warning("Found DMA residue, indicating recent DMA activity")
        else:
            self.logger.log_good("No DMA residue found")

        if found_lpc_dma:
            self.logger.log_warning("Found potential undocumented LPC DMA registers")
        else:
            self.logger.log_good("No undocumented LPC DMA registers found")

        if dma_responded:
            self.logger.log_warning("DMA controller responded to commands")
        else:
            self.logger.log_warning("DMA controller did not respond to commands")

        # Final assessment
        if found_lpc_dma or dma_responded:
            self.logger.log_warning("Potential DMA capabilities over LPC detected!")
            return ModuleResult.WARNING
        else:
            self.logger.log_good("No evidence of undocumented DMA capabilities over LPC")
            return ModuleResult.PASSED