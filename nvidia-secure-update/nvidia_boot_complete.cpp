#include "config.h"

#include "i2c_comm_lib.hpp"
#include "watch.hpp"

#include <CLI/CLI.hpp>
#include <com/nvidia/Secureboot/Cec/server.hpp>
#include <nlohmann/json.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>

#include <chrono>
#include <map>

using namespace phosphor::logging;

using namespace phosphor::software::updater;

namespace phosphor
{
namespace NvidiaBootComplete
{

static constexpr auto busIdentifier = CEC_BUS_IDENTIFIER;

static constexpr auto deviceAddrress = CEC_DEVICE_ADDRESS;

constexpr uint8_t sleepInSeconds{100};

using namespace std::chrono;

} // namespace NvidiaBootComplete
} // namespace phosphor

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    I2CCommLib deviceLayer(phosphor::NvidiaBootComplete::busIdentifier,
                           phosphor::NvidiaBootComplete::deviceAddrress);
    uint8_t retVal = static_cast<uint8_t>(I2CCommLib::CommandStatus::UNKNOWN);
    uint8_t count{0};
    uint8_t maxRetry{5};

    CLI::App app{"Nvidia Send Boot Complete Service"};

    while (count < maxRetry)
    {
        try
        {
            deviceLayer.SendBootComplete();

            constexpr auto setWaitForComplete = std::chrono::milliseconds(
                phosphor::NvidiaBootComplete::sleepInSeconds);

            std::this_thread::sleep_for(setWaitForComplete);

            retVal = deviceLayer.GetLastCmdStatus();

            if (retVal !=
                static_cast<uint8_t>(I2CCommLib::CommandStatus::SUCCESS))
            {
                log<level::ERR>(
                    "secure_monitor_service - SendBootComplete command failed.",
                    entry("ERR=0x%x", retVal));
                count++;
                continue;
            }

            break;
        }
        catch (const std::exception& e)
        {
            log<level::ERR>(
                "secure_monitor_service - SendBootComplete command failed.",
                entry("EXCEPTION=%s", e.what()));
            count++;
        }
    }

    return 0;
}
