"""
IT8888F EEPROM Configuration Generator
======================================
This script generates a binary file for configuring the IT8888F PCI-to-ISA bridge
via its SMB Boot ROM Configuration feature using an AT24C02 EEPROM.

The configuration data consists of 5-byte groups:
- First byte: Register index (address)
- Next 4 bytes: 32-bit data value in little-endian format
- Special index 0xAA marks the end of configuration

Reference: IT8888F Preliminary Specification V0.9
"""

import argparse
import struct
import os
from typing import Dict, List, Tuple

# Register names and their addresses
REGISTERS = {
    # Device/Vendor ID (Read-Only)
    "DEV_VENDOR_ID": 0x00,
    # Status/Command
    "STATUS_CMD": 0x04,
    # Class Code/Revision ID (Read-Only) 
    "CLASS_REVISION": 0x08,
    # Header Type/MLT/Cache Line Size
    "HEADER_MLT_CLS": 0x0C,
    # Subsystem Device/Vendor ID
    "SUBSYS_ID": 0x2C,
    # DDMA Slave Channels 0-1
    "DDMA_CH0_CH1": 0x40,
    # DDMA Slave Channels 2-3
    "DDMA_CH2_CH3": 0x44,
    # DDMA Slave Channel 5/DMA Type-F/PPD
    "DDMA_CH5_TYPE_F_PPD": 0x48,
    # DDMA Slave Channels 6-7
    "DDMA_CH6_CH7": 0x4C,
    # ROM/ISA Spaces and Timing Control
    "ISA_SPACES_TIMING": 0x50,
    # Retry/Discard Timers, Misc Control
    "TIMERS_MISC_CTRL": 0x54,
    # Positively Decoded I/O Spaces 0-5
    "IO_SPACE_0": 0x58,
    "IO_SPACE_1": 0x5C,
    "IO_SPACE_2": 0x60,
    "IO_SPACE_3": 0x64,
    "IO_SPACE_4": 0x68,
    "IO_SPACE_5": 0x6C,
    # Positively Decoded Memory Spaces 0-3
    "MEM_SPACE_0": 0x70,
    "MEM_SPACE_1": 0x74,
    "MEM_SPACE_2": 0x78,
    "MEM_SPACE_3": 0x7C,
}

# Default configuration
DEFAULT_CONFIG = {
    # Enable subtractive decode and delayed transaction
    "ISA_SPACES_TIMING": 0x00000003,
    # Enable DDMA-Concurrent, ISA Bus Refresh, Force PCI clock
    "TIMERS_MISC_CTRL": 0x8C000000,
}

def parse_args():
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(
        description="Generate an IT8888F configuration binary for AT24C02 EEPROM"
    )
    parser.add_argument(
        "-o", "--output", 
        default="it8888f_config.bin", 
        help="Output binary file (default: it8888f_config.bin)"
    )
    parser.add_argument(
        "-c", "--config", 
        action="append", 
        help="Add register configuration in format REG_NAME=0xVALUE (can be used multiple times)"
    )
    parser.add_argument(
        "--verbose", 
        action="store_true", 
        help="Print detailed configuration information"
    )
    parser.add_argument(
        "--list-registers", 
        action="store_true", 
        help="List available register names and exit"
    )
    
    # Add specific options for common configurations
    parser.add_argument(
        "--enable-subtractive", 
        action="store_true", 
        help="Enable subtractive decode for non-claimed PCI cycles"
    )
    parser.add_argument(
        "--enable-delayed-tx", 
        action="store_true", 
        help="Enable delayed transaction for PIO cycles"
    )
    parser.add_argument(
        "--claim-io", 
        action="append", 
        help="Claim I/O space in format 'base,size,speed'. Example: 0x3F8,8,fast"
    )
    parser.add_argument(
        "--claim-memory", 
        action="append", 
        help="Claim memory space in format 'base,size,speed'. Example: 0xD0000,64KB,medium"
    )
    parser.add_argument(
        "--bios-segments", 
        choices=["C", "D", "E", "F", "all"], 
        action="append", 
        help="Enable ROM chip select for specified segment(s)"
    )
    
    return parser.parse_args()

