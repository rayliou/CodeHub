#include <cstdint>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <cstring>
#include <iomanip>
#include <cmath>

#define I2C_BUS "/dev/i2c-7"
#define I2C_ADDRESS 0x10
#define GAIN 0.0576  // Assume this is your gain factor from Python code

// I2C read/write block function using ioctl and i2c_smbus_ioctl_data
int i2c_rdwr_block(int file, uint8_t command, uint8_t read_write, uint8_t length, unsigned char* buffer) {
    struct i2c_smbus_ioctl_data ioctl_data;
    union i2c_smbus_data smbus_data;

    if (length > I2C_SMBUS_BLOCK_MAX) {
        std::cerr << "Requested Length is greater than the maximum specified" << std::endl;
        return -1;
    }

    smbus_data.block[0] = length;
    if (read_write == I2C_SMBUS_WRITE) {
        for (int i = 0; i < length; i++) {
            smbus_data.block[i + 1] = buffer[i];
        }
    }

    ioctl_data.read_write = read_write;
    ioctl_data.command = command;
    ioctl_data.size = I2C_SMBUS_I2C_BLOCK_DATA;
    ioctl_data.data = &smbus_data;

    if (ioctl(file, I2C_SMBUS, &ioctl_data) < 0) {
        std::cerr << "Failed to access I2C bus via ioctl" << std::endl;
        return -1;
    }

    if (read_write == I2C_SMBUS_READ) {
        for (int i = 0; i < length; i++) {
            buffer[i] = smbus_data.block[i + 1];
        }
    }

    return 0;
}

// I2C read word function using ioctl and i2c_smbus_ioctl_data
int i2c_read_word(int file, uint8_t command) {
    struct i2c_smbus_ioctl_data ioctl_data;
    union i2c_smbus_data smbus_data;

    ioctl_data.read_write = I2C_SMBUS_READ;
    ioctl_data.command = command;
    ioctl_data.size = I2C_SMBUS_WORD_DATA;
    ioctl_data.data = &smbus_data;

    if (ioctl(file, I2C_SMBUS, &ioctl_data) < 0) {
        std::cerr << "Failed to read word from I2C bus via ioctl" << std::endl;
        return -1;
    }

    return smbus_data.word;
}

int main() {
    // Open the I2C bus
    int file = open(I2C_BUS, O_RDWR);
    if (file < 0) {
        std::cerr << "Failed to open I2C bus" << std::endl;
        return -1;
    }

    // Set the I2C slave address
    if (ioctl(file, I2C_SLAVE, I2C_ADDRESS) < 0) {
        std::cerr << "Failed to set I2C slave address" << std::endl;
        close(file);
        return -1;
    }

    // Prepare data for block write (equivalent to bus.write_i2c_block_data)
    unsigned char block_data_0[] = {0xC0, 0x10};
    unsigned char block_data_1[] = {0x00, 0x00};
    unsigned char block_data_2[] = {0x00, 0x00};
    unsigned char block_data_3[] = {0x00, 0x00};

    // Write block data to multiple registers
    i2c_rdwr_block(file, 0x00, I2C_SMBUS_WRITE, 2, block_data_0);
    i2c_rdwr_block(file, 0x01, I2C_SMBUS_WRITE, 2, block_data_1);
    i2c_rdwr_block(file, 0x02, I2C_SMBUS_WRITE, 2, block_data_2);
    i2c_rdwr_block(file, 0x03, I2C_SMBUS_WRITE, 2, block_data_3);

    // Read a word from register 0x04 (equivalent to bus.read_word_data)
    int16_t light = i2c_read_word(file, 0x04);

    // Apply GAIN and round the value
    auto r = light * GAIN;

    // Print the light value
    std::cout << "Light Level: " << r << std::endl;

    // Close the I2C file descriptor
    close(file);

    return 0;
}

