# Save current cfg60
.\dist\Debug_x64\it8888ctl.exe cfgread 0x60 4

# Temporarily map a small I/O decode window at 0x8080
.\dist\Debug_x64\it8888ctl.exe cfgwrite 0x60 4 0xe2008080
.\dist\Debug_x64\it8888ctl.exe cfgread 0x60 4

# Send POST codes through the high forwarded bridge window
.\dist\Debug_x64\it8888ctl.exe trace-clear
.\dist\Debug_x64\it8888ctl.exe out 0x8080 1 0x12
Start-Sleep -Milliseconds 1
.\dist\Debug_x64\it8888ctl.exe out 0x8080 1 0x34
Start-Sleep -Milliseconds 1
.\dist\Debug_x64\it8888ctl.exe out 0x8080 1 0x56
.\dist\Debug_x64\it8888ctl.exe trace