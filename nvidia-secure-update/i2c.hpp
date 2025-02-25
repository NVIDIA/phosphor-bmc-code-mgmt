#pragma once

#include "i2c_interface.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{

class I2CDevice : public I2CInterface
{
  private:
    I2CDevice() = delete;

    /** @brief Constructor
     *
     * Construct I2CDevice object from the bus id and device address
     *
     * Automatically opens the I2CDevice if initialState is OPEN.
     *
     * @param[in] busId - The i2c bus ID
     * @param[in] devAddr - The device address of the I2C device
     * @param[in] initialState - Initial state of the I2CDevice object
     */
    explicit I2CDevice(uint8_t busId, uint8_t devAddr, bool useCustom = false,
                       InitialState initialState = InitialState::OPEN) :
        busId(busId),
        devAddr(devAddr), customRW(useCustom),
        busStr("/dev/i2c-" + std::to_string(busId))
    {
        if (initialState == InitialState::OPEN)
        {
            open();
        }
    }

    /** @brief Invalid file descriptor */
    static constexpr int INVALID_FD = -1;

    /** @brief Empty adapter functionality value with no bit flags set */
    static constexpr unsigned long NO_FUNCS = 0;

    /** @brief The I2C bus ID */
    uint8_t busId;

    /** @brief The i2c device address in the bus */
    uint8_t devAddr;

    bool customRW;

    /** @brief The file descriptor of the opened i2c device */
    int fd = INVALID_FD;

    /** @brief The i2c bus path in /dev */
    std::string busStr;

    /** @brief Cached I2C adapter functionality value */
    unsigned long cachedFuncs = NO_FUNCS;

    /** @brief Check that device interface is open
     *
     * @throw I2CException if device is not open
     */
    void checkIsOpen() const
    {
        if (!isOpen())
        {
            throw I2CException("Device not open", busStr, devAddr);
        }
    }

    /** @brief Close device without throwing an exception if an error occurs */
    void closeWithoutException() noexcept
    {
        try
        {
            close();
        }
        catch (...)
        {}
    }

  public:
    /** @copydoc I2CInterface::~I2CInterface() */
    ~I2CDevice()
    {
        if (isOpen())
        {
            // Note: destructors must not throw exceptions
            closeWithoutException();
        }
    }

    /** @copydoc I2CInterface::open() */
    void open();

    /** @copydoc I2CInterface::isOpen() */
    bool isOpen() const
    {
        return (fd != INVALID_FD);
    }

    /** @copydoc I2CInterface::close() */
    void close();

    void readCustom(uint8_t addr, uint8_t& size, uint8_t* result) override;

    void writeCustom(uint8_t addr, uint8_t size, uint8_t* data) override;

    /** @brief Create an I2CInterface instance
     *
     * Automatically opens the I2CInterface if initialState is OPEN.
     *
     * @param[in] busId - The i2c bus ID
     * @param[in] devAddr - The device address of the i2c
     * @param[in] initialState - Initial state of the I2CInterface object
     *
     * @return The unique_ptr holding the I2CInterface
     */
    static std::unique_ptr<I2CInterface>
        create(uint8_t busId, uint8_t devAddr, bool useCustom = false,
               InitialState initialState = InitialState::OPEN);
};

} // namespace updater
} // namespace software
} // namespace phosphor
