#include "config.h"

#include "inventory_manager.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/manager.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

using namespace phosphor::logging;

int main()
{
    auto bus = sdbusplus::bus::new_default();

    try
    {
        bus.request_name("xyz.openbmc_project.Software.BMC.Inventory");
    }
    catch (const sdbusplus::exception::SdBusError& error)
    {
        log<level::ERR>("Error while requesting service name",
                        entry("ERROR=%s", error.what()));
        return -1;
    }

    // Add sdbusplus ObjectManager.
    sdbusplus::server::manager::manager objManager(bus, SOFTWARE_OBJPATH);
    std::unique_ptr<phosphor::software::manager::InventoryManager> manager;
    try
    {
        manager =
            std::make_unique<phosphor::software::manager::InventoryManager>(
                bus);
    }
    catch (
        const sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure&
            error)
    {
        log<level::ERR>("Error while adding ObjectManager",
                        entry("ERROR=%s", error.what()));
        return -1;
    }

    while (true)
    {
        try
        {
            bus.process_discard();
        }
        catch (const sdbusplus::exception::SdBusError& error)
        {
            log<level::ERR>("Error in bus process",
                            entry("ERROR=%s", error.what()));
        }
        bus.wait();
    }
    return 0;
}
