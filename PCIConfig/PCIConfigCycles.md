
# Driver for PCI Configuration Cycles on IT8888

The IT8888 will exist as a **PCI add-in card** (not integrated into motherboard):
- **Motherboard BIOS won't configure it** - BIOS only handles onboard devices
- **Windows/Linux will detect it** as a standard PCI device (VID: 1283h, DID: 8888h) 
- **Configuration must happen post-boot** via software driver

We need to give **complete control** over the IT8888 configuration and ensure it **works perfectly** as a PCI add-in card with ISA slot(s). The key is having the right Windows/Linux driver infrastructure to access PCI configuration space.

## **Advantages of Post-Boot Configuration**

### **Dynamic Configuration**
- **Adapt to installed ISA cards**: Detect and configure for specific cards.
- **User preferences**: Allow users to enable/disable features.
- **Hot reconfiguration**: Change settings without reboot.
- **No EEPROM Programming**: No need for EEPROM programmer to change settings.

### **No BIOS Dependencies**
- **Works on any motherboard**: No need for BIOS support.
- **Portable solution**: Same driver works across different systems.
- **Future-proof**: Independent of motherboard manufacturer.

### **Advanced Features**
- **Conflict detection**: Check for address conflicts with other devices.
- **Performance tuning**: Optimize timing based on installed ISA cards.
- **Diagnostics**: Provide detailed status and configuration info.

## **Timing Considerations**

**ISA card compatibility:**
- **Configure IT8888 first**: Before ISA cards try to initialize.
- **Load early in boot**: Use service with "Boot" or "System" start type.
- **Consider PnP timing**: Some ISA PnP cards enumerate during Windows/Linux boot.

---

# DMA Configuration

DMA configuration is crucial for ISA compatibility. The IT8888 has comprehensive DMA support with **two different DMA systems**. We need the configuration to provide **full DMA functionality** that's compatible with standard ISA cards like Sound Blaster, network cards, and other DMA using devices. The key is setting up the base addresses correctly and enabling the appropriate channels for specific ISA cards.

## **IT8888 DMA Architecture Overview**

**Two DMA Systems:**
1. **DDMA (Distributed DMA)**: 7 channels (0,1,2,3,5,6,7) - maps to standard ISA DMA channels
2. **PC/PCI DMA (PPDMA)**: Uses PPDREQ#/PPDGNT# protocol for chipset integration

**Channel Mapping:**
- **Channels 0,1,2,3**: 8-bit transfers (legacy ISA DMA 0-3)
- **Channels 5,6,7**: 16-bit transfers (legacy ISA DMA 5-7) 
- **Channel 4**: Not implemented (cascade channel in original 8237)

## **Key DMA Configuration Points**

### **Base Address Requirements**
- **16-byte aligned**: Base addresses must be aligned to 16-byte boundaries.
- **Unique addresses**: Each channel needs its own unique base address range.
- **Address range**: Each channel uses 16 bytes of I/O space.

### **Extended Addressing**
- **Enable bit 3/19**: Allows 32-bit addressing beyond 16MB ISA limit.
- **Performance**: Better for modern systems with memory above 16MB.
- **Compatibility**: May need to be disabled for very old ISA cards.

### **Type-F vs Standard Timing**
- **Standard timing**: ISA-compatible, slower but safe for all cards.
- **Type-F timing**: Faster performance but may not work with all ISA cards.
- **Per-channel control**: Can enable selectively based on installed cards.