def list_registers():
    """Print a list of register names and addresses"""
    print("Available IT8888F registers:")
    print("-" * 40)
    print(f"{'Register Name':<20} {'Address':<10} {'Description':<30}")
    print("-" * 40)
    
    # Register descriptions
    descriptions = {
        "DEV_VENDOR_ID": "Device/Vendor ID (Read-Only)",
        "STATUS_CMD": "Status/Command Register",
        "CLASS_REVISION": "Class Code/Revision ID (Read-Only)",
        "HEADER_MLT_CLS": "Header Type/MLT/Cache Line Size",
        "SUBSYS_ID": "Subsystem Device/Vendor ID",
        "DDMA_CH0_CH1": "DDMA Slave Channels 0-1",
        "DDMA_CH2_CH3": "DDMA Slave Channels 2-3",
        "DDMA_CH5_TYPE_F_PPD": "DDMA Ch5/DMA Type-F/PPD",
        "DDMA_CH6_CH7": "DDMA Slave Channels 6-7",
        "ISA_SPACES_TIMING": "ROM/ISA Spaces and Timing Control",
        "TIMERS_MISC_CTRL": "Retry/Discard Timers, Misc Control",
        "IO_SPACE_0": "Positively Decoded I/O Space 0",
        "IO_SPACE_1": "Positively Decoded I/O Space 1",
        "IO_SPACE_2": "Positively Decoded I/O Space 2",
        "IO_SPACE_3": "Positively Decoded I/O Space 3",
        "IO_SPACE_4": "Positively Decoded I/O Space 4",
        "IO_SPACE_5": "Positively Decoded I/O Space 5",
        "MEM_SPACE_0": "Positively Decoded Memory Space 0",
        "MEM_SPACE_1": "Positively Decoded Memory Space 1",
        "MEM_SPACE_2": "Positively Decoded Memory Space 2",
        "MEM_SPACE_3": "Positively Decoded Memory Space 3",
    }
    
    for name, addr in sorted(REGISTERS.items(), key=lambda x: x[1]):
        desc = descriptions.get(name, "")
        print(f"{name:<20} 0x{addr:02X}        {desc:<30}")

def parse_io_claim(claim_str: str) -> Tuple[int, int, int]:
    """Parse an I/O claim specification"""
    parts = claim_str.split(',')
    if len(parts) != 3:
        raise ValueError(f"Invalid I/O claim format: {claim_str}")
    
    # Parse base address
    base = int(parts[0], 0) if parts[0].startswith(('0x', '0X')) else int(parts[0])
    
    # Parse size
    size_map = {
        "1": 0, "2": 1, "4": 2, "8": 3,
        "16": 4, "32": 5, "64": 6, "128": 7
    }
    size_str = parts[1].strip().lower()
    if size_str.endswith(('b', 'byte', 'bytes')):
        size_str = size_str.rstrip('bytes')
    size_str = size_str.strip()
    
    if size_str not in size_map:
        raise ValueError(f"Invalid I/O size: {parts[1]}. Must be 1, 2, 4, 8, 16, 32, 64, or 128 bytes")
    
    size_bits = size_map[size_str]
    
    # Parse speed
    speed_map = {
        "subtractive": 0b00, "slow": 0b01, "medium": 0b10, "fast": 0b11
    }
    speed_str = parts[2].strip().lower()
    if speed_str not in speed_map:
        raise ValueError(f"Invalid I/O speed: {parts[2]}. Must be subtractive, slow, medium, or fast")
    
    speed_bits = speed_map[speed_str]
    
    return base, size_bits, speed_bits

