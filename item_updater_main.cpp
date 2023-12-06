#include "config.h"

#include "item_updater.hpp"

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/manager.hpp>

boost::asio::io_context& getIOContext()
{
    static boost::asio::io_context io;
    return io;
}

int main()
{
#ifdef NVIDIA_SECURE_BOOT
    auto bus = sdbusplus::bus::new_default();

    sd_event* loop = nullptr;

    sd_event_default(&loop);
#else
    sdbusplus::asio::connection bus(getIOContext());
#endif

    // Add sdbusplus ObjectManager.
    sdbusplus::server::manager_t objManager(bus, SOFTWARE_OBJPATH);

    phosphor::software::updater::ItemUpdater updater(bus, SOFTWARE_OBJPATH);

    bus.request_name(BUSNAME_UPDATER);

#ifdef NVIDIA_SECURE_BOOT
    bus.attach_event(loop, SD_EVENT_PRIORITY_NORMAL);

    sd_event_loop(loop);

    sd_event_unref(loop);
#else
    getIOContext().run();
#endif

    return 0;
}
