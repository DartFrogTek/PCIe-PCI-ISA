# **Configuring IT8888 in Linux Post-Boot**

## **Implementation Approaches**

### **1. Linux Kernel Module** (Recommended Approach)
```c
// it8888_driver.c
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>

#define IT8888_VENDOR_ID 0x1283
#define IT8888_DEVICE_ID 0x8888

static struct pci_device_id it8888_pci_tbl[] = {
    { PCI_DEVICE(IT8888_VENDOR_ID, IT8888_DEVICE_ID) },
    { 0, }
};

MODULE_DEVICE_TABLE(pci, it8888_pci_tbl);

static int it8888_probe(struct pci_dev *pdev, const struct pci_device_id *id) {
    int ret;
    u32 config_data;
    
    dev_info(&pdev->dev, "IT8888 PCI-to-ISA bridge found\n");
    
    // Enable the device
    ret = pci_enable_device(pdev);
    if (ret) {
        dev_err(&pdev->dev, "Failed to enable device\n");
        return ret;
    }
    
    // Read current Cfg_50h register
    pci_read_config_dword(pdev, 0x50, &config_data);
    
    // Configure IT8888 for ISA card support
    config_data |= 0x01;        // Enable subtractive decode
    config_data |= 0x20;        // Enable Port 80h snoop
    pci_write_config_dword(pdev, 0x50, config_data);
    
    // Set up I/O decode windows
    configure_io_spaces(pdev);
    
    // Set up memory decode windows  
    configure_memory_spaces(pdev);
    
    dev_info(&pdev->dev, "IT8888 configured successfully\n");
    return 0;
}

static void it8888_remove(struct pci_dev *pdev) {
    dev_info(&pdev->dev, "IT8888 driver removed\n");
}

static struct pci_driver it8888_driver = {
    .name       = "it8888",
    .id_table   = it8888_pci_tbl,
    .probe      = it8888_probe,
    .remove     = it8888_remove,
};

static int __init it8888_init(void) {
    return pci_register_driver(&it8888_driver);
}

static void __exit it8888_exit(void) {
    pci_unregister_driver(&it8888_driver);
}

module_init(it8888_init);
module_exit(it8888_exit);

MODULE_AUTHOR("Dart Frog Tek");
MODULE_DESCRIPTION("IT8888 PCI-to-ISA Bridge Driver");
MODULE_LICENSE("MIT");
```

### **2. User-Space with libpci (Development/Testing)**
```c
// it8888_config.c
#include <stdio.h>
#include <stdlib.h>
#include <pci/pci.h>
#include <unistd.h>

struct pci_access *pacc;

int configure_it8888() {
    struct pci_dev *dev;
    u32 config_data;
    
    // Initialize PCI library
    pacc = pci_alloc();
    pci_init(pacc);
    pci_scan_bus(pacc);
    
    // Find IT8888 device
    for (dev = pacc->devices; dev; dev = dev->next) {
        if (dev->vendor_id == 0x1283 && dev->device_id == 0x8888) {
            printf("Found IT8888 at %02x:%02x.%d\n", 
                   dev->bus, dev->dev, dev->func);
            
            // Configure main control register (Cfg_50h)
            config_data = pci_read_long(dev, 0x50);
            config_data |= 0x01;    // Enable subtractive decode
            config_data |= 0x20;    // Enable Port 80h snoop
            pci_write_long(dev, 0x50, config_data);
            
            // Configure I/O spaces for ISA cards
            setup_isa_io_spaces(dev);
            
            // Configure memory spaces
            setup_isa_memory_spaces(dev);
            
            printf("IT8888 configuration complete\n");
            return 0;
        }
    }
    
    printf("IT8888 not found\n");
    return -1;
}

void setup_isa_io_spaces(struct pci_dev *dev) {
    u32 io_space;
    
    // Configure I/O Space 0: Sound card at 0x220-0x22F  
    io_space = 0x80000000 |     // Enable
               (0x02 << 29) |   // Medium speed
               (0x04 << 24) |   // 16 bytes
               0x0220;          // Base address
    pci_write_long(dev, 0x58, io_space);
    
    // Configure I/O Space 1: Game port at 0x200-0x207
    io_space = 0x80000000 |     // Enable  
               (0x02 << 29) |   // Medium speed
               (0x03 << 24) |   // 8 bytes
               0x0200;          // Base address
    pci_write_long(dev, 0x5C, io_space);
}

int main(int argc, char **argv) {
    if (geteuid() != 0) {
        printf("This program requires root privileges\n");
        return 1;
    }
    
    return configure_it8888();
}
```