def parse_memory_claim(claim_str: str) -> Tuple[int, int, int]:
    """Parse a memory claim specification"""
    parts = claim_str.split(',')
    if len(parts) != 3:
        raise ValueError(f"Invalid memory claim format: {claim_str}")
    
    # Parse base address
    base = int(parts[0], 0) if parts[0].startswith(('0x', '0X')) else int(parts[0])
    
    # Parse size
    size_map = {
        "16kb": 0, "32kb": 1, "64kb": 2, "128kb": 3,
        "256kb": 4, "512kb": 5, "1mb": 6, "2mb": 7
    }
    size_str = parts[1].strip().lower().replace(" ", "")
    
    if size_str not in size_map:
        raise ValueError(f"Invalid memory size: {parts[1]}. Must be 16KB, 32KB, 64KB, 128KB, 256KB, 512KB, 1MB or 2MB")
    
    size_bits = size_map[size_str]
    
    # Parse speed
    speed_map = {
        "subtractive": 0b00, "slow": 0b01, "medium": 0b10, "fast": 0b11
    }
    speed_str = parts[2].strip().lower()
    if speed_str not in speed_map:
        raise ValueError(f"Invalid memory speed: {parts[2]}. Must be subtractive, slow, medium, or fast")
    
    speed_bits = speed_map[speed_str]
    
    return base, size_bits, speed_bits

def configure_io_space(space_num: int, base: int, size_bits: int, speed_bits: int) -> Tuple[int, int]:
    """Configure an I/O space register"""
    # Register address based on space number
    reg_addr = 0x58 + (space_num * 4)
    
    # Construct the register value
    value = (1 << 31)  # Enable
    value |= (speed_bits << 29)  # Speed
    value |= (size_bits << 24)   # Size
    value |= (base & 0xFFFF)     # Base address (16 bits)
    
    return reg_addr, value

def configure_memory_space(space_num: int, base: int, size_bits: int, speed_bits: int) -> Tuple[int, int]:
    """Configure a memory space register"""
    # Register address based on space number
    reg_addr = 0x70 + (space_num * 4)
    
    # Construct the register value
    value = (1 << 31)  # Enable
    value |= (speed_bits << 29)  # Speed
    value |= (size_bits << 24)   # Size
    
    # Handle base address - high 8 bits and low 16 bits
    high_page = (base >> 24) & 0xFF
    low_base = (base >> 8) & 0xFFFF
    
    value |= (high_page << 16)  # High page
    value |= low_base           # Low base
    
    return reg_addr, value

def configure_bios_segments(segments: List[str]) -> Tuple[int, int]:
    """Configure ROM decoding for BIOS segments"""
    # Start with current value or default
    value = DEFAULT_CONFIG.get("ISA_SPACES_TIMING", 0)
    
    # Extract the upper byte for ROM decoding
    rom_decode = (value >> 24) & 0xFF
    
    # Set the write protect bit by default
    rom_decode |= (1 << 0)  # Bit 24 of the register
    
    if "all" in segments:
        segments = ["C", "D", "E", "F"]
    
    for segment in segments:
        if segment == "C":
            # Enable C0000-C7FFF and C8000-CFFFF
            rom_decode |= (1 << 1) | (1 << 2)
        elif segment == "D":
            # Enable D0000-DFFFF
            rom_decode |= (1 << 3)
        elif segment == "E":
            # Enable E0000-EFFFF
            rom_decode |= (1 << 4)
        elif segment == "F":
            # For F segment, we need to set bit 3 in the second byte
            # This will positively decode F segment with fast DEVSEL#
            value |= (1 << 3)
    
    # Update the ROM decode bits
    value = (value & 0x00FFFFFF) | (rom_decode << 24)
    
    return REGISTERS["ISA_SPACES_TIMING"], value

