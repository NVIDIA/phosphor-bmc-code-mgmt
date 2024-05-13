#include "config.h"

#include "ap_fw_updater.hpp"
#include "i2c_comm_lib.hpp"
#include "pris_ap_fw_state_machine.hpp"
#include "version_inv_entry.hpp"
#include "watch.hpp"

#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/manager.hpp>

#include <filesystem>

using namespace phosphor::logging;
namespace server = sdbusplus::xyz::openbmc_project::Software::server;

static std::string getCecVersion(void)
{
    std::unique_ptr<phosphor::software::updater::I2CCommLib> deviceLayer =
        std::make_unique<phosphor::software::updater::I2CCommLib>(
            CEC_BUS_IDENTIFIER, CEC_DEVICE_ADDRESS);
    phosphor::software::updater::I2CCommLib::ReadCecVersion version;
    deviceLayer->GetCecVersion(version);
    return std::to_string(version.major) + "-" + std::to_string(version.minor);
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    auto bus = sdbusplus::bus::new_default();

    std::unique_ptr<phosphor::software::manager::Watch> watchCEC;

    sd_event* loop = nullptr;
    sd_event_default(&loop);

    try
    {
        bus.request_name(BUSNAME_NVIDIA_UPDATER);

        sdbusplus::server::manager::manager objManager(bus, SOFTWARE_OBJPATH);

        std::unique_ptr<phosphor::software::manager::VersionInventoryEntry>
            versionPtr = std::make_unique<
                phosphor::software::manager::VersionInventoryEntry>(
                bus, std::filesystem::path(SOFTWARE_CEC_OBJPATH),
                getCecVersion());
        versionPtr->createFunctionalAssociation(SOFTWARE_OBJPATH);
        versionPtr->createUpdateableAssociation(SOFTWARE_OBJPATH);
        versionPtr->purpose(server::Version::VersionPurpose::Other);

        phosphor::software::firmwareupdater::UpdateManager imageManager(bus);

        watchCEC = std::make_unique<phosphor::software::manager::Watch>(
            loop, phosphor::software::firmwareupdater::cecFWFolder,
            std::bind(std::mem_fn(&phosphor::software::firmwareupdater::
                                      UpdateManager::processImage),
                      &imageManager, std::placeholders::_1));

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
