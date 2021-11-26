#include "config.h"

#include "inventory_manager.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/manager.hpp>

int main()
{
    auto bus = sdbusplus::bus::new_default();

    // Add sdbusplus ObjectManager.
    sdbusplus::server::manager::manager objManager(bus, SOFTWARE_OBJPATH);

    phosphor::software::manager::InventoryManager manager(bus);

    bus.request_name("xyz.openbmc_project.Software.BMC.Inventory");

    while (true)
    {
        bus.process_discard();
        bus.wait();
    }
    return 0;
}