def generate_config(args) -> Dict[int, int]:
    """Generate configuration based on command line arguments"""
    config = DEFAULT_CONFIG.copy()
    
    # Parse explicit register configurations
    if args.config:
        for conf in args.config:
            try:
                name, value_str = conf.split('=')
                if name not in REGISTERS:
                    raise ValueError(f"Unknown register name: {name}")
                    
                value = int(value_str, 0) if value_str.startswith(('0x', '0X')) else int(value_str)
                config[name] = value
            except ValueError as e:
                print(f"Error parsing configuration: {conf}")
                print(f"  {str(e)}")
                exit(1)
    
    # Apply specific options
    if args.enable_subtractive:
        isa_timing = config.get("ISA_SPACES_TIMING", 0)
        config["ISA_SPACES_TIMING"] = isa_timing | 0x01
    
    if args.enable_delayed_tx:
        isa_timing = config.get("ISA_SPACES_TIMING", 0)
        config["ISA_SPACES_TIMING"] = isa_timing | 0x02
    
    # Configure I/O spaces
    if args.claim_io:
        for i, claim_str in enumerate(args.claim_io):
            if i >= 6:
                print("Warning: Only 6 I/O spaces available, ignoring extra claims")
                break
                
            try:
                base, size_bits, speed_bits = parse_io_claim(claim_str)
                reg_name = f"IO_SPACE_{i}"
                reg_addr = REGISTERS[reg_name]
                
                # Create the I/O space configuration
                value = (1 << 31)  # Enable
                value |= (speed_bits << 29)  # Speed
                value |= (size_bits << 24)   # Size
                value |= (base & 0xFFFF)     # Base address (16 bits)
                
                config[reg_name] = value
            except ValueError as e:
                print(f"Error parsing I/O claim: {claim_str}")
                print(f"  {str(e)}")
                exit(1)
    
    # Configure memory spaces
    if args.claim_memory:
        for i, claim_str in enumerate(args.claim_memory):
            if i >= 4:
                print("Warning: Only 4 memory spaces available, ignoring extra claims")
                break
                
            try:
                base, size_bits, speed_bits = parse_memory_claim(claim_str)
                reg_name = f"MEM_SPACE_{i}"
                reg_addr = REGISTERS[reg_name]
                
                # Create the memory space configuration
                value = (1 << 31)  # Enable
                value |= (speed_bits << 29)  # Speed
                value |= (size_bits << 24)   # Size
                
                # Handle base address - high 8 bits and low 16 bits
                high_page = (base >> 24) & 0xFF
                low_base = (base >> 8) & 0xFFFF
                
                value |= (high_page << 16)  # High page
                value |= low_base           # Low base
                
                config[reg_name] = value
            except ValueError as e:
                print(f"Error parsing memory claim: {claim_str}")
                print(f"  {str(e)}")
                exit(1)
    
    # Configure BIOS segments
    if args.bios_segments:
        reg_addr, value = configure_bios_segments(args.bios_segments)
        reg_name = "ISA_SPACES_TIMING"
        config[reg_name] = value
    
    return config

def create_binary_data(config: Dict[str, int]) -> bytes:
    """Create binary data for the AT24C02 EEPROM"""
    data = bytearray()
    
    # Convert register name/value to address/value pairs
    addr_values = []
    for name, value in config.items():
        if name in REGISTERS:
            addr_values.append((REGISTERS[name], value))
        else:
            print(f"Warning: Unknown register name: {name}")
    
    # Sort by address for deterministic output
    addr_values.sort()
    
    # Generate 5-byte records
    for addr, value in addr_values:
        # Index byte
        data.append(addr)
        # 32-bit value in little-endian
        data.extend(struct.pack("<I", value))
    
    # End marker
    data.append(0xAA)
    
    # Pad to AT24C02 size (256 bytes) if needed
    if len(data) < 256:
        data.extend(b'\xFF' * (256 - len(data)))
    elif len(data) > 256:
        print(f"Warning: Configuration data size ({len(data)} bytes) exceeds AT24C02 capacity (256 bytes)")
        data = data[:256]
    
    return bytes(data)

