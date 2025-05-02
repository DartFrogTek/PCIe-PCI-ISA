## Connect your AT24C02 as described:
- AT24C02 VCC to Pico 3.3V
- AT24C02 GND to Pico GND
- AT24C02 SDA to Pico GP4
- AT24C02 SCL to Pico GP5
- AT24C02 WP (Write Protect) to GND (to allow writing)
- Address pins as needed (typically all to GND)


## Run the program using Thonny or your MicroPython environment.
For IT8888F configuration, you can use option 1 and paste Intel HEX data:
```
:1000000050F10700004080400000100154FF3F0000DA
:10001000700000F30040000000740000A200000000D9
:10002000780000C2000000007C0000E1000000000001
:10003000580000010200000100640000010200AC0242
:10004000AA00000000000000000000000000000000AE
:00000001FF
```

The program will:
- Parse the Intel HEX format
- Write the data to the AT24C02
- Verify the data was written correctly

## Advanced Option: Using a Text File
If you want to use a text file instead, you can:
- Create a file named config.hex on your Pico with the hex data
- Choose option 2 and enter the filename "config.hex"

This will be useful if you want to store multiple configurations.

## Customizations:
- If your AT24C02 is at a different I2C address, adjust the EEPROM_ADDR constant.
- If you're using different I2C pins, modify the i2c initialization line.
- The program handles page boundaries in the EEPROM automatically.