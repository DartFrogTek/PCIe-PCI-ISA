# Restore Intel root port to original-ish disabled I/O window
.\dist\Debug_x64\it8888ctl.exe pci-cfgwrite 0 28 3 0x04 2 0x0404
.\dist\Debug_x64\it8888ctl.exe pci-cfgwrite 0 28 3 0x1c 1 0xf0
.\dist\Debug_x64\it8888ctl.exe pci-cfgwrite 0 28 3 0x1d 1 0x00
.\dist\Debug_x64\it8888ctl.exe pci-cfgwrite 0 28 3 0x30 2 0x0000
.\dist\Debug_x64\it8888ctl.exe pci-cfgwrite 0 28 3 0x32 2 0x0000

# Restore IT8893 to original-ish disabled/inverted I/O window
.\dist\Debug_x64\it8888ctl.exe pci-cfgwrite 3 0 0 0x04 2 0x0407
.\dist\Debug_x64\it8888ctl.exe pci-cfgwrite 3 0 0 0x1c 1 0xf1
.\dist\Debug_x64\it8888ctl.exe pci-cfgwrite 3 0 0 0x1d 1 0x01
.\dist\Debug_x64\it8888ctl.exe pci-cfgwrite 3 0 0 0x30 2 0x0000
.\dist\Debug_x64\it8888ctl.exe pci-cfgwrite 3 0 0 0x32 2 0x0000