#pragma once
#include "version_inv_entry.hpp"

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

/**
 * @brief Manages Inventory for BMC firmware versions. This publishes the BMC
 *        version information into a DBUS path for other services to query.
 *
 */
class InventoryManager
{
  public:
    /**
     * @brief Construct a new Inventory Manager object
     *
     * @param[in/out] bus dbus service
     */
    InventoryManager(sdbusplus::bus::bus& bus) : bus(bus)
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

  private:
    std::unique_ptr<VersionInventoryEntry> versionPtr;

    sdbusplus::bus::bus& bus;
};

} // namespace manager
} // namespace software
} // namespace phosphor
