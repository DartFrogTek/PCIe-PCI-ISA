#include "intel_hex_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// Configuration
#define I2C_PORT i2c1
#define I2C_SDA 2
#define I2C_SCL 3
#define I2C_FREQ 100000

#define EEPROM_ADDR 0x50
#define EEPROM_SIZE 256
#define PAGE_SIZE 8

// Function prototypes
bool scan_i2c_devices(void);
bool write_page(uint8_t start_addr, uint8_t *data, int len);
bool read_sequential(uint8_t start_addr, uint8_t *buffer, int num_bytes);
bool write_data_to_eeprom(void);
bool verify_eeprom_data(void);
void dump_eeprom_contents(uint8_t start_addr, int length);

// Initialize I2C
void init_i2c(void) {
    i2c_init(I2C_PORT, I2C_FREQ);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
}

bool scan_i2c_devices(void) {
    printf("Scanning I2C devices...\n");
    bool found = false;
    
    for (int addr = 0; addr < 128; addr++) {
        uint8_t data;
        int ret = i2c_read_blocking(I2C_PORT, addr, &data, 1, false);
        if (ret >= 0) {
            printf("I2C device found at address 0x%02X\n", addr);
            if (addr == EEPROM_ADDR) {
                found = true;
            }
        }
    }
    
    if (found) {
        printf("AT24C02 found at address 0x%02X!\n", EEPROM_ADDR);
        return true;
    } else {
        printf("AT24C02 not found! Check connections and address.\n");
        return false;
    }
}

bool write_page(uint8_t start_addr, uint8_t *data, int len) {
    if (start_addr >= EEPROM_SIZE) {
        printf("Start address %d out of range (0-%d)\n", start_addr, EEPROM_SIZE-1);
        return false;
    }
    
    uint8_t end_addr = start_addr + len - 1;
    if (end_addr >= EEPROM_SIZE) {
        printf("End address %d out of range (0-%d)\n", end_addr, EEPROM_SIZE-1);
        return false;
    }
    
    // Check if data crosses page boundary
    if ((start_addr / PAGE_SIZE) != (end_addr / PAGE_SIZE)) {
        // Split into multiple writes
        uint8_t first_page_end = ((start_addr / PAGE_SIZE) + 1) * PAGE_SIZE - 1;
        uint8_t first_chunk_size = first_page_end - start_addr + 1;
        
        // Write first chunk
        uint8_t buffer[PAGE_SIZE + 1];
        buffer[0] = start_addr;
        memcpy(&buffer[1], data, first_chunk_size);
        
        int ret = i2c_write_blocking(I2C_PORT, EEPROM_ADDR, buffer, first_chunk_size + 1, false);
        if (ret < 0) return false;
        sleep_ms(5);
        
        // Write remaining chunks
        int pos = first_chunk_size;
        uint8_t current_addr = first_page_end + 1;
        
        while (pos < len) {
            uint8_t current_page_end = ((current_addr / PAGE_SIZE) + 1) * PAGE_SIZE - 1;
            uint8_t chunk_size = (current_page_end - current_addr + 1 < len - pos) ? 
                                current_page_end - current_addr + 1 : len - pos;
            
            buffer[0] = current_addr;
            memcpy(&buffer[1], &data[pos], chunk_size);
            
            ret = i2c_write_blocking(I2C_PORT, EEPROM_ADDR, buffer, chunk_size + 1, false);
            if (ret < 0) return false;
            sleep_ms(5);
            
            pos += chunk_size;
            current_addr += chunk_size;
        }
    } else {
        // No page boundary crossing
        uint8_t buffer[PAGE_SIZE + 1];
        buffer[0] = start_addr;
        memcpy(&buffer[1], data, len);
        
        int ret = i2c_write_blocking(I2C_PORT, EEPROM_ADDR, buffer, len + 1, false);
        if (ret < 0) return false;
        sleep_ms(5);
    }
    
    return true;
}

bool read_sequential(uint8_t start_addr, uint8_t *buffer, int num_bytes) {
    if (start_addr >= EEPROM_SIZE) {
        printf("Start address %d out of range (0-%d)\n", start_addr, EEPROM_SIZE-1);
        return false;
    }
    
    if (start_addr + num_bytes > EEPROM_SIZE) {
        printf("Read would exceed EEPROM size\n");
        return false;
    }
    
    i2c_write_blocking(I2C_PORT, EEPROM_ADDR, &start_addr, 1, true);
    int ret = i2c_read_blocking(I2C_PORT, EEPROM_ADDR, buffer, num_bytes, false);
    return ret >= 0;
}

