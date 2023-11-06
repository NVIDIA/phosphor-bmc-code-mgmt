#include "config.h"

#include "watch.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <elog-errors.hpp>
#include <experimental/filesystem>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <string>


#include "pris_ap_fw_state_machine.hpp"
#include "state_machine.hpp"
#include "ap_fw_updater.hpp"

#include <sdbusplus/exception.hpp>
#include <fstream>


namespace phosphor
{
namespace software
{
namespace firmwareupdater
{

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Software::Image::Error;
using namespace phosphor::software::updater;
using sdbusplus::exception::SdBusError;
using namespace std::chrono;

namespace Software = phosphor::logging::xyz::openbmc_project::Software;
namespace server = sdbusplus::xyz::openbmc_project::Software::server;
namespace fs = std::experimental::filesystem;

std::unique_ptr<MachineContext> sUpdateMachineContext;
std::unique_ptr<StateMachine> fwUpdateMachine;
class PrisAPFWStateMachine;


struct RemovablePath
{
    fs::path path;

    RemovablePath(const fs::path& path) : path(path)
    {
    }
    ~RemovablePath()
    {
        if (!path.empty())
        {
            std::error_code ec;
            fs::remove_all(path, ec);
        }
    }
};

void UpdateManager::subscribeToSystemdSignals()
{
    auto method = this->bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                            SYSTEMD_INTERFACE, "Subscribe");
    try
    {
        this->bus.call_noreply(method);
    }
    catch (const SdBusError& e)
    {
        if (e.name() != nullptr &&
            strcmp("org.freedesktop.systemd1.AlreadySubscribed", e.name()) == 0)
        {
            // If an Activation attempt fails, the Unsubscribe method is not
            // called. This may lead to an AlreadySubscribed error if the
            // Activation is re-attempted.
        }
        else
        {
            log<level::ERR>("Error subscribing to systemd",
                            entry("ERROR=%s", e.what()));
        }
    }

    return;
}


void UpdateManager::unsubscribeFromSystemdSignals()
{
    auto method = this->bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                            SYSTEMD_INTERFACE, "Unsubscribe");
    try
    {
        this->bus.call_noreply(method);
    }
    catch (const SdBusError& e)
    {
        log<level::ERR>("Error in unsubscribing from systemd signals",
                        entry("ERROR=%s", e.what()));
    }

