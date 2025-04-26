#!/usr/bin/env python

from chipsec.module_common import BaseModule, ModuleResult

class lpc_dma_check(BaseModule):
    def __init__(self):
        BaseModule.__init__(self)
        
    def is_supported(self):
        # This test can run on any platform
        return True
        
    def find_lpc_controller(self):
        # Try to find the LPC controller
        self.logger.log("[*] Looking for LPC controller...")
        
        # Typical bus/device/function for LPC on Intel platforms
        bus, dev, fun = 0, 31, 0  # Common for Intel
        
        # Try to read PCI configuration to verify it's an LPC controller
        try:
            vid = self.cs.pci.read_word(bus, dev, fun, 0x00)
            did = self.cs.pci.read_word(bus, dev, fun, 0x02)
            class_code = self.cs.pci.read_byte(bus, dev, fun, 0x0B)
            sub_class = self.cs.pci.read_byte(bus, dev, fun, 0x0A)
            
            # LPC Bridge should be class 0x06, subclass 0x01
            if class_code == 0x06 and sub_class == 0x01:
                self.logger.log_good(f"Found LPC controller at {bus:02X}:{dev:02X}.{fun:X} (VID: 0x{vid:04X}, DID: 0x{did:04X})")
                return bus, dev, fun, did
                
            # Try to identify by vendor/device ID for Intel LPC
            if vid == 0x8086:
                # Many Intel LPC controllers have specific device IDs
                self.logger.log_good(f"Found Intel controller at {bus:02X}:{dev:02X}.{fun:X} (DID: 0x{did:04X})")
                return bus, dev, fun, did
        except:
            pass
            
        self.logger.log_error("Could not find LPC controller!")
        return None, None, None, None
        
    def check_tseg_protection(self):
        # Try to directly read TSEG registers based on known addresses
        self.logger.log("[*] Checking TSEG/SMRAM protection...")
        
        try:
            # TSEGMB is typically at PCI config 0:0:0 offset 0xB8 for Intel
            tsegmb_reg = self.cs.pci.read_dword(0, 0, 0, 0xB8)
            
            # Extract TSEG base and lock bit (varies by chipset, this is common pattern)
            tseg_base = (tsegmb_reg >> 20) & 0xFFF
            tseg_lock = tsegmb_reg & 0x1
            
            self.logger.log(f"TSEG Register: 0x{tsegmb_reg:08X}")
            self.logger.log(f"TSEG Base: 0x{tseg_base:03X}00000, Lock: {tseg_lock}")
            
            if tseg_lock:
                self.logger.log_good("TSEG is locked, which helps protect against DMA attacks")
                return True
            else:
                self.logger.log_bad("TSEG is not locked! SMRAM could be vulnerable to DMA attacks")
                return False
        except Exception as e:
            self.logger.log_warning(f"Could not check TSEG: {str(e)}")
            return None
            
    def check_smrr_protection(self):
        # Check SMRR registers via MSR if accessible
        self.logger.log("[*] Checking SMRR protection...")
        
        try:
            # Check SMRR base and mask on CPU 0
            smrr_base_msr = 0x1F2  # IA32_SMRR_PHYSBASE
            smrr_mask_msr = 0x1F3  # IA32_SMRR_PHYSMASK
            
            # Read MSRs directly
            smrr_base_val = self.cs.read_register_field(smrr_base_msr, 'SMRR_PHYSBASE', 0)
            smrr_mask_val = self.cs.read_register_field(smrr_mask_msr, 'SMRR_PHYSMASK', 0)
            
            self.logger.log(f"SMRR Base MSR: 0x{smrr_base_val:08X}")
            self.logger.log(f"SMRR Mask MSR: 0x{smrr_mask_val:08X}")
            
            # Check if SMRR is valid (bit 11 in mask register)
            smrr_valid = (smrr_mask_val >> 11) & 0x1
            
            if smrr_valid:
                self.logger.log_good("SMRR is enabled and valid")
                return True
            else:
                self.logger.log_warning("SMRR might not be enabled")
                return False
        except Exception as e:
            self.logger.log_warning(f"Could not check SMRR: {str(e)}")
            return None
            
    def check_for_undocumented_features(self, bus, dev, fun):
        # Scan for potential undocumented features based on register values
        self.logger.log("[*] Scanning for potential undocumented features...")
        
        suspicious_regs = []
        
        # Check range of LPC configuration registers
        for offset in range(0x80, 0x100, 4):
            try:
                val = self.cs.pci.read_dword(bus, dev, fun, offset)
                if val != 0 and val != 0xFFFFFFFF:
                    # Add to list of interesting registers
                    suspicious_regs.append((offset, val))
                    
                    # Log the register
                    self.logger.log(f"Register 0x{offset:02X}: 0x{val:08X}")
                    
                    # Check for specific bits that might enable DMA
                    if offset == 0x80 and (val & 0x100):
                        self.logger.log_warning(f"Potential DMA control bit detected in register 0x{offset:02X}")
                    
                    if offset == 0xD0 or offset == 0xE0: 
                        self.logger.log_warning(f"Potential DMA configuration detected in register 0x{offset:02X}")
            except:
                pass
                
        return suspicious_regs
        
    def check_vtd_protection(self):
        # Check for VT-d (IOMMU) protection
        self.logger.log("[*] Checking for IOMMU/VT-d protection...")
        
        try:
            # VTBAR is typically at 0xFED90000 for Intel platforms
            # We can check if the DMAR table is present in ACPI
            has_vtd = False
            
            # Try to read VT-d registers from known locations
            try:
                # VTBAR register is typically at PCI config 0:0:0 offset 0x180 for newer Intel
                vtd_bar = self.cs.pci.read_dword(0, 0, 0, 0x180)
                if vtd_bar != 0 and vtd_bar != 0xFFFFFFFF:
                    self.logger.log(f"VT-d Base Address: 0x{vtd_bar:08X}")
                    has_vtd = True
            except:
                pass
                
            if has_vtd:
                self.logger.log_good("System appears to have VT-d capability")
                return True
            else:
                self.logger.log_warning("Could not detect VT-d capability")
                return False
        except Exception as e:
            self.logger.log_warning(f"Could not check VT-d: {str(e)}")
            return None
            
    def check_lpc_dma(self):
        self.logger.start_test("Checking for potential DMA over LPC capabilities")
        
        # Find the LPC controller
        bus, dev, fun, did = self.find_lpc_controller()
        if bus is None:
            return ModuleResult.ERROR
            
        # List for tracking potential vulnerabilities
        potential_vulnerabilities = []
        
        # Check TSEG protection
        tseg_locked = self.check_tseg_protection()
        if tseg_locked is False:
            potential_vulnerabilities.append("TSEG is not locked, making SMRAM potentially vulnerable to DMA attacks")
            
        # Check SMRR protection
        smrr_valid = self.check_smrr_protection()
        if smrr_valid is False:
            potential_vulnerabilities.append("SMRR protection may not be enabled")
            
        # Check for undocumented features
        self.logger.log("\n[*] Checking for unusual LPC register values...")
        suspicious_regs = self.check_for_undocumented_features(bus, dev, fun)
        
        if suspicious_regs:
            self.logger.log_warning(f"Found {len(suspicious_regs)} non-standard register values that might indicate undocumented features")
            
        # Check for VT-d protection
        vtd_enabled = self.check_vtd_protection()
        if vtd_enabled is False:
            potential_vulnerabilities.append("No IOMMU/VT-d protection detected against DMA attacks")
            
        # Final assessment
        self.logger.log("\n[*] Summary of findings:")
        
        if tseg_locked:
            self.logger.log_good("SMRAM is protected by locked TSEG")
            
        if vtd_enabled:
            self.logger.log_good("System has IOMMU capability, which can protect against DMA attacks if properly configured")
        
        if potential_vulnerabilities:
            self.logger.log_bad("Potential vulnerabilities found:")
            for vuln in potential_vulnerabilities:
                self.logger.log_bad(f"- {vuln}")
            return ModuleResult.WARNING
        else:
            self.logger.log_good("No obvious DMA vulnerabilities detected on the LPC bus")
            return ModuleResult.PASSED
            
    def run(self, module_argv):
        self.logger.log("##################################################")
        self.logger.log("# LPC Bus DMA Capability Security Check")
        self.logger.log("##################################################")
        
        return self.check_lpc_dma()