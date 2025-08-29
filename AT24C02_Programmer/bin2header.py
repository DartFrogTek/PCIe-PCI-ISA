#!/usr/bin/env python3
"""
Binary to C Header Converter
============================
Converts binary files to C header files with byte arrays.
Useful for embedding binary data in C programs.
"""

import argparse
import os
import sys

def bin_to_c_header(bin_file, header_file, var_name, array_size, bytes_per_line):
    """Convert binary file to C header with byte array"""
    try:
        with open(bin_file, 'rb') as f:
            data = f.read()
    except IOError as e:
        print(f"Error reading input file: {e}")
        return False
    
    # Pad or truncate data to specified size
    if len(data) < array_size:
        data += b'\xFF' * (array_size - len(data))
        print(f"Padded data from {len(data)} to {array_size} bytes with 0xFF")
    elif len(data) > array_size:
        data = data[:array_size]
        print(f"Truncated data from {len(data)} to {array_size} bytes")
    
    try:
        with open(header_file, 'w') as f:
            # Write header guard
            guard_name = os.path.basename(header_file).upper().replace('.', '_').replace('-', '_')
            f.write(f'#ifndef {guard_name}\n')
            f.write(f'#define {guard_name}\n\n')
            
            # Write array declaration
            f.write(f'unsigned char {var_name}[{array_size}] = {{\n')
            
            # Write comment with offset range
            f.write(f'\t// Offset 0x{0:08X} to 0x{array_size-1:08X}\n')
            
            # Write data in chunks
            for i in range(0, len(data), bytes_per_line):
                f.write('\t')
                chunk = data[i:i+bytes_per_line]
                hex_values = [f'0x{b:02X}' for b in chunk]
                f.write(', '.join(hex_values))
                
                if i + bytes_per_line < len(data):
                    f.write(',')
                f.write('\n')
            
            f.write('};\n\n')
            f.write(f'#endif // {guard_name}\n')
            
    except IOError as e:
        print(f"Error writing output file: {e}")
        return False
    
    print(f"Successfully converted {bin_file} to {header_file}")
    print(f"Array name: {var_name}")
    print(f"Array size: {array_size} bytes")
    return True

def parse_args():
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(
        description='Convert binary files to C header files with byte arrays',
        epilog='Example: %(prog)s config.bin config.h --var-name ucConfigData --size 256'
    )
    
    parser.add_argument(
        'input',
        help='Input binary file path'
    )
    
    parser.add_argument(
        'output',
        help='Output C header file path'
    )
    
    parser.add_argument(
        '--var-name', '-n',
        default='ucDataBlock',
        help='C variable name for the byte array (default: ucDataBlock)'
    )
    
    parser.add_argument(
        '--size', '-s',
        type=int,
        default=256,
        help='Array size in bytes (default: 256). Data will be padded or truncated to this size'
    )
    
    parser.add_argument(
        '--bytes-per-line', '-b',
        type=int,
        default=12,
        choices=range(1, 33),
        help='Number of bytes per line in output (default: 12, max: 32)'
    )
    
    parser.add_argument(
        '--force', '-f',
        action='store_true',
        help='Overwrite output file if it exists'
    )
    
    parser.add_argument(
        '--version', '-v',
        action='version',
        version='%(prog)s 1.0'
    )
    
    return parser.parse_args()

def validate_args(args):
    """Validate command line arguments"""
    # Check input file exists
    if not os.path.isfile(args.input):
        print(f"Error: Input file '{args.input}' does not exist")
        return False
    
    # Check if output file exists (unless force is specified)
    if os.path.exists(args.output) and not args.force:
        response = input(f"Output file '{args.output}' exists. Overwrite? (y/N): ")
        if response.lower() not in ['y', 'yes']:
            print("Operation cancelled")
            return False
    
    # Validate variable name (basic C identifier check)
    if not args.var_name.isidentifier():
        print(f"Error: Variable name '{args.var_name}' is not a valid C identifier")
        return False
    
    # Check array size is reasonable
    if args.size <= 0:
        print("Error: Array size must be positive")
        return False
    
    if args.size > 1024 * 1024:  # 1MB limit
        print("Warning: Large array size specified (>1MB)")
        response = input("Continue? (y/N): ")
        if response.lower() not in ['y', 'yes']:
            return False
    
    return True

def main():
    """Main entry point"""
    args = parse_args()
    
    if not validate_args(args):
        sys.exit(1)
    
    # Show configuration
    print("Binary to C Header Converter")
    print("-" * 40)
    print(f"Input file:      {args.input}")
    print(f"Output file:     {args.output}")
    print(f"Variable name:   {args.var_name}")
    print(f"Array size:      {args.size} bytes")
    print(f"Bytes per line:  {args.bytes_per_line}")
    print("-" * 40)
    
    # Convert the file
    if bin_to_c_header(args.input, args.output, args.var_name, args.size, args.bytes_per_line):
        print("Conversion completed successfully!")
    else:
        print("Conversion failed!")
        sys.exit(1)

if __name__ == "__main__":
    main()