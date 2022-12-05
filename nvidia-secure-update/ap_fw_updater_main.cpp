#include "config.h"

#include "pris_ap_fw_state_machine.hpp"

#include "ap_fw_updater.hpp"
#include "watch.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/manager.hpp>
#include <phosphor-logging/log.hpp>

int main([[maybe_unused]]int argc, [[maybe_unused]]char* argv[])
{
    auto bus = sdbusplus::bus::new_default();
    
    std::unique_ptr<phosphor::software::manager::Watch> watchGPU;
    std::unique_ptr<phosphor::software::manager::Watch> watchCEC;

    sd_event* loop = nullptr;
    sd_event_default(&loop);

    try
    {
        using namespace phosphor::logging;
        phosphor::software::firmwareupdater::UpdateManager imageManager(bus);

        if(ENABLE_GPU_IB_UPDATE)
        {
            watchGPU = std::make_unique<phosphor::software::manager::Watch>(
               loop, phosphor::software::firmwareupdater::gpuFWFolder,std::bind(std::mem_fn(&phosphor::software::firmwareupdater::UpdateManager::processImage), &imageManager,
                            std::placeholders::_1));
        }

        watchCEC = std::make_unique<phosphor::software::manager::Watch>(
            loop, phosphor::software::firmwareupdater::cecFWFolder,std::bind(std::mem_fn(&phosphor::software::firmwareupdater::UpdateManager::processImage), &imageManager,
                            std::placeholders::_1));

        bus.attach_event(loop, SD_EVENT_PRIORITY_NORMAL);
        sd_event_loop(loop);
    }
    catch (std::exception& e)
    {
        using namespace phosphor::logging;
        log<level::ERR>(e.what());
        return -1;
    }

    sd_event_unref(loop);

    return 0;
}
