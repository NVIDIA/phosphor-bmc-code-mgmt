#pragma once

#include "version.hpp"

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Association/server.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>

#include <string>
#include <vector>

namespace phosphor
{
namespace software
{
namespace manager
{

namespace server = sdbusplus::xyz::openbmc_project::Software::server;

using VersionInventoryEntryInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::Version,
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;

/**
 * @brief Class extends version
 *
 */
class VersionInventoryEntry : public VersionInventoryEntryInherit
{
  public:
    const std::string updateableFwdAssociation = "software_version";
    const std::string updateableRevAssociation = "updateable";
    /**
     * @brief Construct a new Version Extended object
     *
     * @param bus dbus interface
     * @param objPath dbus path for the created object
     * @param versionString string representation of the version
     */
    VersionInventoryEntry(sdbusplus::bus::bus& bus, const std::string& objPath,
                          const std::string& versionString) :
        VersionInventoryEntryInherit(bus, (objPath).c_str(), true)
    {
        // Set properties.
        purpose(server::Version::VersionPurpose::BMC);
        version(versionString);
        emit_object_added();
    };

    void createUpdateableAssociation(const std::string& p)
    {
        auto assocs = associations();
        assocs.emplace_back(std::make_tuple(updateableFwdAssociation,
                                            updateableRevAssociation, p));
        associations(assocs);
    }
};

} // namespace manager
} // namespace software
} // namespace phosphor
