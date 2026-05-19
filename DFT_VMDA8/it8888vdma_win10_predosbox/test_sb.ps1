# Keep high bridge window
.\dist\Debug_x64\it8888ctl.exe bridge-iowin 0 28 3 0x8000 0x8fff
.\dist\Debug_x64\it8888ctl.exe bridge-iowin 3 0 0 0x8000 0x8fff

# Map PicoGUS control
.\dist\Debug_x64\it8888ctl.exe cfgwrite 0x60 4 0xe20081d0

# Request SB_MODE = 5
.\dist\Debug_x64\it8888ctl.exe out 0x81D0 1 0xCC
.\dist\Debug_x64\it8888ctl.exe out 0x81D0 1 0x03
.\dist\Debug_x64\it8888ctl.exe out 0x81D2 1 0x05

# Save settings, then reboot PicoGUS
.\dist\Debug_x64\it8888ctl.exe out 0x81D0 1 0xCC
.\dist\Debug_x64\it8888ctl.exe out 0x81D0 1 0xE1
.\dist\Debug_x64\it8888ctl.exe out 0x81D2 1 0xFF

.\dist\Debug_x64\it8888ctl.exe out 0x81D0 1 0xCC
.\dist\Debug_x64\it8888ctl.exe out 0x81D0 1 0xE2
.\dist\Debug_x64\it8888ctl.exe out 0x81D2 1 0xFF