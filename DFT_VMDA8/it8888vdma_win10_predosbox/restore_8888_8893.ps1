# Disable/invert root port I/O forwarding
.\dist\Debug_x64\it8888ctl.exe pci-cfgwrite 0 28 3 0x04 2 0x0404
.\dist\Debug_x64\it8888ctl.exe pci-cfgwrite 0 28 3 0x1c 1 0xf0
.\dist\Debug_x64\it8888ctl.exe pci-cfgwrite 0 28 3 0x1d 1 0x00
.\dist\Debug_x64\it8888ctl.exe pci-cfgwrite 0 28 3 0x30 2 0x0000
.\dist\Debug_x64\it8888ctl.exe pci-cfgwrite 0 28 3 0x32 2 0x0000

# Disable/invert IT8893 I/O forwarding
.\dist\Debug_x64\it8888ctl.exe pci-cfgwrite 3 0 0 0x04 2 0x0407
.\dist\Debug_x64\it8888ctl.exe pci-cfgwrite 3 0 0 0x1c 1 0xf1
.\dist\Debug_x64\it8888ctl.exe pci-cfgwrite 3 0 0 0x1d 1 0x01
.\dist\Debug_x64\it8888ctl.exe pci-cfgwrite 3 0 0 0x30 2 0x0000
.\dist\Debug_x64\it8888ctl.exe pci-cfgwrite 3 0 0 0x32 2 0x0000

# Restore normal IT8888 decode registers
.\dist\Debug_x64\it8888ctl.exe cfgwrite 0x58 4 0xe4000220
.\dist\Debug_x64\it8888ctl.exe cfgwrite 0x5C 4 0xe3000330
.\dist\Debug_x64\it8888ctl.exe cfgwrite 0x60 4 0xe2000388

# Clear trace/errors
.\dist\Debug_x64\it8888ctl.exe clear-errors
.\dist\Debug_x64\it8888ctl.exe trace-clear

# Verify
.\dist\Debug_x64\it8888ctl.exe pci-dumpcfg 0 28 3
.\dist\Debug_x64\it8888ctl.exe pci-dumpcfg 3 0 0
.\dist\Debug_x64\it8888ctl.exe info