### **3. Using setpci Command Line Tool**
```bash
#!/bin/bash
# it8888_setup.sh - Configure IT8888 from shell script

# Find IT8888 device
IT8888_DEV=$(lspci -d 1283:8888 | cut -d' ' -f1)

if [ -z "$IT8888_DEV" ]; then
    echo "IT8888 not found"
    exit 1
fi

echo "Configuring IT8888 at $IT8888_DEV"

# Enable subtractive decode and Port 80h snoop
setpci -s $IT8888_DEV 50.l=00000021

# Configure I/O Space 0: Sound card at 0x220 (16 bytes, medium speed)
setpci -s $IT8888_DEV 58.l=84040220

# Configure I/O Space 1: Game port at 0x200 (8 bytes, medium speed) 
setpci -s $IT8888_DEV 5c.l=84030200

# Configure Memory Space 0: VGA at 0xA0000 (64KB, medium speed)
setpci -s $IT8888_DEV 70.l=8401A000

echo "IT8888 configuration complete"

# Display current configuration
echo "Current configuration:"
lspci -s $IT8888_DEV -vvv
```

## **Makefile for Kernel Module**
```makefile
# Makefile
obj-m += it8888.o
it8888-objs := it8888_driver.o

KDIR := /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

install: all
	sudo insmod it8888.ko

remove:
	sudo rmmod it8888

# Build user-space tool
userspace: it8888_config.c
	gcc -o it8888_config it8888_config.c -lpci

.PHONY: all clean install remove userspace
```

---

# **DMA Configuration Over PCI Cycles**

## **Complete Linux DMA Configuration Code**

