#pragma once
#include "version_inv_entry.hpp"
#include <xyz/openbmc_project/Common/FactoryReset/server.hpp>
#include <xyz/openbmc_project/Software/Settings/server.hpp>
#include <com/nvidia/Common/CompleteReset/server.hpp>
#include "utils.hpp"
#include <phosphor-logging/elog.hpp>
#include <sdbusplus/server.hpp>

#include <filesystem>
#include <iostream>
#include <string>

namespace phosphor
{
namespace software
{
namespace manager
{
namespace fs = std::filesystem;

using namespace phosphor::logging;

using InventoryManagerInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Common::server::FactoryReset>;
using OemCompleteResetInherit = sdbusplus::server::object::object<
    sdbusplus::com::nvidia::Common::server::CompleteReset>;
using SettingsInventoryEntryInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::Settings>;

/**
 * @brief Manages Inventory for BMC firmware versions. This publishes the BMC
 *        version information into a DBUS path for other services to query.
 *        Handles Factory reset Dbus backend implementation for erasing RWFS 
 *
 */
class InventoryManager : public InventoryManagerInherit, OemCompleteResetInherit, SettingsInventoryEntryInherit
{
  public:
    /**
     * @brief Construct a new Inventory Manager object
     *
     * @param[in/out] bus dbus service
     */
    InventoryManager(sdbusplus::bus::bus& bus) :
        InventoryManagerInherit(bus, SOFTWARE_OBJPATH,
                                InventoryManagerInherit::action::defer_emit),
        OemCompleteResetInherit(bus, SOFTWARE_OBJPATH,
                                OemCompleteResetInherit::action::defer_emit),
        SettingsInventoryEntryInherit(bus,(std::string{SOFTWARE_OBJPATH} + "/" + std::string{FIRMWARE_INV_NAME}).c_str(),
                                SettingsInventoryEntryInherit::action::defer_emit),
        bus(bus)
    {
        readExistingVersion();
    }

    /**
     * @brief Read the existing BMC version
     *
     */
    void readExistingVersion()
    {
        using VersionClass = phosphor::software::manager::Version;
        auto version = VersionClass::getBMCVersion(OS_RELEASE_FILE);
        auto id = VersionClass::getId(version);
        std::string name = FIRMWARE_INV_NAME;
        if (name.length() == 0) {
            name = id;
        }
        auto path = fs::path(SOFTWARE_OBJPATH) / name;

        versionPtr = std::make_unique<VersionInventoryEntry>(
            bus, path, version);

        versionPtr->createFunctionalAssociation(SOFTWARE_OBJPATH);
        versionPtr->createUpdateableAssociation(SOFTWARE_OBJPATH);
        std::string platformBMCId = PLATFORM_BMC_ID;
        if (platformBMCId.length() == 0)
        {
            platformBMCId = id;
        }
        auto chassisPath =
            "/xyz/openbmc_project/inventory/system/bmc/" + platformBMCId;
        versionPtr->createForwardAssociation(chassisPath);
    }
    
    /** @brief BMC factory reset - marks the read-write partition for
     * recreation upon reboot. */
    void reset() 
    {
        constexpr auto setFactoryResetWait = std::chrono::seconds(3);
        // Mark the read-write partition for recreation upon reboot.
        utils::execute("/sbin/fw_setenv", "openbmconce", "factory-reset");

        // used to create a RF log upon reboot 
        utils::execute("/sbin/fw_setenv", "openbmclog", "factory-reset");

        // Need to wait for env variables to complete, otherwise an immediate reboot
        // will not factory reset.
        std::this_thread::sleep_for(setFactoryResetWait);

        log<level::INFO>("BMC factory reset will take effect upon reboot.");            
    }

    /** @brief BMC complete reset - marks the read-write partition for
     * recreation upon reboot, clears logs and diagnostic data */
    void completeReset() 
    {
        constexpr auto setCompleteResetWait = std::chrono::seconds(3);
        // Mark the read-write partition for recreation upon reboot.
        utils::execute("/sbin/fw_setenv", "openbmconce", "complete-reset");

        // used to create a RF log upon reboot 
        utils::execute("/sbin/fw_setenv", "openbmclog", "logs-reset");        

        // Need to wait for env variables to complete, otherwise an immediate reboot
        // will not factory reset.
        std::this_thread::sleep_for(setCompleteResetWait);

        log<level::INFO>("BMC complete reset will take effect upon reboot.");            
    }

  private:
    std::unique_ptr<VersionInventoryEntry> versionPtr;

    sdbusplus::bus::bus& bus;
};

} // namespace manager
} // namespace software
} // namespace phosphor
