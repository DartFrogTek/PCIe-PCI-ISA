from machine import Pin, I2C
import time
import os

# Initialize I2C with appropriate pins
i2c = I2C(0, scl=Pin(5), sda=Pin(4), freq=100000)

# AT24C02 address (default 0x50)
EEPROM_ADDR = 0x50

# EEPROM specifications
EEPROM_SIZE = 256  # 2KB = 256 bytes
PAGE_SIZE = 8      # AT24C02 page size is 8 bytes

def scan_i2c_devices():
    """Scan for available I2C devices"""
    devices = i2c.scan()
    print(f"I2C devices found: {[hex(dev) for dev in devices]}")
    if EEPROM_ADDR in devices:
        print(f"AT24C02 found at address {hex(EEPROM_ADDR)}!")
        return True
    else:
        print("AT24C02 not found! Check connections and address.")
        return False

def write_byte(addr, data):
    """Write a single byte to the specified address"""
    if addr >= EEPROM_SIZE:
        raise ValueError(f"Address {addr} out of range (0-{EEPROM_SIZE-1})")
    
    buffer = bytearray([addr, data])
    i2c.writeto(EEPROM_ADDR, buffer)
    # Wait for write cycle to complete
    time.sleep(0.005)

def read_byte(addr):
    """Read a single byte from the specified address"""
    if addr >= EEPROM_SIZE:
        raise ValueError(f"Address {addr} out of range (0-{EEPROM_SIZE-1})")
    
    i2c.writeto(EEPROM_ADDR, bytearray([addr]))
    return i2c.readfrom(EEPROM_ADDR, 1)[0]

