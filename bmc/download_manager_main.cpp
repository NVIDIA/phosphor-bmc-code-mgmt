#include "config.h"

#include "download_manager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>

#include <exception>

int main()
{
    auto bus = sdbusplus::bus::new_default();

    // Add sdbusplus ObjectManager.
    sdbusplus::server::manager_t objManager(bus, SOFTWARE_OBJPATH);

    phosphor::software::manager::Download manager(bus, SOFTWARE_OBJPATH);

    try
    {
        bus.request_name(DOWNLOAD_BUSNAME);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        lg2::error("Error requesting bus name: {ERROR}", "ERROR", e);
        return -1;
    }

    while (true)
    {
        try
        {
            bus.process_discard();
            bus.wait();
        }
        catch (const sdbusplus::exception::SdBusError& e)
        {
            lg2::error("Error in bus process: {ERROR}", "ERROR", e);
        }
    }
    return 0;
}
