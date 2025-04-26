# GRUB PCI Bridge Configuration
It's possible to configure bridges using GRUB, avoiding the need for DXE Drivers or ACPI SSDT. These changes would need to be applied before OS boot and are temporary. You'll need to add them to your GRUB configuration to apply them on every boot.

**Important:** The GRUB setpci command may be restricted or blocked when Secure Boot is enabled due to GRUB's lockdown mode. If you experience issues, disabling Secure Boot may be necessary.

*Note: These steps are under active testing and refinement.*

---
# Using lspci to find bridges
#### List all PCI devices with full details
`lspci -vvv` *Shows all PCI devices with verbose information including capabilities and configurations.*

#### Show PCI tree structure to identify bridge hierarchy
`lspci -tv` *Displays the PCI bus tree showing parent-child relationships between devices.*

#### Show specific bridge information
`lspci -vvv -s [bus:device.function]` *Shows detailed information for a specific PCI device.*

#### Show PCI bridge windows and forwarding settings
`lspci -vvv | grep -i -e "bridge" -e "i/o behind bridge" -e "memory behind bridge"` *Filters output to show only bridge-related configuration settings.*

---
# Using GRUB and setpci for bridge configuration
### Remember to replace `00:1c.0` with the actual PCI address of your bridge(s) as identified by `lspci`.
#### Enable I/O forwarding for ISA range (0x00-0xFF)
```
setpci -s 00:1c.0 1c.b=00    # I/O Base - starting address for I/O forwarding (0x00)
setpci -s 00:1c.0 1d.b=f0    # I/O Limit - ending address (0xF0 = 0xFF in PCI convention)
setpci -s 00:1c.0 1e.w=0000  # I/O Base Upper 16 bits - extends I/O base address
setpci -s 00:1c.0 30.w=f0f0  # I/O Limit Upper 16 bits - extends I/O limit address
```
#### Enable Bridge Control bits for forwarding
`setpci -s 00:1c.0 3e.w=000b`
Enables ISA forwarding and error handling:
- Bit 0: Parity Error Response Enable
- Bit 1: SERR# Enable
- Bit 3: ISA Enable (crucial for ISA DMA forwarding)
#### Enable Bus Master and I/O Space
`setpci -s 00:1c.0 04.w=0007`
Sets command register to enable:
- Bit 0: I/O Space Enable
- Bit 1: Memory Space Enable
- Bit 2: Bus Master Enable
#### Advanced: Preserve existing bits when enabling Bus Master and I/O Space
`setpci -s 00:1c.0 04.w=$(printf "%04x" $(( 0x$(setpci -s 00:1c.0 04.w) | 0x7 )))`
*Note: This preserves existing bits while setting the required ones, but may not work in all GRUB environments due to shell limitations. The simpler overwrite method is recommended for most users.*
#### Enable Memory Forwarding for ISA Shared Memory (Optional)
```
setpci -s 00:1c.0 20.w=c000  # Memory Base - starting address (0xC0000)
setpci -s 00:1c.0 22.w=fff0  # Memory Limit - ending address (0xFFFFF)
```
- Required for ISA cards using shared memory windows (video cards, some sound cards)
- Covers ISA upper memory area (0xC0000-0xFFFFF)
- 0xC0000–0xFFFFF is sometimes called "Option ROM space" or "shadow RAM" by BIOSes.

---

# Modifying GRUB configurations
Add `setpci` commands to your GRUB configuration so they apply on every boot.
## Create a Custom GRUB Script
Create a custom script file:
```bash
sudo nano /etc/grub.d/40_custom
```
Add setpci commands to this file:
```bash
#!/bin/sh
exec tail -n +3 $0
# This file provides an easy way to add custom menu entries.
menuentry 'Linux with PCI Bridge Configuration' {
    set root='hd0,msdos1' # Adjust based on your partition layout (may be hd0,gpt1 for GPT disks)
    linux /vmlinuz root=/dev/sda1 ro quiet splash
    initrd /initrd.img
    
    # Configure PCI bridges before booting
    setpci -s 00:1c.0 1c.b=00    # I/O Base (0x00)
    setpci -s 00:1c.0 1d.b=f0    # I/O Limit (0xFF)
    setpci -s 00:1c.0 1e.w=0000  # I/O Base Upper 16 bits
    setpci -s 00:1c.0 30.w=f0f0  # I/O Limit Upper 16 bits
    setpci -s 00:1c.0 3e.w=000b  # Enable ISA forwarding and error handling
    setpci -s 00:1c.0 04.w=0007  # Enable I/O, memory, bus master
    setpci -s 00:1c.0 20.w=c000  # Memory Base (0xC0000)
    setpci -s 00:1c.0 22.w=fff0  # Memory Limit (0xFFFFF)
}
```
The `set root='hd0,msdos1'` and `linux /vmlinuz root=/dev/sda1` lines need adjustment based on your system configuration.

Make the script executable:
```bash
sudo chmod +x /etc/grub.d/40_custom
```

Update GRUB configuration:
```bash
sudo update-grub
```

## Alternative: Modify 10_linux Script
For applying to all Linux entries, you can modify the 10_linux script:

*Note: modifying 10_linux will affect all kernel entries and may be overwritten on GRUB package updates.*

Create a copy of the original:
```bash
sudo cp /etc/grub.d/10_linux /etc/grub.d/10_linux.bak
```

Edit the script:
```bash
sudo nano /etc/grub.d/10_linux
```

Find the `linux_entry ()` function and add your setpci commands before the `linux` command:
```bash
# Add these lines before the "linux ${rel_dirname}/${basename}" line
echo "  setpci -s 00:1c.0 1c.b=00"
echo "  setpci -s 00:1c.0 1d.b=f0"
echo "  setpci -s 00:1c.0 1e.w=0000"
echo "  setpci -s 00:1c.0 30.w=f0f0"
echo "  setpci -s 00:1c.0 3e.w=000b"
echo "  setpci -s 00:1c.0 04.w=0007"
echo "  setpci -s 00:1c.0 20.w=c000"
echo "  setpci -s 00:1c.0 22.w=fff0"
```

Update GRUB:
```bash
sudo update-grub
```

---

## Verify 
Run `lspci -vvv` again to verify changes are applied.
