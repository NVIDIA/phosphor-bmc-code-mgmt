#include "config.h"

#include "i2c_comm_lib.hpp"

#include <CLI/CLI.hpp>
#include <filesystem>
#include <fstream>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

using namespace phosphor::logging;

using namespace phosphor::software::updater;

using sdbusplus::exception::SdBusError;

namespace match_rules = sdbusplus::bus::match::rules;

namespace phosphor
{
namespace NvidiaCopyImage
{


static constexpr auto busIdentifier = CEC_BUS_IDENTIFIER;

static constexpr auto deviceAddrress = CEC_DEVICE_ADDRESS;

} // namespace NvidiaCopyImage
} // namespace phosphor

int main(int argc, char** argv)
{
    I2CCommLib deviceLayer(phosphor::NvidiaCopyImage::busIdentifier,
                           phosphor::NvidiaCopyImage::deviceAddrress);
    std::string fileName = {};
    uint32_t fileSize{0};

    CLI::App app{"Nvidia Secure Image Copier "};

    // Add an input option
    app.add_option("-f", fileName, "Filename of f/w image")->required();
    app.add_option("-s", fileSize, "Actual size of the f/w image")->required();


    // Parse input parameter
    try
    {
        app.parse(argc, argv);
    }
    catch (CLI::Error& e)
    {
        log<level::ERR>("copy_image_service EXECPTION",
                        entry("EXCEPTION=%s", e.what()));
    }


    try
    {
        deviceLayer.SendImageToCEC(fileName, fileSize);

    }
    catch (const std::exception& e)
    {
        log<level::ERR>(
            "copy_image_service - transfer image failed.",
            entry("EXCEPTION=%s", e.what()));

        return -1;
    }

    return 0;
}