### **Kernel Module DMA Configuration**
```c
// it8888_dma.c - Linux kernel module for DMA configuration
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>

struct it8888_dma_config {
    struct pci_dev *pdev;
    bool ddma_enabled;
    bool ppdma_enabled; 
    bool type_f_timing;
};

static void configure_it8888_dma(struct it8888_dma_config *config) {
    struct pci_dev *pdev = config->pdev;
    u32 cfg_40h, cfg_44h, cfg_48h, cfg_4ch;
    
    dev_info(&pdev->dev, "Configuring IT8888 DMA channels\n");
    
    // ===== Configure DDMA Channels 0 & 1 (Cfg_40h) =====
    cfg_40h = 0;
    
    // Channel 0 (8-bit): Base address 0x1000, enabled, extended addressing
    cfg_40h |= (0x100 << 4);    // Base address A[15:4] = 0x100 (0x1000)
    cfg_40h |= (1 << 3);        // Extended addressing enable
    cfg_40h |= (1 << 0);        // Channel 0 enable
    
    // Channel 1 (8-bit): Base address 0x2000, enabled
    cfg_40h |= (0x200 << 20);   // Base address A[15:4] = 0x200 (0x2000) 
    cfg_40h |= (1 << 19);       // Extended addressing enable
    cfg_40h |= (1 << 16);       // Channel 1 enable
    
    pci_write_config_dword(pdev, 0x40, cfg_40h);
    
    // ===== Configure DDMA Channels 2 & 3 (Cfg_44h) =====
    cfg_44h = 0;
    
    // Channel 2 (8-bit): Base address 0x3000, enabled
    cfg_44h |= (0x300 << 4);    // Base address A[15:4] = 0x300 (0x3000)
    cfg_44h |= (1 << 3);        // Extended addressing enable
    cfg_44h |= (1 << 0);        // Channel 2 enable
    
    // Channel 3 (8-bit): Base address 0x4000, enabled
    cfg_44h |= (0x400 << 20);   // Base address A[15:4] = 0x400 (0x4000)
    cfg_44h |= (1 << 19);       // Extended addressing enable
    cfg_44h |= (1 << 16);       // Channel 3 enable
    
    pci_write_config_dword(pdev, 0x44, cfg_44h);
    
    // ===== Configure Channel 5, Type-F Timing, PPDMA (Cfg_48h) =====
    cfg_48h = 0;
    
    // PC/PCI DMA Control (bits 7:0)
    cfg_48h |= 0xF0;            // Enable DREQ for channels 7,6,5
    cfg_48h |= (1 << 4);        // PPDMA Global Enable
    cfg_48h |= 0x0F;            // Enable DREQ for channels 3,2,1,0
    
    // Type-F DMA Timing (bits 15:8)
    if (config->type_f_timing) {
        cfg_48h |= 0xFF00;      // Enable Type-F for all channels
        dev_info(&pdev->dev, "Type-F DMA timing enabled\n");
    }
    
    // Channel 5 (16-bit): Base address 0x5000, enabled
    cfg_48h |= (0x500 << 20);   // Base address A[15:4] = 0x500 (0x5000)
    cfg_48h |= (1 << 19);       // Extended addressing enable
    cfg_48h |= (1 << 16);       // Channel 5 enable
    
    pci_write_config_dword(pdev, 0x48, cfg_48h);
    
    // ===== Configure DDMA Channels 6 & 7 (Cfg_4Ch) =====
    cfg_4ch = 0;
    
    // Channel 6 (16-bit): Base address 0x6000, enabled
    cfg_4ch |= (0x600 << 4);    // Base address A[15:4] = 0x600 (0x6000)
    cfg_4ch |= (1 << 3);        // Extended addressing enable
    cfg_4ch |= (1 << 0);        // Channel 6 enable
    
    // Channel 7 (16-bit): Base address 0x7000, enabled
    cfg_4ch |= (0x700 << 20);   // Base address A[15:4] = 0x700 (0x7000)
    cfg_4ch |= (1 << 19);       // Extended addressing enable
    cfg_4ch |= (1 << 16);       // Channel 7 enable
    
    pci_write_config_dword(pdev, 0x4C, cfg_4ch);
    
    dev_info(&pdev->dev, "DMA configuration complete - all channels enabled\n");
}

// Probe function with DMA setup
static int it8888_probe(struct pci_dev *pdev, const struct pci_device_id *id) {
    struct it8888_dma_config config = {
        .pdev = pdev,
        .ddma_enabled = true,
        .ppdma_enabled = true,
        .type_f_timing = false  // Start with ISA-compatible timing
    };
    
    int ret = pci_enable_device(pdev);
    if (ret) return ret;
    
    // Set DMA mask for 32-bit addressing
    ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
    if (ret) {
        dev_err(&pdev->dev, "Failed to set DMA mask\n");
        return ret;
    }
    
    configure_it8888_dma(&config);
    
    // Store config in driver private data
    pci_set_drvdata(pdev, &config);
    
    return 0;
}
```

