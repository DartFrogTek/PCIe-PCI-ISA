## Understanding the IT8893E Bridge
The IT8893E is a single function PCI Express to PCI bridge that:
- Complies with PCI Express Base Specification 1.1
- Complies with PCI Local Bus Specification 3.0
- Supports a x1 lane configuration with data transfer rates up to 250MB/s
- Supports 32-bit PCI bus operating at up to 66 MHz

## Creating an ACPI SSDT for IT8893E
To create an SSDT for the IT8893E bridge:
1. Identify the device in the system
2. Create an SSDT that properly configures the device properties
3. Load the SSDT during boot

## Example SSDT
See `IT8893_ACPI_SSDT.asl`

## Key Configuration Points
IT8893E key registers:
1. **Transaction Layer Control Register (TLCR)** - Address offset 0x50
   - Bit 13 controls Legacy Mode Enable
   - When set to 1, legacy support is enabled with subtractive decode
2. **PCI Control Register (PCICR)** - Address offset 0x64
   - Bits 7-0 control PCI Arbiter Priority (default 0xAA)
   - Bit 8 enables PCI Arbiter Time Out
   - Bit 11 enables pull-up resistors on PCI control signals
   - Bit 12 controls PCI Clock Slew Rate
3. **Device Control Register** - Address offset 0x78
   - Bits 7-5 control Maximum Payload Size
   - Value 000b = 128 bytes, 001b = 256 bytes

## Implementation Notes
1. **Memory Mapping**: Need to determine the base address where the IT8893E's configuration space is mapped in the system (shown as 0xE0000000).
2. **Device Path**: Adjust the device path (`\_SB.PCI0.PCIE`) and address (`_ADR`) to match the system's configuration.
3. **Loading the SSDT**: 
   - For Linux: Place the compiled SSDT in `/sys/firmware/acpi/tables/`
   - For Windows: Use tools like acpiexec or DSDT editors
   - For macOS: Use OpenCore or Clover bootloader to inject the SSDT
4. **Legacy Mode**: For supporting older devices, enable Legacy Mode (bit 13 of the Transaction Layer Control Register) as shown.


## "SSDT-IT8893E.aml"
1. The SSDT is first written in ACPI Source Language (ASL) as shown in `IT8893_ACPI_SSDT.asl`.
2. This file is compiled into an AML file using an ASL compiler such as iasl (Intel's ACPI Source Language compiler).
3. The compiled .aml file is what gets loaded by the operating system or bootloader to apply the configuration settings to the device.
4. This file would  be placed in a location where the system's bootloader or operating system can find and load it during the boot process to configure the IT8893E bridge.


## Testing the Configuration
After applying the SSDT, verify the configuration:
1. Check that the bridge appears correctly in the system:
   - Linux: `lspci -vv | grep -A 20 "ITE Tech"`
   - Windows: Device Manager or HWiNFO
2. Verify that any devices connected to the bridge function properly
3. Check if subtractive decode is working correctly for legacy devices if you've enabled that feature

## OS support
- All major operating systems should offer support for dynamically loaded SSDTs, though the implementation details and ease of use vary.
- A recovery plan should be in place in case the system becomes unstable after loading the custom SSDT.

#### Windows
Windows supports loading custom ACPI tables through various methods:
- The Windows Hardware Lab Kit (HLK) includes tools for loading ACPI tables at runtime
- Third-party tools like DSDT Editor or AIDA64 can load custom ACPI tables
- The Advanced Configuration and Power Interface (ACPI) specification that Windows follows allows for dynamic loading of SSDTs through Windows Boot Manager

#### macOS
macOS has robust support for custom ACPI tables:
- OpenCore bootloader has built-in functionality specifically designed for injecting custom SSDTs
- Clover bootloader also supports SSDT injection
- These methods are commonly used in the Hackintosh community to enable hardware compatibility

#### Linux
Linux provides several mechanisms for loading custom ACPI tables:
- The initrd/initramfs can include custom ACPI tables that are loaded during boot
- Custom tables can be loaded through the `/sys/firmware/acpi/tables/` directory
- Tools like `acpiexec` can be used to test and load ACPI tables
- The kernel parameter `acpi_table_file=` can specify custom ACPI tables to load
