#pragma once
#include "ap_fw_activation.hpp"
#include "version.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/timer.hpp>

namespace phosphor
{
namespace software
{
namespace firmwareupdater
{
namespace sdbusRule = sdbusplus::bus::match::rules;

static const std::string progressFile{"progress.txt"};

using DbusInterface = std::string;

using DbusProperty = std::string;

using Value = std::variant<bool, uint8_t, int16_t, uint16_t, int32_t, uint32_t,
                           int64_t, uint64_t, double, std::string>;

using PropertyMap = std::map<DbusProperty, Value>;

using DbusInterfaceMap = std::map<DbusInterface, PropertyMap>;

using ObjectValueTree =
    std::map<sdbusplus::message::object_path, DbusInterfaceMap>;

class UpdateManager;

class UpdateManager
{
  public:
    UpdateManager(sdbusplus::bus::bus& bus) :
        bus(bus),
        systemdSignals(
            bus,
            sdbusRule::type::signal() + sdbusRule::member("JobRemoved") +
                sdbusRule::path("/org/freedesktop/systemd1") +
                sdbusRule::interface("org.freedesktop.systemd1.Manager"),
            std::bind(std::mem_fn(&UpdateManager::unitStateChange), this,
                      std::placeholders::_1)) {};

    int processImage(const std::string& filePath);

    void failUpdate(uint8_t progress, std::string error_msg = "",
                    bool failed = true);

    void progress(uint8_t progress, std::string msg = "",
                  bool fwUpdatePass = false, bool updateResult = false);

  private:
    void subscribeToSystemdSignals();

    void unsubscribeFromSystemdSignals();

    void EnableRebootGuard();

    void DisableRebootGuard();

    ObjectValueTree getManagedObjects(const std::string& service,
                                      const std::string& objPath);

    bool checkActiveBMCUpdate();

    void unitStateChange(sdbusplus::message::message& msg);

  public:
    enum class SecureUpdate
    {
        IDLE,
        INPROGRESS
    };

    SecureUpdate secureUpdateProgress = SecureUpdate::IDLE;

    bool secureFlashSuceeded = true;

    std::unique_ptr<sdbusplus::Timer> secureUpdateTimer;

    sdbusplus::bus::bus& bus;

  private:
    /** @brief Used to subscribe to dbus systemd signals **/
    sdbusplus::bus::match_t systemdSignals;
    std::unique_ptr<ApFwActivation> activation;
    std::unique_ptr<ApFwActivationProgress> activationProgress;
};

} // namespace firmwareupdater
} // namespace software
} // namespace phosphor
