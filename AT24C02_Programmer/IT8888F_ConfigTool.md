# IT8888F Configuration Tool
This Python script provides a flexible way to generate a configuration binary for the IT8888F using an AT24C02 EEPROM.

## Usage Examples

1. **List available registers**:
   ```
   python it8888f_config.py --list-registers
   ```

2. **Basic configuration with subtractive decode and delayed transaction**:
   ```
   python it8888f_config.py -o config.bin --enable-subtractive --enable-delayed-tx
   ```

3. **Configure I/O spaces**:
   ```
   python it8888f_config.py --claim-io "0x3F8,8,fast" --claim-io "0x378,8,medium"
   ```
   This claims the COM1 port (0x3F8) and LPT1 port (0x378).

4. **Configure memory spaces**:
   ```
   python it8888f_config.py --claim-memory "0xD0000,64kb,medium" --claim-memory "0xF0000,64kb,fast"
   ```

5. **Enable BIOS ROM segments**:
   ```
   python it8888f_config.py --bios-segments F --bios-segments E
   ```
   This enables ROM access for the E and F segments.

6. **Set specific register values directly**:
   ```
   python it8888f_config.py -c "ISA_SPACES_TIMING=0x01000003" -c "IO_SPACE_0=0xC00002AC"
   ```

7. **Complete example with detailed output**:
   ```
   python it8888f_config.py --enable-subtractive --enable-delayed-tx \
     --claim-io "0x3F8,8,fast" --claim-io "0x2E8,8,medium" \
     --claim-memory "0xD0000,64kb,medium" --claim-memory "0xF0000,64kb,fast" \
     --bios-segments all --verbose
   ```

The script follows the SMB Boot ROM Configuration format specified in section 6.11 of the IT8888F datasheet. 

It allows you to configure every register that's available for configuration.

The resulting binary file can be programmed into an AT24C02 EEPROM, which the IT8888F will read on power-up if the SMB Boot ROM Configuration power-on strap is enabled.