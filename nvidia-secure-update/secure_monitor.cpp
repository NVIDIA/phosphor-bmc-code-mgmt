#include "config.h"

#include "i2c_comm_lib.hpp"
#include "version.hpp"
#include "watch.hpp"
#include "xyz/openbmc_project/Software/Version/server.hpp"

#include <CLI/CLI.hpp>
#include <chrono>
#include <com/nvidia/Secureboot/Cec/server.hpp>
#include <filesystem>
#include <fstream>
#include <gpiod.hpp>
#include <map>
#include <nlohmann/json.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/server.hpp>
#include <sdbusplus/server/manager.hpp>
#include <sdbusplus/server/object.hpp>
#include <sdbusplus/timer.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/source/event.hpp>
#include <sdeventplus/source/io.hpp>
#include <sdeventplus/source/signal.hpp>
#include <stdplus/signal.hpp>
#include <variant>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>

using namespace phosphor::logging;

using namespace phosphor::software::updater;

using sdbusplus::exception::SdBusError;

namespace MatchRules = sdbusplus::bus::match::rules;

namespace server = sdbusplus::xyz::openbmc_project::Software::server;

namespace phosphor
{
namespace NvidiaSecureUpdate
{

static sd_event* cecGpioEventLoop = nullptr;

static gpiod::line cecGpioLine;

static int cecGpioLineFd;

static constexpr auto cecGpioName = CEC_GPIO_LINE;

static constexpr auto busIdentifier = CEC_BUS_IDENTIFIER;

static constexpr auto deviceAddrress = CEC_DEVICE_ADDRESS;

std::unique_ptr<phosphor::Timer> checkTimer;

static constexpr uint8_t checkTimerExpiry{60};

static bool enabledGuard{false};

// in milliseconds
constexpr uint8_t sleepInSeconds{100};

sdbusplus::bus::bus bus = sdbusplus::bus::new_default();

using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

namespace fs = std::filesystem;

using CecInherit = sdbusplus::server::object::object<
    sdbusplus::com::nvidia::Secureboot::server::Cec>;

using DbusInterface = std::string;

using DbusProperty = std::string;

using Value = std::variant<bool, uint8_t, int16_t, uint16_t, int32_t, uint32_t,
                           int64_t, uint64_t, double, std::string>;

using PropertyMap = std::map<DbusProperty, Value>;

using DbusInterfaceMap = std::map<DbusInterface, PropertyMap>;

using ObjectValueTree =
    std::map<sdbusplus::message::object_path, DbusInterfaceMap>;

using Activation =
    sdbusplus::xyz::openbmc_project::Software::server::Activation;

using namespace std::chrono;

void ApplyRebootGaurd();

class CecImpl : public CecInherit
{
  public:
    CecImpl(sdbusplus::bus::bus& bus, const std::string& objPath) :
        CecInherit(bus, (objPath).c_str()){};

    bool getStatus()
    {
        return sdbusplus::com::nvidia::Secureboot::server::Cec::status();
    };

