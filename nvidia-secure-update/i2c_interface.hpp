#pragma once

#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace phosphor
{
namespace software
{
namespace updater
{

class I2CException : public std::exception
{
  public:
    explicit I2CException(const std::string& info, const std::string& bus,
                          uint8_t addr, int errorCode = 0) :
        bus(bus),
        addr(addr), errorCode(errorCode)
    {
        std::stringstream ss;
        ss << "I2CException: " << info << ": bus " << bus << ", addr 0x"
           << std::hex << static_cast<int>(addr);
        if (errorCode != 0)
        {
            ss << std::dec << ", errno " << errorCode << ": "
               << strerror(errorCode);
        }
        errStr = ss.str();
    }
    virtual ~I2CException() = default;

    const char* what() const noexcept override
    {
        return errStr.c_str();
    }
    std::string bus;
    uint8_t addr;
    int errorCode;
    std::string errStr;
};

class I2CInterface
{
  public:
    /** @brief Destructor
     *
     * Closes the I2C interface to the device if necessary.
     */
    virtual ~I2CInterface() = default;

    /** @brief Initial state when an I2CInterface object is created */
    enum class InitialState
    {
        OPEN,
        CLOSED
    };

    /** @brief Open the I2C interface to the device
     *
     * Throws an I2CException if the interface is already open.  See isOpen().
     *
     * @throw I2CException on error
     */
    virtual void open() = 0;

    /** @brief Indicates whether the I2C interface to the device is open
     *
     * @return true if interface is open, false otherwise
     */
    virtual bool isOpen() const = 0;

    /** @brief Close the I2C interface to the device
     *
     * The interface can later be re-opened by calling open().
     *
     * Note that the destructor will automatically close the I2C interface if
     * necessary.
     *
     * Throws an I2CException if the interface is not open.  See isOpen().
     *
     * @throw I2CException on error
     */
    virtual void close() = 0;

    virtual void readCustom(uint8_t addr, uint8_t& size, uint8_t* result) = 0;

    virtual void writeCustom(uint8_t addr, uint8_t size, uint8_t* data) = 0;
};

/** @brief Create an I2CInterface instance
 *
 * Automatically opens the I2CInterface if initialState is OPEN.
 *
 * @param[in] busId - The i2c bus ID
 * @param[in] devAddr - The device address of the i2c
/bin/bash: /reg: No such file or directory
 * @return The unique_ptr holding the I2CInterface
 */
std::unique_ptr<I2CInterface> create(
    uint8_t busId, uint8_t devAddr, bool useCustom = false,
    I2CInterface::InitialState initialState = I2CInterface::InitialState::OPEN);

} // namespace updater
} // namespace software
} // namespace phosphor
