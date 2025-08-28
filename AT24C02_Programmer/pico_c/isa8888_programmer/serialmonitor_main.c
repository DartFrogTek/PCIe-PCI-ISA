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

#define MAX_HEX_LINE 64
#define MAX_MEMORY_DATA 256

typedef struct {
    uint8_t byte_count;
    uint16_t address;
    uint8_t record_type;
    uint8_t data[16];
    uint8_t checksum;
} hex_record_t;

typedef struct {
    uint16_t address;
    uint8_t value;
} memory_entry_t;

// Global variables
memory_entry_t memory_data[MAX_MEMORY_DATA];
int memory_data_count = 0;

// Function prototypes
bool scan_i2c_devices(void);
bool write_byte(uint8_t addr, uint8_t data);
uint8_t read_byte(uint8_t addr);
bool write_page(uint8_t start_addr, uint8_t *data, int len);
bool read_sequential(uint8_t start_addr, uint8_t *buffer, int num_bytes);
bool parse_hex_line(const char *line, hex_record_t *record);
bool load_from_hex_string(const char *hex_string);
bool write_memory_to_eeprom(void);
bool verify_eeprom_data(void);
void dump_eeprom_contents(uint8_t start_addr, int length);
void main_menu(void);
char *get_line(char *buffer, int size);
int hex_char_to_int(char c);

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

bool write_byte(uint8_t addr, uint8_t data) {
    if (addr >= EEPROM_SIZE) {
        printf("Address %d out of range (0-%d)\n", addr, EEPROM_SIZE-1);
        return false;
    }
    
    uint8_t buffer[2] = {addr, data};
    int ret = i2c_write_blocking(I2C_PORT, EEPROM_ADDR, buffer, 2, false);
    if (ret < 0) {
        return false;
    }
    
    sleep_ms(5); // Wait for write cycle
    return true;
}