bool write_data_to_eeprom(void) {
    printf("Writing %d bytes to EEPROM...\n", EEPROM_SIZE);
    
    // Write data in pages
    for (int addr = 0; addr < EEPROM_SIZE; addr += PAGE_SIZE) {
        int page_size = (EEPROM_SIZE - addr < PAGE_SIZE) ? EEPROM_SIZE - addr : PAGE_SIZE;
        
        printf("Writing page at address 0x%02X: ", addr);
        for (int j = 0; j < page_size; j++) {
            printf("0x%02X ", ucDataBlock[addr + j]);
        }
        printf("\n");
        
        if (!write_page(addr, &ucDataBlock[addr], page_size)) {
            printf("Error writing page at address 0x%02X\n", addr);
            return false;
        }
    }
    
    printf("EEPROM write complete!\n");
    return true;
}

bool verify_eeprom_data(void) {
    printf("Verifying %d bytes in EEPROM...\n", EEPROM_SIZE);
    
    uint8_t read_buffer[16];
    int mismatches = 0;
    
    for (int addr = 0; addr < EEPROM_SIZE; addr += 16) {
        int chunk_size = (EEPROM_SIZE - addr < 16) ? EEPROM_SIZE - addr : 16;
        
        if (!read_sequential(addr, read_buffer, chunk_size)) {
            printf("Error reading EEPROM at address 0x%02X\n", addr);
            return false;
        }
        
        for (int i = 0; i < chunk_size; i++) {
            if (read_buffer[i] != ucDataBlock[addr + i]) {
                printf("Verification failed at address 0x%02X: expected 0x%02X, got 0x%02X\n", 
                       addr + i, ucDataBlock[addr + i], read_buffer[i]);
                mismatches++;
                if (mismatches > 10) {
                    printf("Too many mismatches, stopping verification\n");
                    return false;
                }
            }
        }
    }
    
    if (mismatches == 0) {
        printf("EEPROM verification successful!\n");
        return true;
    } else {
        printf("EEPROM verification failed with %d mismatches\n", mismatches);
        return false;
    }
}

void dump_eeprom_contents(uint8_t start_addr, int length) {
    if (start_addr >= EEPROM_SIZE) {
        printf("Invalid start address: %d\n", start_addr);
        return;
    }
    
    if (start_addr + length > EEPROM_SIZE) {
        length = EEPROM_SIZE - start_addr;
    }
    
    printf("EEPROM contents from 0x%02X to 0x%02X:\n", start_addr, start_addr + length - 1);
    
    uint8_t buffer[16];
    for (int base_addr = start_addr; base_addr < start_addr + length; base_addr += 16) {
        int read_len = (start_addr + length - base_addr < 16) ? start_addr + length - base_addr : 16;
        
        if (!read_sequential(base_addr, buffer, read_len)) {
            printf("Error reading EEPROM\n");
            return;
        }
        
        printf("0x%04X: ", base_addr);
        
        // Print hex values
        for (int i = 0; i < read_len; i++) {
            printf("%02X ", buffer[i]);
            if (i == 7) printf(" ");
        }
        
        // Pad with spaces
        for (int i = read_len; i < 16; i++) {
            printf("   ");
            if (i == 7) printf(" ");
        }
        
        // Print ASCII
        printf(" |");
        for (int i = 0; i < read_len; i++) {
            if (buffer[i] >= 32 && buffer[i] <= 126) {
                printf("%c", buffer[i]);
            } else {
                printf(".");
            }
        }
        printf("|\n");
    }
}

int main() {
    stdio_init_all();
    
    // Wait a bit for USB serial to be ready
    sleep_ms(2000);
    
    printf("==================================================\n");
    printf("AT24C02 EEPROM Automatic Programmer\n");
    printf("Programming 256 bytes from ucDataBlock array\n");
    printf("==================================================\n");
    
    init_i2c();
    
    if (!scan_i2c_devices()) {
        printf("FAILED: EEPROM not detected!\n");
        while (1) {
            sleep_ms(1000);
        }
    }
    
    printf("\nData to be written (first 32 bytes):\n");
    for (int i = 0; i < 32; i++) {
        if (i % 16 == 0) printf("0x%04X: ", i);
        printf("%02X ", ucDataBlock[i]);
        if (i % 16 == 15) printf("\n");
    }
    
    printf("\nProgramming EEPROM...\n");
    if (!write_data_to_eeprom()) {
        printf("FAILED: Could not write to EEPROM!\n");
        while (1) {
            sleep_ms(1000);
        }
    }
    
    printf("\nVerifying EEPROM...\n");
    if (!verify_eeprom_data()) {
        printf("FAILED: Verification failed!\n");
        while (1) {
            sleep_ms(1000);
        }
    }
    
    printf("\n==================================================\n");
    printf("SUCCESS: EEPROM programmed and verified!\n");
    printf("==================================================\n");
    
    printf("\nFinal EEPROM contents (first 64 bytes):\n");
    dump_eeprom_contents(0, 64);
    
    printf("\nProgramming complete. Safe to disconnect.\n");
    
    // Blink LED to indicate success
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    while (1) {
        gpio_put(LED_PIN, 1);
        sleep_ms(500);
        gpio_put(LED_PIN, 0);
        sleep_ms(500);
    }
    
    return 0;
}