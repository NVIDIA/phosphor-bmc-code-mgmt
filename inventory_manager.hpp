#pragma once
#include "version_inv_entry.hpp"
#include <xyz/openbmc_project/Common/FactoryReset/server.hpp>
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

/**
 * @brief Manages Inventory for BMC firmware versions. This publishes the BMC
 *        version information into a DBUS path for other services to query.
 *        Handles Factory reset Dbus backend implementation for erasing RWFS 
 *
 */
class InventoryManager : public InventoryManagerInherit
{
  public:
    /**
     * @brief Construct a new Inventory Manager object
     *
     * @param[in/out] bus dbus service
     */
    InventoryManager(sdbusplus::bus::bus& bus) :
        InventoryManagerInherit(bus, SOFTWARE_OBJPATH, false), bus(bus)
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

        versionPtr->createUpdateableAssociation(SOFTWARE_OBJPATH);
    }
    
    /** @brief BMC factory reset - marks the read-write partition for
     * recreation upon reboot. */
    void reset() 
    {
        constexpr auto setFactoryResetWait = std::chrono::seconds(3);
        // Mark the read-write partition for recreation upon reboot.
        utils::execute("/sbin/fw_setenv", "openbmconce", "factory-reset");

        // Need to wait for env variables to complete, otherwise an immediate reboot
        // will not factory reset.
        std::this_thread::sleep_for(setFactoryResetWait);

        log<level::INFO>("BMC factory reset will take effect upon reboot.");            
    }

  private:
    std::unique_ptr<VersionInventoryEntry> versionPtr;

    sdbusplus::bus::bus& bus;
};

} // namespace manager
} // namespace software
} // namespace phosphor
