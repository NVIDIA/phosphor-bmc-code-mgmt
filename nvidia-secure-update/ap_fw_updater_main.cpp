#include "config.h"

#include "pris_ap_fw_state_machine.hpp"
#include "ap_fw_updater.hpp"
#include "watch.hpp"
#include "i2c_comm_lib.hpp"
#include "version_inv_entry.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/manager.hpp>
#include <phosphor-logging/log.hpp>

#include <filesystem>

static constexpr auto busIdentifier = CEC_BUS_IDENTIFIER;
static constexpr auto deviceAddrress = CEC_DEVICE_ADDRESS;

using namespace phosphor::logging;


static std::string getCecVersion(void)
{
    std::unique_ptr<phosphor::software::updater::I2CCommLib> deviceLayer = std::make_unique<phosphor::software::updater::I2CCommLib>(
        CEC_BUS_IDENTIFIER, CEC_DEVICE_ADDRESS);
    phosphor::software::updater::I2CCommLib::ReadCecVersion version;
    deviceLayer->GetCecVersion(version);
    return std::to_string(version.major) + "-" + std::to_string(version.minor);
}

int main([[maybe_unused]]int argc, [[maybe_unused]]char* argv[])
{
    auto bus = sdbusplus::bus::new_default();
    
    std::unique_ptr<phosphor::software::manager::Watch> watchGPU;
    std::unique_ptr<phosphor::software::manager::Watch> watchCEC;

    sd_event* loop = nullptr;
    sd_event_default(&loop);

    try
    {
        bus.request_name("xyz.openbmc_project.Software.BMC.Nvidia.Updater");

        std::unique_ptr<phosphor::software::manager::VersionInventoryEntry> versionPtr = std::make_unique<phosphor::software::manager::VersionInventoryEntry>(
            bus, std::filesystem::path(SOFTWARE_OBJPATH) / "BF_CEC", getCecVersion());
        versionPtr->createFunctionalAssociation(SOFTWARE_OBJPATH);
        versionPtr->createUpdateableAssociation(SOFTWARE_OBJPATH);

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
        log<level::ERR>(e.what());
        return -1;
    }

    sd_event_unref(loop);

    return 0;
}