uint8_t read_byte(uint8_t addr) {
    if (addr >= EEPROM_SIZE) {
        printf("Address %d out of range (0-%d)\n", addr, EEPROM_SIZE-1);
        return 0;
    }
    
    uint8_t data;
    i2c_write_blocking(I2C_PORT, EEPROM_ADDR, &addr, 1, true);
    i2c_read_blocking(I2C_PORT, EEPROM_ADDR, &data, 1, false);
    return data;
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

int hex_char_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

bool parse_hex_line(const char *line, hex_record_t *record) {
    if (line[0] != ':') {
        return false;
    }
    
    int len = strlen(line);
    if (len < 11) { // Minimum: :LLAAAATT[DD...]CC
        printf("Line too short: %s\n", line);
        return false;
    }
    
    // Parse byte count
    int h1 = hex_char_to_int(line[1]);
    int h2 = hex_char_to_int(line[2]);
    if (h1 < 0 || h2 < 0) return false;
    record->byte_count = (h1 << 4) | h2;
    
    // Parse address
    h1 = hex_char_to_int(line[3]);
    h2 = hex_char_to_int(line[4]);
    int h3 = hex_char_to_int(line[5]);
    int h4 = hex_char_to_int(line[6]);
    if (h1 < 0 || h2 < 0 || h3 < 0 || h4 < 0) return false;
    record->address = (h1 << 12) | (h2 << 8) | (h3 << 4) | h4;
    
    // Parse record type
    h1 = hex_char_to_int(line[7]);
    h2 = hex_char_to_int(line[8]);
    if (h1 < 0 || h2 < 0) return false;
    record->record_type = (h1 << 4) | h2;
    
    // Parse data
    for (int i = 0; i < record->byte_count; i++) {
        int pos = 9 + i * 2;
        if (pos + 1 >= len) return false;
        h1 = hex_char_to_int(line[pos]);
        h2 = hex_char_to_int(line[pos + 1]);
        if (h1 < 0 || h2 < 0) return false;
        record->data[i] = (h1 << 4) | h2;
    }
    
    // Parse checksum
    int pos = 9 + record->byte_count * 2;
    if (pos + 1 >= len) return false;
    h1 = hex_char_to_int(line[pos]);
    h2 = hex_char_to_int(line[pos + 1]);
    if (h1 < 0 || h2 < 0) return false;
    record->checksum = (h1 << 4) | h2;
    
    // Verify checksum
    uint8_t calc_sum = record->byte_count + (record->address >> 8) + (record->address & 0xFF) + record->record_type;
    for (int i = 0; i < record->byte_count; i++) {
        calc_sum += record->data[i];
    }
    uint8_t calc_checksum = (0x100 - calc_sum) & 0xFF;
    
    if (calc_checksum != record->checksum) {
        printf("Checksum error in line: %s\n", line);
        printf("Calculated: 0x%02X, Expected: 0x%02X\n", calc_checksum, record->checksum);
        return false;
    }
    
    return true;
}

bool load_from_hex_string(const char *hex_string) {
    memory_data_count = 0;
    
    char line[MAX_HEX_LINE];
    int line_pos = 0;
    int str_pos = 0;
    
    while (hex_string[str_pos] != '\0') {
        if (hex_string[str_pos] == '\n' || hex_string[str_pos] == '\r') {
            if (line_pos > 0) {
                line[line_pos] = '\0';
                
                hex_record_t record;
                if (parse_hex_line(line, &record)) {
                    if (record.record_type == 0) { // Data record
                        for (int i = 0; i < record.byte_count; i++) {
                            if (memory_data_count >= MAX_MEMORY_DATA) {
                                printf("Memory data buffer full!\n");
                                return false;
                            }
                            memory_data[memory_data_count].address = record.address + i;
                            memory_data[memory_data_count].value = record.data[i];
                            memory_data_count++;
                        }
                    } else if (record.record_type == 1) { // End of file
                        break;
                    }
                }
                line_pos = 0;
            }
        } else {
            if (line_pos < MAX_HEX_LINE - 1) {
                line[line_pos++] = hex_string[str_pos];
            }
        }
        str_pos++;
    }
    
    return memory_data_count > 0;
}

bool write_memory_to_eeprom(void) {
    if (memory_data_count == 0) {
        printf("No data to write to EEPROM\n");
        return false;
    }
    
    printf("Writing %d bytes to EEPROM...\n", memory_data_count);
    
    // Sort memory data by address (simple bubble sort)
    for (int i = 0; i < memory_data_count - 1; i++) {
        for (int j = 0; j < memory_data_count - i - 1; j++) {
            if (memory_data[j].address > memory_data[j + 1].address) {
                memory_entry_t temp = memory_data[j];
                memory_data[j] = memory_data[j + 1];
                memory_data[j + 1] = temp;
            }
        }
    }
    
    // Group consecutive addresses for page writes
    int i = 0;
    while (i < memory_data_count) {
        uint8_t page_data[PAGE_SIZE];
        uint16_t start_addr = memory_data[i].address;
        int page_len = 1;
        page_data[0] = memory_data[i].value;
        
        // Check for consecutive addresses on the same page
        while (i + page_len < memory_data_count && 
               memory_data[i + page_len].address == start_addr + page_len &&
               (start_addr / PAGE_SIZE) == ((start_addr + page_len) / PAGE_SIZE) &&
               page_len < PAGE_SIZE) {
            page_data[page_len] = memory_data[i + page_len].value;
            page_len++;
        }
        
        printf("Writing page at address 0x%02X: ", start_addr);
        for (int j = 0; j < page_len; j++) {
            printf("0x%02X ", page_data[j]);
        }
        printf("\n");
        
        if (!write_page(start_addr, page_data, page_len)) {
            printf("Error writing page\n");
            return false;
        }
        
        i += page_len;
    }
    
    printf("EEPROM write complete!\n");
    return true;
}

bool verify_eeprom_data(void) {
    if (memory_data_count == 0) {
        printf("No data to verify\n");
        return false;
    }
    
    printf("Verifying %d bytes in EEPROM...\n", memory_data_count);
    
    for (int i = 0; i < memory_data_count; i++) {
        uint16_t addr = memory_data[i].address;
        uint8_t expected = memory_data[i].value;
        
        if (addr >= EEPROM_SIZE) {
            printf("Skipping verification of address 0x%02X (out of range)\n", addr);
            continue;
        }
        
        uint8_t actual = read_byte(addr);
        if (actual != expected) {
            printf("Verification failed at address 0x%02X: expected 0x%02X, got 0x%02X\n", 
                   addr, expected, actual);
            return false;
        }
    }
    
    printf("EEPROM verification successful!\n");
    return true;
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

char *get_line(char *buffer, int size) {
    int i = 0;
    int c;
    
    // Clear the buffer first
    memset(buffer, 0, size);
    
    while (i < size - 1) {
        c = getchar_timeout_us(1000000); // 1 second timeout
        
        if (c == PICO_ERROR_TIMEOUT) {
            continue; // Keep trying
        }
        
        if (c == '\n' || c == '\r') {
            break;
        }
        
        if (c == EOF || c < 0) {
            if (i == 0) return NULL;
            break;
        }
        
        // Echo the character back for user feedback
        putchar(c);
        fflush(stdout);
        
        buffer[i++] = c;
    }
    
    buffer[i] = '\0';
    printf("\n"); // Add newline after input
    return buffer;
}

void main_menu(void) {
    char input[256];
    char choice[10];
    
    if (!scan_i2c_devices()) {
        return;
    }
    
    while (true) {
        printf("\nAT24C02 EEPROM Programming Tool\n");
        printf("-------------------------------\n");
        printf("1. Program from Intel HEX string\n");
        printf("2. Dump EEPROM contents\n");
        printf("3. Exit\n");
        printf("Enter your choice (1-3): ");
        fflush(stdout);  // Force output

        printf("DEBUG: About to read input...\n");
        if (!get_line(choice, sizeof(choice))) {
            printf("DEBUG: get_line returned NULL\n");
            continue;
        }
        printf("DEBUG: Read choice: '%s'\n", choice);
        
        if (strcmp(choice, "1") == 0) {
            printf("DEBUG: Entering hex input mode\n");
            printf("Enter Intel HEX data (paste multiple lines, end with a line containing only 'END'):\n");
            fflush(stdout);
            
            char hex_string[2048] = "";
            char line[MAX_HEX_LINE];
            
            while (true) {
                if (!get_line(line, sizeof(line))) {
                    break;
                }
                
                if (strcmp(line, "END") == 0) {
                    break;
                }
                
                strcat(hex_string, line);
                strcat(hex_string, "\n");
            }
            
            if (load_from_hex_string(hex_string)) {
                if (write_memory_to_eeprom()) {
                    verify_eeprom_data();
                }
            }
            
        } else if (strcmp(choice, "2") == 0) {
            printf("Enter start address (hex, default 00): ");
            if (!get_line(input, sizeof(input))) {
                input[0] = '\0';
            }
            
            uint8_t start_addr = 0;
            if (strlen(input) > 0) {
                start_addr = (uint8_t)strtol(input, NULL, 16);
            }
            
            printf("Enter length (decimal, default 256): ");
            if (!get_line(input, sizeof(input))) {
                input[0] = '\0';
            }
            
            int length = EEPROM_SIZE;
            if (strlen(input) > 0) {
                length = atoi(input);
            }
            
            dump_eeprom_contents(start_addr, length);
            
        } else if (strcmp(choice, "3") == 0) {
            printf("Exiting...\n");
            break;
        } else {
            printf("Invalid choice\n");
        }
    }
}

int main() {
    stdio_init_all();
    
    // Wait a bit for USB serial to be ready
    sleep_ms(2000);
    
    printf("AT24C02 EEPROM Programmer Starting...\n");
    
    init_i2c();
    main_menu();
    
    return 0;
}