### **User-Space DMA Configuration with libpci**
```c
// it8888_dma_config.c - User-space DMA configuration
#include <stdio.h>
#include <stdlib.h>
#include <pci/pci.h>
#include <unistd.h>
#include <stdint.h>

void setup_dma_for_isa_cards(struct pci_dev *dev) {
    printf("Configuring IT8888 DMA for ISA card support...\n");
    
    // Configure DDMA Channels 0 & 1 (Cfg_40h)
    uint32_t cfg_40h = 0;
    cfg_40h |= (0x100 << 4) | (1 << 3) | (1 << 0);        // Channel 0
    cfg_40h |= (0x200 << 20) | (1 << 19) | (1 << 16);     // Channel 1
    pci_write_long(dev, 0x40, cfg_40h);
    
    // Configure DDMA Channels 2 & 3 (Cfg_44h)  
    uint32_t cfg_44h = 0;
    cfg_44h |= (0x300 << 4) | (1 << 3) | (1 << 0);        // Channel 2
    cfg_44h |= (0x400 << 20) | (1 << 19) | (1 << 16);     // Channel 3
    pci_write_long(dev, 0x44, cfg_44h);
    
    // Configure Channel 5, Type-F, PPDMA (Cfg_48h)
    uint32_t cfg_48h = 0;
    cfg_48h |= 0xF0 | (1 << 4) | 0x0F;                    // PPDMA control
    cfg_48h |= (0x500 << 20) | (1 << 19) | (1 << 16);     // Channel 5
    pci_write_long(dev, 0x48, cfg_48h);
    
    // Configure DDMA Channels 6 & 7 (Cfg_4Ch)
    uint32_t cfg_4ch = 0;
    cfg_4ch |= (0x600 << 4) | (1 << 3) | (1 << 0);        // Channel 6
    cfg_4ch |= (0x700 << 20) | (1 << 19) | (1 << 16);     // Channel 7  
    pci_write_long(dev, 0x4C, cfg_4ch);
    
    printf("DMA configuration complete:\n");
    printf("- Channels 0-3 (8-bit): Enabled, bases 0x1000-0x4000\n");
    printf("- Channels 5-7 (16-bit): Enabled, bases 0x5000-0x7000\n");
    printf("- PC/PCI DMA: Enabled\n");
}

void setup_dma_sound_blaster(struct pci_dev *dev) {
    printf("Configuring DMA for Sound Blaster compatibility...\n");
    
    // Sound Blaster typically uses DMA 1 (8-bit) and DMA 5 (16-bit)
    
    // Configure Channel 1 only
    uint32_t cfg_40h = pci_read_long(dev, 0x40);
    cfg_40h |= (0x100 << 20) | (1 << 19) | (1 << 16);     // Channel 1
    pci_write_long(dev, 0x40, cfg_40h);
    
    // Configure Channel 5 only  
    uint32_t cfg_48h = pci_read_long(dev, 0x48);
    cfg_48h |= 0xF0 | (1 << 4) | 0x0F;                    // Enable PPDMA
    cfg_48h |= (0x500 << 20) | (1 << 19) | (1 << 16);     // Channel 5
    pci_write_long(dev, 0x48, cfg_48h);
    
    printf("Sound Blaster DMA setup complete (Channels 1 & 5)\n");
}

void display_dma_status(struct pci_dev *dev) {
    printf("\nIT8888 DMA Configuration Status:\n");
    
    uint32_t cfg_40h = pci_read_long(dev, 0x40);
    uint32_t cfg_44h = pci_read_long(dev, 0x44);
    uint32_t cfg_48h = pci_read_long(dev, 0x48);
    uint32_t cfg_4ch = pci_read_long(dev, 0x4C);
    
    // Display channel status
    printf("Channel 0 (8-bit):  %s, Base: 0x%04X\n",
           (cfg_40h & 0x01) ? "Enabled " : "Disabled",
           ((cfg_40h >> 4) & 0xFFF) << 4);
           
    printf("Channel 1 (8-bit):  %s, Base: 0x%04X\n", 
           (cfg_40h & 0x10000) ? "Enabled " : "Disabled",
           ((cfg_40h >> 20) & 0xFFF) << 4);
           
    printf("Channel 2 (8-bit):  %s, Base: 0x%04X\n",
           (cfg_44h & 0x01) ? "Enabled " : "Disabled", 
           ((cfg_44h >> 4) & 0xFFF) << 4);
           
    printf("Channel 3 (8-bit):  %s, Base: 0x%04X\n",
           (cfg_44h & 0x10000) ? "Enabled " : "Disabled",
           ((cfg_44h >> 20) & 0xFFF) << 4);
           
    printf("Channel 5 (16-bit): %s, Base: 0x%04X\n",
           (cfg_48h & 0x10000) ? "Enabled " : "Disabled",
           ((cfg_48h >> 20) & 0xFFF) << 4);
           
    printf("Channel 6 (16-bit): %s, Base: 0x%04X\n",
           (cfg_4ch & 0x01) ? "Enabled " : "Disabled",
           ((cfg_4ch >> 4) & 0xFFF) << 4);
           
    printf("Channel 7 (16-bit): %s, Base: 0x%04X\n",
           (cfg_4ch & 0x10000) ? "Enabled " : "Disabled", 
           ((cfg_4ch >> 20) & 0xFFF) << 4);
    
    printf("\nPC/PCI DMA: %s\n", (cfg_48h & 0x10) ? "Enabled" : "Disabled");
    printf("Type-F Timing: 0x%02X %s\n", 
           (cfg_48h >> 8) & 0xFF,
           ((cfg_48h >> 8) & 0xFF) ? "(Fast/Non-ISA)" : "(Standard/ISA)");
}

int main(int argc, char **argv) {
    struct pci_access *pacc;
    struct pci_dev *dev;
    
    if (geteuid() != 0) {
        printf("This program requires root privileges\n");
        return 1;
    }
    
    // Initialize PCI library
    pacc = pci_alloc();
    pci_init(pacc);
    pci_scan_bus(pacc);
    
    // Find IT8888
    for (dev = pacc->devices; dev; dev = dev->next) {
        if (dev->vendor_id == 0x1283 && dev->device_id == 0x8888) {
            printf("Found IT8888 at %02x:%02x.%d\n",
                   dev->bus, dev->dev, dev->func);
            
            if (argc > 1 && strcmp(argv[1], "soundblaster") == 0) {
                setup_dma_sound_blaster(dev);
            } else {
                setup_dma_for_isa_cards(dev);
            }
            
            display_dma_status(dev);
            return 0;
        }
    }
    
    printf("IT8888 not found\n");
    return 1;
}
```