def write_page(start_addr, data):
    """Write a page of data starting from the specified address"""
    if start_addr >= EEPROM_SIZE:
        raise ValueError(f"Start address {start_addr} out of range (0-{EEPROM_SIZE-1})")
    
    # Calculate end address
    end_addr = start_addr + len(data) - 1
    if end_addr >= EEPROM_SIZE:
        raise ValueError(f"End address {end_addr} out of range (0-{EEPROM_SIZE-1})")
    
    # Check if data crosses page boundary
    if (start_addr // PAGE_SIZE) != (end_addr // PAGE_SIZE):
        # If crossing boundary, split into multiple writes
        first_page_end = ((start_addr // PAGE_SIZE) + 1) * PAGE_SIZE - 1
        first_chunk_size = first_page_end - start_addr + 1
        
        # Write first chunk
        buffer = bytearray([start_addr]) + bytearray(data[:first_chunk_size])
        i2c.writeto(EEPROM_ADDR, buffer)
        time.sleep(0.005)  # Wait for write cycle
        
        # Write remaining chunks
        pos = first_chunk_size
        current_addr = first_page_end + 1
        
        while pos < len(data):
            # Calculate current page end
            current_page_end = ((current_addr // PAGE_SIZE) + 1) * PAGE_SIZE - 1
            chunk_size = min(current_page_end - current_addr + 1, len(data) - pos)
            
            # Write chunk
            buffer = bytearray([current_addr]) + bytearray(data[pos:pos+chunk_size])
            i2c.writeto(EEPROM_ADDR, buffer)
            time.sleep(0.005)  # Wait for write cycle
            
            pos += chunk_size
            current_addr += chunk_size
    else:
        # No page boundary crossing, write all at once
        buffer = bytearray([start_addr]) + bytearray(data)
        i2c.writeto(EEPROM_ADDR, buffer)
        time.sleep(0.005)  # Wait for write cycle

def read_sequential(start_addr, num_bytes):
    """Read multiple bytes starting from the specified address"""
    if start_addr >= EEPROM_SIZE:
        raise ValueError(f"Start address {start_addr} out of range (0-{EEPROM_SIZE-1})")
    
    if start_addr + num_bytes > EEPROM_SIZE:
        raise ValueError(f"Read would exceed EEPROM size")
    
    i2c.writeto(EEPROM_ADDR, bytearray([start_addr]))
    return i2c.readfrom(EEPROM_ADDR, num_bytes)

def parse_hex_line(line):
    """Parse a line of Intel HEX format data"""
    if not line.startswith(':'):
        return None
    
    # Remove the leading ':'
    line = line[1:]
    
    # Ensure line is valid hex
    try:
        # Convert from hex to bytes
        data = bytes.fromhex(line)
    except ValueError:
        print(f"Invalid hex line: {line}")
        return None
    
    # Check minimum length
    if len(data) < 5:
        print(f"Line too short: {line}")
        return None
    
    # Parse fields
    byte_count = data[0]
    address = (data[1] << 8) | data[2]
    record_type = data[3]
    hex_data = data[4:4+byte_count]
    checksum = data[4+byte_count]
    
    # Verify the checksum (sum of all bytes + checksum should be 0)
    calc_sum = sum(data[:-1]) & 0xFF
    calc_checksum = (0x100 - calc_sum) & 0xFF
    
    if calc_checksum != checksum:
        print(f"Checksum error in line: {line}")
        print(f"Calculated: {calc_checksum}, Expected: {checksum}")
        return None
    
    return {
        'byte_count': byte_count,
        'address': address,
        'record_type': record_type,
        'data': hex_data,
        'checksum': checksum
    }

def load_from_hex_string(hex_string):
    """Load Intel HEX data from a string"""
    lines = hex_string.strip().split('\n')
    return process_hex_lines(lines)

def load_from_hex_file(filename):
    """Load Intel HEX data from a file"""
    try:
        with open(filename, 'r') as f:
            lines = f.readlines()
        return process_hex_lines([line.strip() for line in lines])
    except OSError as e:
        print(f"Error opening file: {e}")
        return None

def process_hex_lines(lines):
    """Process parsed Intel HEX lines into EEPROM data"""
    memory_data = {}  # Dictionary to hold address -> data mapping
    
    for line in lines:
        line = line.strip()
        if not line:
            continue
            
        parsed = parse_hex_line(line)
        if not parsed:
            continue
            
        if parsed['record_type'] == 0:  # Data record
            # Add data to our memory map
            for i, byte_val in enumerate(parsed['data']):
                addr = parsed['address'] + i
                memory_data[addr] = byte_val
        elif parsed['record_type'] == 1:  # End of file record
            break
    
    return memory_data

def write_memory_to_eeprom(memory_data):
    """Write the parsed memory data to the EEPROM"""
    if not memory_data:
        print("No data to write to EEPROM")
        return False
    
    print(f"Writing {len(memory_data)} bytes to EEPROM...")
    
    # Convert dictionary to a list of (address, value) tuples and sort by address
    sorted_data = sorted(memory_data.items())
    
    # Group consecutive addresses for page writes
    current_page_start = None
    current_page_data = []
    
    for addr, value in sorted_data:
        page_num = addr // PAGE_SIZE
        
        if current_page_start is None:
            # First data item
            current_page_start = addr
            current_page_data = [value]
        elif addr // PAGE_SIZE == current_page_start // PAGE_SIZE and addr == current_page_start + len(current_page_data):
            # Same page and consecutive address
            current_page_data.append(value)
        else:
            # New page or non-consecutive address, write the current page
            if current_page_data:
                print(f"Writing page at address 0x{current_page_start:02X}: {[hex(d) for d in current_page_data]}")
                try:
                    write_page(current_page_start, current_page_data)
                except Exception as e:
                    print(f"Error writing page: {e}")
                    return False
            
            # Start a new page
            current_page_start = addr
            current_page_data = [value]
    
    # Write the last page if there's any data left
    if current_page_data:
        print(f"Writing page at address 0x{current_page_start:02X}: {[hex(d) for d in current_page_data]}")
        try:
            write_page(current_page_start, current_page_data)
        except Exception as e:
            print(f"Error writing page: {e}")
            return False
    
    print("EEPROM write complete!")
    return True

def verify_eeprom_data(memory_data):
    """Verify the EEPROM data matches what we wrote"""
    if not memory_data:
        print("No data to verify")
        return False
    
    print(f"Verifying {len(memory_data)} bytes in EEPROM...")
    
    # Check each address
    for addr, expected_value in sorted(memory_data.items()):
        if addr >= EEPROM_SIZE:
            print(f"Skipping verification of address 0x{addr:02X} (out of range)")
            continue
            
        actual_value = read_byte(addr)
        if actual_value != expected_value:
            print(f"Verification failed at address 0x{addr:02X}: expected 0x{expected_value:02X}, got 0x{actual_value:02X}")
            return False
    
    print("EEPROM verification successful!")
    return True

def dump_eeprom_contents(start_addr=0, length=EEPROM_SIZE):
    """Display the EEPROM contents in a hex dump format"""
    if start_addr < 0 or start_addr >= EEPROM_SIZE:
        print(f"Invalid start address: {start_addr}")
        return
    
    if start_addr + length > EEPROM_SIZE:
        length = EEPROM_SIZE - start_addr
    
    print(f"EEPROM contents from 0x{start_addr:02X} to 0x{start_addr+length-1:02X}:")
    
    # Read data in chunks
    chunk_size = 16  # Display 16 bytes per line
    for base_addr in range(start_addr, start_addr + length, chunk_size):
        # Calculate how many bytes to read in this chunk
        read_len = min(chunk_size, start_addr + length - base_addr)
        
        # Read the chunk
        data = read_sequential(base_addr, read_len)
        
        # Format the address
        line = f"0x{base_addr:04X}: "
        
        # Format the hex values
        for i, byte_val in enumerate(data):
            line += f"{byte_val:02X} "
            if i == 7:  # Add an extra space in the middle for readability
                line += " "
        
        # Pad with spaces if we didn't read a full line
        if read_len < chunk_size:
            line += "   " * (chunk_size - read_len)
            if read_len <= 8:  # Adjust for the extra middle space
                line += " "
        
        # Add ASCII representation
        line += " |"
        for byte_val in data:
            if 32 <= byte_val <= 126:  # Printable ASCII
                line += chr(byte_val)
            else:
                line += "."
        line += "|"
        
        print(line)

# Example usage
def main():
    if not scan_i2c_devices():
        return
    
    print("\nAT24C02 EEPROM Programming Tool")
    print("-------------------------------")
    print("1. Program from Intel HEX string")
    print("2. Program from Intel HEX file")
    print("3. Dump EEPROM contents")
    print("4. Exit")
    
    choice = input("Enter your choice (1-4): ")
    
    if choice == "1":
        print("Enter Intel HEX data (paste multiple lines, end with an empty line):")
        lines = []
        while True:
            line = input()
            if not line:
                break
            lines.append(line)
        
        hex_data = "\n".join(lines)
        memory_data = load_from_hex_string(hex_data)
        
        if memory_data:
            if write_memory_to_eeprom(memory_data):
                verify_eeprom_data(memory_data)
    
    elif choice == "2":
        filename = input("Enter Intel HEX filename: ")
        memory_data = load_from_hex_file(filename)
        
        if memory_data:
            if write_memory_to_eeprom(memory_data):
                verify_eeprom_data(memory_data)
    
    elif choice == "3":
        start_str = input("Enter start address (hex, default 0x00): ")
        length_str = input("Enter length (decimal, default 256): ")
        
        start_addr = 0
        length = EEPROM_SIZE
        
        if start_str:
            try:
                start_addr = int(start_str, 16)
            except ValueError:
                print("Invalid hex address, using default 0x00")
        
        if length_str:
            try:
                length = int(length_str)
            except ValueError:
                print(f"Invalid length, using default {EEPROM_SIZE}")
        
        dump_eeprom_contents(start_addr, length)
    
    elif choice == "4":
        print("Exiting...")
    
    else:
        print("Invalid choice")

if __name__ == "__main__":
    main()