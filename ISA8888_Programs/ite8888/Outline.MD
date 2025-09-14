**Windows 9x/NT/2K/XP** will **not automatically enumerate ISA devices** behind a bridge unless they are **properly declared and configured** in the **PCI configuration space** of the bridge itself.

---

## Problem Summary

> **Windows doesn't see the ISA card behind the ITE8888 bridge**, even though:
- The bridge is detected
- The config tool runs
- The ISA card is physically inserted

---

## What You Need to Do

### 1. **Understand: ISA is NOT Plug-and-Play**
Unlike PCI, **ISA devices are not auto-enumerated**. 

You must **manually declare** them to the OS via the bridge.

---

### 2. **Configure the ITE8888 to Positively Decode the ISA Card’s Resources**

You **must** configure the **ITE8888** to **positively decode** the **I/O ports**, **memory**, and **DMA/IRQ** used by the ISA card.

#### Use the config tool (`ite8888_vs.cpp`) to:
- **Enable I/O space decoding** for the exact range the ISA card uses
- **Enable memory space decoding** if the card uses memory-mapped I/O
- **Enable DMA channel** if the card uses DMA
- **Enable IRQ** via **Serialized IRQ** or **legacy IRQ** routing

#### Example: Sound Blaster 16
```cpp
IO Space 0: 0x220–0x22F (16 bytes, fast)
IO Space 1: 0x330–0x331 (2 bytes, fast)
DMA: Channel 1 (8-bit), Channel 5 (16-bit)
IRQ: 5 or 7 (via SERIRQ or legacy)
```

---

### 3. **Enable Serialized IRQ (SERIRQ)**
If the ISA card uses **IRQs**, you **must**:
- Enable **SERIRQ** in the **ITE8888** config
- Ensure the **IRQ is routed** to the **PCI interrupt** (INTA#)
- Set the **IRQ polarity** and **trigger type** correctly

> In the config tool:
```cpp
misc_control |= (1 << 24); // Enable SERIRQ
```

---

### 4. **Tell Windows About the ISA Card**

#### For Windows 9x (95/98/ME):
You **must** add a **static entry** in:
- `C:\Windows\System.ini`
- Or use **Device Manager > Add New Hardware > Manual**
- Or use **INF file** to declare the device

#### For Windows NT/2000/XP:
You **must**:
- Create a **custom INF file** that declares the ISA device behind the PCI bridge
- Use **DevCon** or **Device Manager** to install it
- Or use **ACPI BIOS** to declare the device (if you control the BIOS)

---

### 5. **Check BIOS Settings**
- **PnP OS Installed**: Set to **NO** (especially for 9x)
- **Assign IRQ to PCI**: **Enabled**
- **IRQ routing**: Ensure **IRQ 5, 7, 9, 10, 11** are **available** for ISA

---

### 6. **Verify with Debug Tools**

#### Use these to verify:
- **Windows Device Manager**: Look for **unknown devices** or **conflicts**
- **System Information** (`msinfo32.exe`): Check **IRQ/DMA/I/O** usage
- **PCI Tools**: Like **PCI-Z**, **HWiNFO**, or **the config tool** to verify bridge config
- **ISA Diagnostic Tools**: Like **CheckIt**, **NSSI**, or **vendor tools**

---

## Example: Sound Blaster 16 Behind ITE8888

| Resource | Value |
|----------|--------|
| I/O      | 0x220–0x22F, 0x330–0x331 |
| IRQ      | 5 |
| DMA      | 1 (8-bit), 5 (16-bit) |

### Config Tool Settings:
```cpp
io_spaces[0] = 0xE4000220; // 0x220-0x22F, fast, 16 bytes
io_spaces[1] = 0xE2000330; // 0x330-0x331, fast, 2 bytes
dma_config[0] = 0x80000001; // DMA 1
dma_config[2] = 0x80000005; // DMA 5
misc_control = 0x8C000003;  // DDMA + subtractive + delayed
```

---

## Checklist

| Step | Status |
|------|--------|
| * ITE8888 detected | Y |
| * ISA card inserted | Y |
| * I/O space configured | ? |
| * Memory space configured (if needed) | ? |
| * DMA channel configured | ? |
| * IRQ routed (SERIRQ or legacy) | ? |
| * Windows told about device (INF or manual) | ? |
| * BIOS PnP set to NO | ? |

---