def print_configuration(config: Dict[str, int]):
    """Print the configuration in human-readable format"""
    print("IT8888F Configuration:")
    print("-" * 60)
    print(f"{'Register':<20} {'Address':<8} {'Value':<10} {'Description'}")
    print("-" * 60)
    
    # Register descriptions for special bits
    for name, value in sorted(config.items(), key=lambda x: REGISTERS.get(x[0], 0) if x[0] in REGISTERS else 999):
        if name in REGISTERS:
            addr = REGISTERS[name]
            print(f"{name:<20} 0x{addr:02X}     0x{value:08X}")
            
            # Print detailed descriptions for specific registers
            if name == "ISA_SPACES_TIMING":
                print(f"  {'Subtractive Decode:':<30} {'Enabled' if value & 0x01 else 'Disabled'}")
                print(f"  {'Delayed Transaction:':<30} {'Enabled' if value & 0x02 else 'Disabled'}")
                print(f"  {'F-Segment Fast Decode:':<30} {'Enabled' if value & 0x08 else 'Disabled'}")
                
                # ROM decode
                rom_decode = (value >> 24) & 0xFF
                print(f"  {'ROM Write Protect:':<30} {'Enabled' if rom_decode & 0x01 else 'Disabled'}")
                print(f"  {'ROM CS# for C0000-C7FFF:':<30} {'Enabled' if rom_decode & 0x02 else 'Disabled'}")
                print(f"  {'ROM CS# for C8000-CFFFF:':<30} {'Enabled' if rom_decode & 0x04 else 'Disabled'}")
                print(f"  {'ROM CS# for D-segment:':<30} {'Enabled' if rom_decode & 0x08 else 'Disabled'}")
                print(f"  {'ROM CS# for E-segment:':<30} {'Enabled' if rom_decode & 0x10 else 'Disabled'}")
            
            elif name.startswith("IO_SPACE_"):
                if value & (1 << 31):  # If enabled
                    speeds = ["Subtractive", "Slow", "Medium", "Fast"]
                    speed = speeds[(value >> 29) & 0x03]
                    
                    sizes = [1, 2, 4, 8, 16, 32, 64, 128]
                    size = sizes[(value >> 24) & 0x07]
                    
                    base = value & 0xFFFF
                    
                    print(f"  {'Base Address:':<20} 0x{base:04X}")
                    print(f"  {'Size:':<20} {size} bytes")
                    print(f"  {'Speed:':<20} {speed}")
                    print(f"  {'Alias Enable:':<20} {'Yes' if value & (1 << 28) else 'No'}")
                
            elif name.startswith("MEM_SPACE_"):
                if value & (1 << 31):  # If enabled
                    speeds = ["Subtractive", "Slow", "Medium", "Fast"]
                    speed = speeds[(value >> 29) & 0x03]
                    
                    sizes = ["16KB", "32KB", "64KB", "128KB", "256KB", "512KB", "1MB", "2MB"]
                    size = sizes[(value >> 24) & 0x07]
                    
                    high_page = (value >> 16) & 0xFF
                    low_base = (value & 0xFFFF) << 8
                    
                    # Full base address
                    base = (high_page << 24) | low_base
                    
                    print(f"  {'Base Address:':<20} 0x{base:08X}")
                    print(f"  {'Size:':<20} {size}")
                    print(f"  {'Speed:':<20} {speed}")
            
            elif name == "TIMERS_MISC_CTRL":
                print(f"  {'DDMA-Concurrent:':<30} {'Enabled' if value & (1 << 31) else 'Disabled'}")
                print(f"  {'Force PCI Clock Running:':<30} {'Enabled' if value & (1 << 27) else 'Disabled'}")
                print(f"  {'ISA Bus Refresh Timer:':<30} {'Enabled' if value & (1 << 26) else 'Disabled'}")
                print(f"  {'NOGO/CLKRUN# Selection:':<30} {'CLKRUN#' if value & (1 << 20) else 'NOGO'}")
    
    print("-" * 60)
    print()

def main():
    args = parse_args()
    
    if args.list_registers:
        list_registers()
        return
        
    # Generate configuration
    config = generate_config(args)
    
    # Create binary data
    binary_data = create_binary_data(config)
    
    # Write to file
    with open(args.output, 'wb') as f:
        f.write(binary_data)
    
    print(f"Generated configuration file: {args.output}")
    print(f"Size: {len(binary_data)} bytes")
    
    if args.verbose:
        print()
        print_configuration(config)
        
        # Print binary data in hex format
        print("Binary data (hex):")
        for i in range(0, len(binary_data), 16):
            chunk = binary_data[i:i+16]
            hex_str = ' '.join(f'{b:02X}' for b in chunk)
            ascii_str = ''.join(chr(b) if 32 <= b <= 126 else '.' for b in chunk)
            print(f"{i:04X}: {hex_str:<48} {ascii_str}")

if __name__ == "__main__":
    main()