### **Shell Script with setpci for DMA Configuration**
```bash
#!/bin/bash
# it8888_dma_setup.sh - Configure IT8888 DMA from command line

IT8888_DEV=$(lspci -d 1283:8888 | cut -d' ' -f1)

if [ -z "$IT8888_DEV" ]; then
    echo "IT8888 not found"
    exit 1
fi

echo "Configuring IT8888 DMA at $IT8888_DEV"

# Configure DDMA Channels 0 & 1 (Cfg_40h)
# Channel 0: Base 0x1000, extended addr, enabled
# Channel 1: Base 0x2000, extended addr, enabled  
setpci -s $IT8888_DEV 40.l=02081009

# Configure DDMA Channels 2 & 3 (Cfg_44h)
# Channel 2: Base 0x3000, extended addr, enabled
# Channel 3: Base 0x4000, extended addr, enabled
setpci -s $IT8888_DEV 44.l=04083009

# Configure Channel 5, Type-F, PPDMA (Cfg_48h) 
# PPDMA enabled, Channel 5: Base 0x5000, extended addr, enabled
setpci -s $IT8888_DEV 48.l=050800FF

# Configure DDMA Channels 6 & 7 (Cfg_4Ch)
# Channel 6: Base 0x6000, extended addr, enabled
# Channel 7: Base 0x7000, extended addr, enabled
setpci -s $IT8888_DEV 4c.l=07086009

echo "IT8888 DMA configuration complete"

# Display status
echo "DMA Status:"
echo "Cfg_40h: $(setpci -s $IT8888_DEV 40.l)"
echo "Cfg_44h: $(setpci -s $IT8888_DEV 44.l)" 
echo "Cfg_48h: $(setpci -s $IT8888_DEV 48.l)"
echo "Cfg_4Ch: $(setpci -s $IT8888_DEV 4c.l)"
```

### **Build and Usage Instructions**
```makefile
# Enhanced Makefile
KDIR := /lib/modules/$(shell uname -r)/build

# Kernel module targets
kernel-module:
	make -C $(KDIR) M=$(PWD) modules

install-module: kernel-module
	sudo insmod it8888.ko
	dmesg | tail -10

# User-space targets  
userspace-dma: it8888_dma_config.c
	gcc -o it8888_dma_config it8888_dma_config.c -lpci
	
userspace-basic: it8888_config.c
	gcc -o it8888_config it8888_config.c -lpci

# Installation and testing
install-userspace: userspace-dma userspace-basic
	sudo ./it8888_config
	sudo ./it8888_dma_config

test-soundblaster: userspace-dma
	sudo ./it8888_dma_config soundblaster

# Shell script setup
shell-setup:
	chmod +x it8888_setup.sh it8888_dma_setup.sh
	sudo ./it8888_setup.sh
	sudo ./it8888_dma_setup.sh

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f it8888_config it8888_dma_config

.PHONY: all clean kernel-module install-module userspace-dma userspace-basic
```
