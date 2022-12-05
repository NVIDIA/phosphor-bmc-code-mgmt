#include "i2c.hpp"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>

extern "C" {
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
}

namespace phosphor
{
namespace software
{
namespace updater
{

void I2CDevice::open()
{
    if (isOpen())
    {
        throw I2CException("Device already open", busStr, devAddr);
    }

    fd = ::open(busStr.c_str(), O_RDWR);
    if (fd == -1)
    {
        throw I2CException("Failed to open", busStr, devAddr, errno);
    }

    if (!customRW && ioctl(fd, I2C_SLAVE, devAddr) < 0)
    {
        // Close device since setting slave address failed
        closeWithoutException();

        throw I2CException("Failed to set I2C_SLAVE", busStr, devAddr, errno);
    }
}

void I2CDevice::close()
{
    checkIsOpen();
    if (::close(fd) == -1)
    {
        throw I2CException("Failed to close", busStr, devAddr, errno);
    }
    fd = INVALID_FD;
    cachedFuncs = NO_FUNCS;
}

// Read the given I2C slave device's register and return the read value in
// `*result`:
void I2CDevice::readCustom(uint8_t reg, uint8_t& size, uint8_t* result)
{
    int retVal = 0;
    uint8_t devRegister[2];
    struct i2c_msg msgs[2];
    struct i2c_rdwr_ioctl_data msgset[1];
    uint8_t reg_msb = (reg & 0xff00) >> 8;
    uint8_t reg_lsb = (reg & 0xff);
    uint8_t devRegLen = 2;

    devRegister[0] = reg_msb;
    devRegister[1] = reg_lsb;

    msgs[0].addr = devAddr;
    msgs[0].flags = 0;
    msgs[0].len = devRegLen;
    msgs[0].buf = devRegister;

    msgs[1].addr = devAddr;
    msgs[1].flags = I2C_M_RD | I2C_M_NOSTART;
    msgs[1].len = size;
    msgs[1].buf = result;

    msgset[0].msgs = msgs;
    msgset[0].nmsgs = 2;

    retVal = ioctl(fd, I2C_RDWR, &msgset);

    if (retVal < 0)
    {
        throw I2CException("IOCTL: Failed to write block data", busStr, reg,
                           errno);
    }
}

// Write to an I2C slave device's register:
void I2CDevice::writeCustom(uint8_t reg, uint8_t size, uint8_t* data)
{
    int retVal = 0;

    struct i2c_msg msgs[1];
    struct i2c_rdwr_ioctl_data msgset[1];

    msgs[0].addr = devAddr;
    msgs[0].flags = 0;
    msgs[0].len = size;
    msgs[0].buf = data;

    msgset[0].msgs = msgs;
    msgset[0].nmsgs = 1;

    retVal = ioctl(fd, I2C_RDWR, &msgset);

    if (retVal < 0)
    {
        throw I2CException("IOCTL: Failed to write block data", busStr, reg,
                           retVal);
    }
}

std::unique_ptr<I2CInterface> I2CDevice::create(uint8_t busId, uint8_t devAddr,
                                                bool useCustom,
                                                InitialState initialState)
{
    std::unique_ptr<I2CDevice> dev(
        new I2CDevice(busId, devAddr, useCustom, initialState));
    return dev;
}

std::unique_ptr<I2CInterface> create(uint8_t busId, uint8_t devAddr,
                                     bool useCustom,
                                     I2CInterface::InitialState initialState)
{
    return I2CDevice::create(busId, devAddr, useCustom, initialState);
}
} // namespace updater
} // namespace software
} // namespace phosphor