    bool setStatus(bool value)
    {
        return sdbusplus::com::nvidia::Secureboot::server::Cec::status(value);
    };
};

std::unique_ptr<CecImpl> cecIntManager;

void RebootBmc()
{

    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("nvidia-reboot.service", "replace");
    try
    {
        bus.call(method);
    }
    catch (const SdBusError& e)
    {
        log<level::ALERT>("Error in trying to reboot the BMC. "
                          "The BMC needs to be manually rebooted to complete "
                          "the image activation.");
        report<InternalFailure>();
    }
}

static int GpioInterruptHandler([[maybe_unused]]sd_event_source* s, [[maybe_unused]]int fd, 
		                [[maybe_unused]] uint32_t revents,[[maybe_unused]] void* userdata)
{
    try
    {
        uint8_t retVal =
            static_cast<uint8_t>(I2CCommLib::CECInterruptStatus::UNKNOWN);
        bool current{false};

        auto gpioLineEvent = cecGpioLine.event_read();

        if (gpioLineEvent.event_type == gpiod::line_event::FALLING_EDGE)
        {
            current = cecIntManager->getStatus();

            cecIntManager->setStatus(!current);

            I2CCommLib deviceLayer(
                phosphor::NvidiaSecureUpdate::busIdentifier,
                phosphor::NvidiaSecureUpdate::deviceAddrress);

            retVal = deviceLayer.QueryAboutInterrupt();

            if (retVal ==
                static_cast<uint8_t>(
                    I2CCommLib::CECInterruptStatus::BMC_FW_UPDATE_FAIL))
            {
                log<level::ERR>(
                    "secure_monitor_service - OOB Firmware update failed.",
                    entry("ERR=0x%x", retVal));
            }
            else if (retVal ==
                     static_cast<uint8_t>(I2CCommLib::CECInterruptStatus::
                                              BMC_FW_UPDATE_REQUEST_RESET_NOW))
            {
                log<level::DEBUG>(
                    "secure_monitor_service - OOB Firmware update "
                    "succeeded,immediate reset expected.");
                RebootBmc();
                return 0;
            }
            else
            {
                log<level::DEBUG>(
                    "secure_monitor_service - Firmware update succeeded.");
            }
        }
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("secure_monitor_service - OOB Firmware update, "
                        "exception in handling interrupt .",
                        entry("EXCEPTION=%s", e.what()));
    }

    sd_event_add_io(cecGpioEventLoop, nullptr, cecGpioLineFd, EPOLLIN,
                    GpioInterruptHandler, nullptr);

    return 0;
}

static bool RequestGPIOEvents(const std::string& name, gpiod::line& gpioLine,
                              int& gpioLineFd, sd_event& gpioEventDescriptor)
{
    // Find the GPIO line
    gpioLine = gpiod::find_line(name);

    if (!gpioLine)
    {
        log<level::ERR>(
            "secure_monitor_service - Unable to find gpio LineName.",
            entry("GPIONAME=%s", name.c_str()));

        return false;
    }

    try
    {
        gpioLine.request(
            {"secure_monitor_service", gpiod::line_request::EVENT_BOTH_EDGES, {}});
    }
    catch (std::exception&)
    {
        log<level::ERR>(
            "secure_monitor_service - Failed to request events for ",
            entry("EXCEPTION=%s", name.c_str()));

        return false;
    }

    gpioLineFd = gpioLine.event_get_fd();
    if (gpioLineFd < 0)
    {
        log<level::ERR>("secure_monitor_service - Failed to get event fd  ",
                        entry("EXCEPTION=%s", name.c_str()));

        return false;
    }

    auto rc = sd_event_add_io(&gpioEventDescriptor, nullptr, gpioLineFd,
                              EPOLLIN, GpioInterruptHandler, nullptr);
    if (0 > rc)
    {
        log<level::ERR>("RequestGPIOEvents - Failed to add to event loop  ",
                        entry("RETVAL=%d", rc));
        return false;
    }

    return true;
}

static void HandleTerminate(sdeventplus::source::Signal& source,
                            const struct signalfd_siginfo*)
{
    source.get_event().exit(0);
}

std::string getBMCVersion(const std::string& releaseFilePath)
{
    std::string versionKey = "VERSION_ID=";
    std::string versionValue{};
    std::string version{};
    std::ifstream efile;
    std::string line;
    efile.open(releaseFilePath);

    while (getline(efile, line))
    {
        if (line.substr(0, versionKey.size()).find(versionKey) !=
            std::string::npos)
        {
            // Support quoted and unquoted values
            // 1. Remove the versionKey so that we process the value only.
            versionValue = line.substr(versionKey.size());

            // 2. Look for a starting quote, then increment the position by 1 to
            //    skip the quote character. If no quote is found,
            //    find_first_of() returns npos (-1), which by adding +1 sets pos
            //    to 0 (beginning of unquoted string).
            std::size_t pos = versionValue.find_first_of('"') + 1;

            // 3. Look for ending quote, then decrease the position by pos to
            //    get the size of the string up to before the ending quote. If
            //    no quote is found, find_last_of() returns npos (-1), and pos
            //    is 0 for the unquoted case, so substr() is called with a len
            //    parameter of npos (-1) which according to the documentation
            //    indicates to use all characters until the end of the string.
            version =
                versionValue.substr(pos, versionValue.find_last_of('"') - pos);
            break;
        }
    }
    efile.close();

    if (version.empty())
    {
        log<level::ERR>("Error BMC current version is empty");
        elog<InternalFailure>();
    }

    return version;
}

ObjectValueTree getManagedObjects(const std::string& service,
                                  const std::string& objPath)
{
    ObjectValueTree interfaces;

    auto method = bus.new_method_call(service.c_str(), objPath.c_str(),
                                      "org.freedesktop.DBus.ObjectManager",
                                      "GetManagedObjects");

    auto reply = bus.call(method);

    if (reply.is_method_error())
    {
        log<level::ERR>("Failed to get managed objects",
                        entry("PATH=%s", objPath.c_str()));
        return interfaces;
    }

    reply.read(interfaces);
    return interfaces;
}

void EnableRebootGuard()
{
    if (enabledGuard)
    {
        log<level::INFO>("Already enabled reboot guard");
        return;
    }

    log<level::INFO>("BMC image activating - BMC reboots are disabled.");

    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("reboot-guard-enable.service", "replace");
    bus.call_noreply(method);

    enabledGuard = true;
}

void DisableRebootGuard()
{
    log<level::INFO>("BMC activation has ended - BMC reboots are re-enabled.");

    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("reboot-guard-disable.service", "replace");
    bus.call_noreply(method);

    enabledGuard = false;
}

static void TimerCallBack()
{
    try
    {
        ApplyRebootGaurd();
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("TimerCallBack - ApplyRebootGaurd Exception.",
                        entry("EXCEPTION=%s", e.what()));
    }
}

/** @brief Applied gaurd to stop BMC reboot.
 *
 * Enables reboot gaurd, if the system finds CEC in a busy state
 * after a succesful BMC secure update.
 * Disables reboot gaurd, if the system finds CEC in a non-busy state
 * after a succesful BMC secure update.
 *
 */

void ApplyRebootGaurd()
{
    try
    {
        auto objValueTree =
            getManagedObjects(BUSNAME_UPDATER, SOFTWARE_OBJPATH);

        // Read os-release from /etc/ to get the functional BMC version
        auto functionalVersion = getBMCVersion(OS_RELEASE_FILE);

        bool startTimer{false};

        if (checkTimer)
        {
            checkTimer->stop();
        }
        for (const auto& objIter : objValueTree)
        {
            try
            {
                auto& objPath = objIter.first;
                std::string objPathStr(objPath);
                auto& intfMap = objIter.second;
                auto& activationProps =
                    intfMap.at("xyz.openbmc_project.Software.Activation");
                auto activation =
                    std::get<std::string>(activationProps.at("Activation"));
                auto& versionProps =
                    intfMap.at("xyz.openbmc_project.Software.Version");
                auto versionStr =
                    std::get<std::string>(versionProps.at("Version"));

                auto& pathProps =
                    intfMap.at("xyz.openbmc_project.Common.FilePath");
                auto pathStr = std::get<std::string>(pathProps.at("Path"));

                // Skip if the object name contains RUNNING_BMC and
                // its version property matches the current version
                if (functionalVersion == versionStr &&
                    objPathStr.find(RUNNING_BMC) != std::string::npos)
                {
                    continue;
                }

                if ((Activation::convertActivationsFromString(activation) ==
                     Activation::Activations::Active) ||
                    (Activation::convertActivationsFromString(activation) ==
                     Activation::Activations::Staged))
                {
                    I2CCommLib deviceLayer(busIdentifier, deviceAddrress);

                    uint8_t retVal = static_cast<uint8_t>(
                        I2CCommLib::CommandStatus::UNKNOWN);

                    try
                    {
                        retVal = deviceLayer.GetCECState();

                        if (retVal == static_cast<uint8_t>(
                                          I2CCommLib::CommandStatus::ERR_BUSY))
                        {
                            EnableRebootGuard();
                        }
                        else
                        {
                            DisableRebootGuard();
                        }
                    }
                    catch (const std::exception& e)
                    {
                        log<level::ERR>(
                            "secure_monitor_service - ApplyRebootGaurd failed.",
                            entry("EXCEPTION=%s", e.what()));
                    }
                }

                if ((Activation::convertActivationsFromString(activation) ==
                     Activation::Activations::Ready) ||
                    (Activation::convertActivationsFromString(activation) ==
                     Activation::Activations::Active) ||
                    (Activation::convertActivationsFromString(activation) ==
                     Activation::Activations::Activating) ||
                    (Activation::convertActivationsFromString(activation) ==
                     Activation::Activations::Staged))
                {
                    startTimer = true;
                    break;
                }
            }
            catch (const std::exception& e)
            {
                log<level::ERR>(
                    "secure_monitor_service - ApplyRebootGaurd Exception.",
                    entry("EXCEPTION=%s", e.what()));
            }
        }
        if (startTimer)
        {
            checkTimer->start(
                duration_cast<microseconds>(seconds(checkTimerExpiry)), true);
        }
        else
        {
            checkTimer->stop();
            DisableRebootGuard();
        }
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("ApplyRebootGaurd - Exception.",
                        entry("EXCEPTION=%s", e.what()));
    }
}

static void StartWatchingUpdate(sdbusplus::message::message& msg)
{
    using SVersion = server::Version;
    using VersionPurpose = SVersion::VersionPurpose;
    namespace mesg = sdbusplus::message;

    mesg::object_path objPath;
    std::map<std::string, std::map<std::string, std::variant<std::string>>> interfaces;
    try
    {
        msg.read(objPath, interfaces);
        std::string path(std::move(objPath));
        std::string filePath;

        for (const auto& intf : interfaces)
        {
            if (intf.first == VERSION_IFACE)
            {
                for (const auto& property : intf.second)
                {
                    if (property.first == "Purpose")
                    {
                        auto value = SVersion::convertVersionPurposeFromString(
                            std::get<std::string>(property.second));
                        if (value == VersionPurpose::BMC)
                        {
                            ApplyRebootGaurd();
                        }
                    }
                }
            }
            else if (intf.first == FILEPATH_IFACE)
            {
                for (const auto& property : intf.second)
                {
                    if (property.first == "Path")
                    {
                        std::string strTmp{
                            "Enabled StartWatchingUpdate for Obj:"};
                        filePath = std::get<std::string>(property.second);
                        strTmp += filePath;
                        log<level::INFO>(strTmp.c_str());
                    }
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("StartWatchingUpdate - Exception.",
                        entry("EXCEPTION=%s", e.what()));
    }
}

} // namespace NvidiaSecureUpdate
} // namespace phosphor

int main([[maybe_unused]]int argc, [[maybe_unused]]char** argv)
{
    CLI::App app{"Nvidia Secure Monitor Service"};

    sd_event_default(&phosphor::NvidiaSecureUpdate::cecGpioEventLoop);

    stdplus::signal::block(SIGTERM);
    sdeventplus::source::Signal sigint(
        phosphor::NvidiaSecureUpdate::cecGpioEventLoop, SIGTERM,
        phosphor::NvidiaSecureUpdate::HandleTerminate);

    phosphor::NvidiaSecureUpdate::bus.request_name(SECUREBOOT_BUSNAME);

    sdbusplus::bus::match_t versionMatch(
        phosphor::NvidiaSecureUpdate::bus,
        MatchRules::interfacesAdded() +
            MatchRules::path("/xyz/openbmc_project/software"),
        phosphor::NvidiaSecureUpdate::StartWatchingUpdate);

    // Add sdbusplus ObjectManager for the 'root' path of the  manager.
    sdbusplus::server::manager::manager objManager(
        phosphor::NvidiaSecureUpdate::bus, SECUREBOOT_PATH);

    phosphor::NvidiaSecureUpdate::cecIntManager =
        std::make_unique<phosphor::NvidiaSecureUpdate::CecImpl>(
            phosphor::NvidiaSecureUpdate::bus, SECUREBOOT_PATH);

    phosphor::NvidiaSecureUpdate::bus.attach_event(
        phosphor::NvidiaSecureUpdate::cecGpioEventLoop,
        SD_EVENT_PRIORITY_NORMAL);

    phosphor::NvidiaSecureUpdate::checkTimer =
        std::make_unique<phosphor::Timer>(
            phosphor::NvidiaSecureUpdate::bus.get_event(),
            phosphor::NvidiaSecureUpdate::TimerCallBack);

    if (!phosphor::NvidiaSecureUpdate::RequestGPIOEvents(
            phosphor::NvidiaSecureUpdate::cecGpioName,
            phosphor::NvidiaSecureUpdate::cecGpioLine,
            phosphor::NvidiaSecureUpdate::cecGpioLineFd,
            *phosphor::NvidiaSecureUpdate::cecGpioEventLoop))
    {
        return -1;
    }

    sd_event_loop(phosphor::NvidiaSecureUpdate::cecGpioEventLoop);

    return 0;
}