    return;
}

ObjectValueTree UpdateManager::getManagedObjects(const std::string& service,
                                  const std::string& objPath)
{
    ObjectValueTree interfaces;

    auto method = bus.new_method_call(service.c_str(), objPath.c_str(),
                                      "org.freedesktop.DBus.ObjectManager",
                                      "GetManagedObjects");

    auto reply = bus.call(method);

    if (reply.is_method_error())
    {
        log<level::ERR>("Failed to get managed objects",
                        entry("PATH=%s", objPath.c_str()));
        return interfaces;
    }

    reply.read(interfaces);
    return interfaces;
}

bool UpdateManager::checkActiveBMCUpdate()
{
    try
    {
        auto objValueTree =
                getManagedObjects(BUSNAME_UPDATER, SOFTWARE_OBJPATH);

        for (const auto& objIter : objValueTree)
        {
            try
            {
                auto& intfMap = objIter.second;
                auto& activationProps =
                    intfMap.at("xyz.openbmc_project.Software.Activation");
                auto activation =
                    std::get<std::string>(activationProps.at("Activation"));

                if ((Activation::convertActivationsFromString(activation) ==
                     Activation::Activations::Activating))
                {
                    log<level::ERR>(
                         "BMC firmware update has been triggred and in "
                         "progress");
                    return true;
                }
            }
            catch (const std::exception& e)
            {
                log<level::ERR>(
                   "Error while checking any BMC active update in progress",
                   entry("EXCEPTION=%s", e.what()));
                return true;
            }
          }
    }
    catch (const std::exception& e)
    {
         log<level::ERR>("Error check any BMC active upgrade",
                   entry("EXCEPTION=%s", e.what()));
         return true;
    }

    return false;
}

void UpdateManager::failUpdate(uint8_t progressChange, std::string msg, bool failed)
{
   std::string filePath = 
      std::any_cast<std::string>(sUpdateMachineContext->GetData(keyFWImgName));

   DisableRebootGuard();

   RemovablePath fwFilePathRemove(filePath);

   secureFlashSuceeded = false;
   secureUpdateProgress = SecureUpdate::IDLE;

   progress(progressChange, msg, !failed, true);

   if (sUpdateMachineContext)
   {
       sUpdateMachineContext->ClearData();
       sUpdateMachineContext.reset(nullptr);
   }
   if (fwUpdateMachine)
   {
       fwUpdateMachine.reset(nullptr);
   }

}

void UpdateManager::progress(uint8_t progress, std::string msg, bool fwUpdatePass, bool updateResult)
{
     fs::path filePath(std::any_cast<std::string>(sUpdateMachineContext->GetData(keyFWImgName)));

     std::string fileName = {phosphor::software::firmwareupdater::cecFWFolder + progressFile};
 
     if(activationProgress)
     {
        activationProgress->progress(progress);
     }

     std::string outBuf{""};

     if(!updateResult)
     {
          outBuf = std::string("TaskState=\"Running\"\nTaskStatus=\"OK\"\nTaskProgress=\"") +
              std::to_string(progress) + "\"\n";
     } 
     else
     {
        server::Activation::Activations res = server::Activation::Activations::Active;
        std::string taskState{"Firmware update succeeded.\n"};
        std::string taskStatus{"OK\n"};
        std::string taskProgress{"100\n"};
        
        if(!fwUpdatePass)
        {
              res = server::Activation::Activations::Failed;
              taskState = "Firmware update failed.\n";
              taskStatus = "FAILED\n";
              taskProgress = "\"" + std::to_string(progress) + "\"\n"; 
        }
        outBuf = std::string("TaskState="    + taskState + 
                             "TaskStatus="   + taskStatus + 
                             "TaskProgress=" + taskProgress);

        if(activation)
        {
            activation->activation(res);
        }
     }  
     if (!msg.empty())
     {
        outBuf += std::string("CEC info: ") + msg;
     }

     // Update fwFile
     std::ofstream fwFile(fileName, std::ofstream::out);
     fwFile << outBuf;
     fwFile.close();
}

void UpdateManager::EnableRebootGuard()
{
    log<level::INFO>("BMC image activating - BMC reboots are disabled.");

    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("reboot-guard-enable.service", "replace");
    bus.call_noreply(method);

}

void UpdateManager::DisableRebootGuard()
{
    log<level::INFO>("BMC activation has ended - BMC reboots are re-enabled.");

    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("reboot-guard-disable.service", "replace");
    bus.call_noreply(method);

}

int UpdateManager::processImage(const std::string& filePath)
{
    uint8_t progress{20};
    try
    {
        fs::path fwFilePath(filePath);

        if(filePath.find(progressFile) != std::string::npos)
        {
            return 0;
        }

        if(secureUpdateProgress == SecureUpdate::INPROGRESS)
        {
           RemovablePath fwFilePathRemove(filePath);
           log<level::ERR>("Already a firmware is in progress");

           return -1; 
        }

        EnableRebootGuard();

        std::string objPath = std::string{SOFTWARE_CEC_UPDATE_OBJPATH};
        // Check if activation exists
        if (activation)
        {
            activation.reset();
        }
        // The object must be created everytime an update procedure is initiated
        // in order to trigger the update-service callback function in the bmcweb,
        // which tracks the procedure by checking the object properties.
        activation = std::make_unique<ApFwActivation>(bus, objPath,
                                                      server::Activation::Activations::Ready,
                                                      server::Activation::RequestedActivations::None,
                                                      this);
        // Check if activationProgress exists
        if (activationProgress)
        {
            activationProgress.reset();
        }
        activationProgress = std::make_unique<ApFwActivationProgress>(bus, objPath);

        if (!fs::is_regular_file(filePath))
        {
           std::string msg = std::string("Error file does not exist. path: ") + std::string(filePath);
           log<level::ERR>(msg.c_str(),
                        entry("FILENAME=%s", filePath.c_str()));
           failUpdate(0, msg);
           return -1;
        }

        secureFlashSuceeded = true;
        secureUpdateProgress = SecureUpdate::INPROGRESS;

        if (sUpdateMachineContext)
        {
            sUpdateMachineContext.reset(nullptr);
        }
        if (fwUpdateMachine)
        {
            fwUpdateMachine.reset(nullptr);
        }

        sUpdateMachineContext = std::make_unique<MachineContext>(nullptr);

        uint32_t fSize = static_cast<uint32_t>(fs::file_size(fwFilePath));


        sUpdateMachineContext->SetData(keyUpdateManager, this);

        sUpdateMachineContext->SetData(keyFWFileSize, fSize);

        sUpdateMachineContext->SetData(keyFWImgName, fwFilePath.string());

        fwUpdateMachine =
            std::make_unique<PrisAPFWStateMachine>(*(sUpdateMachineContext.get()));

        if(checkActiveBMCUpdate())
        {
           std::string msg{"AP firmware update failed."};
           log<level::ERR>(msg.c_str());
           failUpdate(0, msg);
           return -1;
        }

        subscribeToSystemdSignals();

        fwUpdateMachine->TriggerFWUpdate();

        std::tuple<bool, std::string> ret =
            sUpdateMachineContext->GetMachineRunStatus();
        bool smRunSuceeded = std::get<0>(ret);

        if (!smRunSuceeded)
        {
            std::string msg{"SECURE UPDATE FAILED IN A STATE "}; 
            std::string retMsg = std::get<1>(ret);
            unsubscribeFromSystemdSignals();
            failUpdate(progress, msg + retMsg);

            log<level::ERR>(msg.c_str(),
                            entry("FAILURE=%s", retMsg.c_str()));
            return -1;
        }
    }
    catch (const std::exception& e)
    {
        unsubscribeFromSystemdSignals();
        std::string msg{"SECURE UPDATE RUN EXCEPTION "};
        failUpdate(progress, msg);

        log<level::ERR>(msg.c_str(),
                        entry("EXCEPTION=%s", e.what()));
        return -1;
    }

    return 0;
}

void UpdateManager::unitStateChange(sdbusplus::message::message& msg)
{
    if (secureUpdateProgress != SecureUpdate::INPROGRESS)
    {
        return;
    }

    uint32_t newStateID{};
    sdbusplus::message::object_path newStateObjPath;
    std::string newStateUnit{};
    std::string newStateResult{};
    uint8_t progress{50};

    try
    {
        // Read the msg and populate each variable
        msg.read(newStateID, newStateObjPath, newStateUnit, newStateResult);

        auto copyImageServiceFile =
             std::any_cast<std::string>(sUpdateMachineContext->GetData(keyCopyServiceNameString));

        if (newStateUnit != copyImageServiceFile)
        {
            return;
        }

        secureUpdateTimer->stop();
        unsubscribeFromSystemdSignals();

        if (newStateResult == "done")
        {
            if (fwUpdateMachine &&
                fwUpdateMachine->GetCurrentState() ==
                    (static_cast<uint8_t>(
                        PrisAPFWStateMachine::States::STATE_COPY_IMAGE)))
            {

                fwUpdateMachine->TriggerState((static_cast<uint8_t>(
                    PrisAPFWStateMachine::States::STATE_CHECK_UPDATE_STATUS)));

                std::tuple<bool, std::string> ret =
                    sUpdateMachineContext->GetMachineRunStatus();
                bool smRunSuceeded = std::get<0>(ret);
                std::string errMsg = std::get<1>(ret);

                progress = 90; 
                if (smRunSuceeded)
                {
                    if(errMsg == checkUpdateInProgress)
                    {
                        //Image copy succeded and f/w update in progress.
                        //this is happy path.

                        //Restart the timer, still f/w update is in progress.
                        secureUpdateTimer->start(
                             duration_cast<microseconds>(seconds(PrisAPFWStateMachine::timerCheckFWUpdateSecs)));
                        return;
                    }
                    progress = 100; 
                    failUpdate(progress, "", false);
                    return;
                }
                else
                {
                    log<level::ERR>("onStateChanges: STATE Failed",
                                    entry("FAILURE=%s", errMsg.c_str()));
                }
            }
        }
        else
        {
            if (newStateResult == "failed" || newStateResult == "dependency")
            {
                log<level::ERR>(
                    "onStateChanges Secure Image Copy script: failed run.",
                    entry("RESULT=%s", newStateResult.c_str()));
            }
        }
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("onStateChanges Exception",
                        entry("EXCEPTION=%s", e.what()));
    }
    failUpdate(progress);

    return;
}


} // namespace manager
} // namespace software
} // namespace phosphor
