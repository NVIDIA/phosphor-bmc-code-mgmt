#include "pris_ap_fw_state_machine.hpp"

#include "state_machine.hpp"

#include <sys/inotify.h>
#include <unistd.h>

#include <chrono>
#include <experimental/filesystem>
#include <fstream>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/timer.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <regex>

namespace phosphor
{
namespace software
{
namespace firmwareupdater
{

using namespace phosphor::logging;
using namespace std::chrono;
using namespace phosphor::software::updater;

namespace swUpdate = phosphor::software::updater;

namespace match_rules = sdbusplus::bus::match::rules;

namespace fs = std::experimental::filesystem;

extern std::unique_ptr<StateMachine> fwUpdateMachine;

PrisAPFWStateMachine::PrisAPFWStateMachine(MachineContext& ctx) :
    StateMachine(static_cast<uint8_t>(States::MAX_STATES), ctx,
                 static_cast<uint8_t>(States::STATE_IDLE)),
    fwUpdateManager(std::any_cast<UpdateManager*>(ctx.GetData(keyUpdateManager)))
{
    deviceLayer = std::make_unique<I2CCommLib>(
        PrisAPFWStateMachine::busIdentifier, PrisAPFWStateMachine::deviceAddrress);

   maxtimerCounter = 0;

   stateFlowSequence = {
        &FuncIdle,      &FuncCheckCECStatus,   &FuncStartFwUpdate,
        &FuncCopyImage, &FuncCheckUpdateStatus,
        &FuncTerminate
    };
}


const std::vector<StateFunc*>& PrisAPFWStateMachine::GetStateFlow()
{
    return stateFlowSequence;
}

void PrisAPFWStateMachine::TriggerFWUpdate()
{
    try
    {
        StartMachine(static_cast<uint8_t>(States::STATE_CHECK_CEC_STATUS));
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("TriggerFWUpdate Exception ",
                        entry("EXCEPTION=%s", e.what()));

        std::string tmpErr{"TriggerFWUpdate Exception: "};
        tmpErr += e.what();

        throw std::runtime_error(tmpErr.c_str());
    }
}

void PrisAPFWStateMachine::TriggerState(uint8_t newState)
{
    try
    {
        StartMachine(newState);
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("TriggerState Exception ",
                        entry("EXCEPTION=%s", e.what()));

        std::string tmpErr{"TriggerState Exception: "};
        tmpErr += e.what();

        throw std::runtime_error(tmpErr.c_str());
    }
}

bool PrisAPFWStateMachine::RunCheckUpdateStatus()
{
    bool flashSuceeded{false};

    try
    {
        if (fwUpdateMachine &&
            fwUpdateMachine->GetCurrentState() ==
                (static_cast<uint8_t>(States::STATE_CHECK_UPDATE_STATUS)))
        {
            fwUpdateMachine->TriggerState(
                (static_cast<uint8_t>(States::STATE_CHECK_UPDATE_STATUS)));

            std::tuple<bool, std::string> ret =
                fwUpdateMachine->myMachineContext.GetMachineRunStatus();
            bool smRunSuceeded = std::get<0>(ret);
            std::string msg{" "};
            if (!smRunSuceeded)
            {
                msg += std::get<1>(ret);
                log<level::ERR>("RunCheckUpdateStatus: Check FW Update Failed",
                                entry("FAILURE=%s", msg.c_str()));
            }
            else
            {
                flashSuceeded = true;
            }
        }
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("RunCheckUpdateStatus ",
                        entry("EXCEPTION=%s", e.what()));
    }
    return flashSuceeded;
}

void PrisAPFWStateMachine::TimerCallBack()
{
    bool flashSuceeded{false};
    uint8_t progress{0};

    try
    {
        auto updateManager = std::any_cast<UpdateManager*>(
            fwUpdateMachine->myMachineContext.GetData(keyUpdateManager));
        std::string msg;
        if (fwUpdateMachine &&
            fwUpdateMachine->GetCurrentState() ==
                (static_cast<uint8_t>(States::STATE_CHECK_UPDATE_STATUS)))
        {
            fwUpdateMachine->TriggerState(
                (static_cast<uint8_t>(States::STATE_CHECK_UPDATE_STATUS)));

            std::tuple<bool, std::string> ret =
                fwUpdateMachine->myMachineContext.GetMachineRunStatus();
            bool smRunSuceeded = std::get<0>(ret);
            msg = std::get<1>(ret);
            progress = 90;
            if (smRunSuceeded)
            {
                if(msg == checkUpdateInProgress && (maxtimerCounter < maxtimerCheck))
                {
                    //Restart the timer, still f/w update is in progress.
                    updateManager->secureUpdateTimer->start(
                           duration_cast<microseconds>(seconds(timerCheckFWUpdateSecs)));

                    maxtimerCounter++;

                    return;
                }
                
                flashSuceeded = true;
                progress = 100;
            }
        }
        updateManager->failUpdate(progress, msg, !flashSuceeded);
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("TimerCallBack ", entry("EXCEPTION=%s", e.what()));
    }
}

void PrisAPFWStateMachine::StateIdle([[maybe_unused]]MachineContext& ctx)
{
        auto updateManager = 
          std::any_cast<UpdateManager*>(fwUpdateMachine->myMachineContext.GetData(keyUpdateManager));

        updateManager->progress(10);  
}

void PrisAPFWStateMachine::StateCheckCECStatus([[maybe_unused]]MachineContext& ctx)
{
    bool stateSuccessful{true};
    uint8_t retVal = static_cast<uint8_t>(I2CCommLib::CommandStatus::UNKNOWN);
    std::string msg;

    maxtimerCounter = 0;

    try
    {
        retVal = deviceLayer->GetCECState();

        if (retVal != static_cast<uint8_t>(I2CCommLib::CommandStatus::SUCCESS))
        {
            stateSuccessful = false;
            if (retVal ==
                static_cast<uint8_t>(I2CCommLib::CommandStatus::ERR_BUSY))
            {
                log<level::ERR>("APFW:StateCheckCECStatus - CEC FW is BUSY.",
                                entry("ERR=0x%x", retVal));
                msg = "CECStatus: ERR_BUSY";
            }
            else
            {
                log<level::ERR>("APFW:StateCheckCECStatus - CEC FW OTHER ERR.",
                                entry("ERR=0x%x", retVal));
                msg = "CEC Update Status: " + deviceLayer->GetCommandStatusStr(retVal);
            }
        }
        auto updateManager = 
          std::any_cast<UpdateManager*>(fwUpdateMachine->myMachineContext.GetData(keyUpdateManager));

        updateManager->progress(10, msg);
    }
    catch (const std::exception& e)
    {
        msg += e.what();
        stateSuccessful = false;
        log<level::ERR>("APFW:StateCheckCECStatus - EXCEPTION.",
                        entry("EXCEPTION=%s", msg.c_str()));
    }

    try
    {
        if (!stateSuccessful)
        {
            myMachineContext.SetMachineFailed(msg);
            DoTransition(static_cast<uint8_t>(States::STATE_TERMINATE));
            return;
        }
        DoTransition(static_cast<uint8_t>(States::STATE_START_FW_UPDATE));
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("APFW:StateCheckCECStatus - Failed in state transition.",
                        entry("EXCEPTION=%s", e.what()));

        throw std::runtime_error(
            "APFW:StateCheckCECStatus: Failed in state transition");
    }
}

void PrisAPFWStateMachine::StateStartFwUpdate(MachineContext& ctx)
{

    bool stateSuccessful{true};
    uint8_t retVal = static_cast<uint8_t>(I2CCommLib::CommandStatus::UNKNOWN);
    std::string msg = "APFW:StateStartFwUpdate ";


    try
    {
        std::string imgFileName =
            std::any_cast<std::string>(ctx.GetData(keyFWImgName));

        uint32_t fileSize =
            std::any_cast<uint32_t>(ctx.GetData(keyFWFileSize));

        uint8_t  fwType = I2CCommLib::CEC_FW_ID;
        fs::path filePath(imgFileName);

        std::ifstream fwFile(imgFileName.c_str(), std::ios::binary);
        std::vector<char> imageData(I2CCommLib::OTA_HEADER_SIZE);

        if(fileSize < I2CCommLib::OTA_HEADER_SIZE)
        {
            throw std::runtime_error(
               "APFW:StateStartFwUpdate: Bad image");
        }


        if (filePath.extension() == swUpdate::romExtension) 
        {
           uint32_t otaHeaderOffset{0}; 

           if(fileSize > I2CCommLib::MB_SIZE)
           {
                otaHeaderOffset = I2CCommLib::OTA_HEADER_OFFSET_2MB_FILE_SIZE;
           }
           else
           {
                otaHeaderOffset = I2CCommLib::OTA_HEADER_OFFSET_1MB_FILE_SIZE;
           }    
           fwFile.seekg(0, std::ios::beg);
           fwFile.seekg(otaHeaderOffset);
           fwFile.read(imageData.data(), I2CCommLib::OTA_HEADER_SIZE);
        } 
        else if(filePath.extension() == swUpdate::binExtension)
        {
           fwFile.seekg(0, std::ios::beg);
           fwFile.read(imageData.data(), I2CCommLib::OTA_HEADER_SIZE);
        } 
        else 
        {
            std::string tmpErr{"APFW:StateStartFwUpdate: Invalid image, file: "};
            tmpErr += imgFileName;
            throw std::runtime_error(tmpErr.c_str());
        }

        uint32_t imgFileSize = I2CCommLib::OTA_HEADER_SIZE;
        imgFileSize += static_cast<uint32_t>(imageData[I2CCommLib::OTA_OFFSET_SIZE1]);
        imgFileSize += static_cast<uint32_t>(imageData[I2CCommLib::OTA_OFFSET_SIZE2]) << 8;
        imgFileSize += static_cast<uint32_t>(imageData[I2CCommLib::OTA_OFFSET_SIZE3]) << 16;
        imgFileSize += static_cast<uint32_t>(imageData[I2CCommLib::OTA_OFFSET_SIZE4]) << 24;


        ctx.SetData(keyActualFWSize, imgFileSize);

        deviceLayer->SendStartFWUpdate(imgFileSize, fwType);

        constexpr auto setWaitInSecs =
            std::chrono::milliseconds(PrisAPFWStateMachine::sleepBeforeRead);

        std::this_thread::sleep_for(setWaitInSecs);

        retVal = deviceLayer->GetLastCmdStatus();

        if (retVal != static_cast<uint8_t>(I2CCommLib::CommandStatus::SUCCESS))
        {
            stateSuccessful = false;
            log<level::ERR>(
                "APFW:StateStartFwUpdate - SendStartFWUpdate command failed.",
                entry("ERR=0x%x", retVal));
            msg += "StateStartFwUpdate - SendStartFWUpdate command failed.";
        }
        auto updateManager = 
          std::any_cast<UpdateManager*>(fwUpdateMachine->myMachineContext.GetData(keyUpdateManager));

        updateManager->progress(20, msg);
    }
    catch (const std::exception& e)
    {
        msg += e.what();
        stateSuccessful = false;
        log<level::ERR>("APFW:StateStartFwUpdate - EXCEPTION.",
                        entry("EXCEPTION=%s", msg.c_str()));
    }

    try
    {
        if (!stateSuccessful)
        {
            myMachineContext.SetMachineFailed(msg);
            DoTransition(static_cast<uint8_t>(States::STATE_TERMINATE));
            return;
        }
        DoTransition(static_cast<uint8_t>(States::STATE_COPY_IMAGE));
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("APFW:StateStartFwUpdate - Failed in state transition.",
                        entry("EXCEPTION=%s", e.what()));
        throw std::runtime_error(
            "APFW:StateStartFwUpdate: Failed in state transition");
    }
}

void PrisAPFWStateMachine::StateCopyImage(MachineContext& ctx)
{
    bool stateSuccessful{true};
    std::string msg{"APFW:StateCopyImage "};

    try
    {
        if(!fwUpdateManager)
	{
            log<level::ERR>("APFW:StateCopyImage - empty fwupdate manager.");
            throw std::runtime_error("APFW:StateCopyImage: Failed in state transition");
	}

        if (!fwUpdateManager->secureUpdateTimer)
        {
            fwUpdateManager->secureUpdateTimer =
                std::make_unique<Timer>(
                    fwUpdateManager->bus.get_event(),
                    PrisAPFWStateMachine::TimerCallBack);
        }

        std::string imgFileName =
            std::any_cast<std::string>(ctx.GetData(keyFWImgName));
 
        imgFileName = std::regex_replace(imgFileName, std::regex("-"), "\\x2D");
        std::replace(imgFileName.begin(), imgFileName.end(), '/', '-');

        uint32_t imgFileSize =
            std::any_cast<uint32_t>(ctx.GetData(keyActualFWSize));

        std::string copyImageServiceFile =
            std::string("nvidia-secure-block-copy@\\x2D") + "f\\x20" + imgFileName+
                                        "\\x20\\x2Ds\\x20"+ std::to_string(imgFileSize) +".service";

        ctx.SetData(keyCopyServiceNameString,copyImageServiceFile);

        auto method = fwUpdateManager->bus.new_method_call(
            SYSTEMD_BUSNAME, SYSTEMD_PATH, SYSTEMD_INTERFACE, "StartUnit");
        method.append(copyImageServiceFile, "replace");
        fwUpdateManager->bus.call_noreply(method);
    }
    catch (const std::exception& e)
    {
        msg += e.what();
        stateSuccessful = false;
        log<level::ERR>("APFW:StateCopyImage - EXCEPTION.",
                        entry("EXCEPTION=%s", msg.c_str()));
    }

    try
    {
        if (!stateSuccessful)
        {
            myMachineContext.SetMachineFailed(msg);
            DoTransition(static_cast<uint8_t>(States::STATE_TERMINATE));
            return;
        }
        auto updateManager = 
          std::any_cast<UpdateManager*>(fwUpdateMachine->myMachineContext.GetData(keyUpdateManager));

        updateManager->progress(50, "CEC Update status: start copy image");

        fwUpdateManager->secureUpdateTimer->start(
            duration_cast<microseconds>(seconds(timerExpirySecs)));
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("APFW:StateCopyImage - Failed in state transition.",
                        entry("EXCEPTION=%s", e.what()));
        throw std::runtime_error("APFW:StateCopyImage: Failed in state transition");
    }
}

void PrisAPFWStateMachine::StateCheckUpdateStatus([[maybe_unused]]MachineContext& ctx)
{
    bool stateSuccessful{true};
    bool updateInProgress{false};
    std::string msg{" "};
    uint8_t retVal = static_cast<uint8_t>(
        I2CCommLib::FirmwareUpdateStatus::STATUS_CODE_OTHER);

    try
    {
        retVal = deviceLayer->GetFWUpdateStatus();

        if (retVal ==
            static_cast<uint8_t>(
                I2CCommLib::FirmwareUpdateStatus::STATUS_UPDATE_IN_PROGRESS))
        {
            msg = checkUpdateInProgress;
            updateInProgress = true;
        }
        else
        {
           if (retVal !=
              static_cast<uint8_t>(
                I2CCommLib::FirmwareUpdateStatus::STATUS_UPDATE_FINISH))
           {
               stateSuccessful = false;
               log<level::ERR>("APFW:StateCheckUpdateStatus - Firmware update failed.",
                            entry("ERR=0x%x", retVal));
                msg = "CEC Update Check: Firmware update failed with status: update didn't finished (" + std::to_string(retVal) + ")";
           }
           else
           {
               msg += "StateCheckUpdateStatus - Frimware update succeeded";
           }
        } 
    }
    catch (const std::exception& e)
    {
        msg += "EXCEPTION: " + std::string(e.what());
        stateSuccessful = false;
        log<level::ERR>("APFW:StateCheckUpdateStatus - EXCEPTION.",
                        entry("EXCEPTION=%s", msg.c_str()));
    }

    try
    {
        if (!stateSuccessful)
        {
            myMachineContext.SetMachineFailed(msg);
        }
        else
        {
            myMachineContext.SetMachineSucceeded(msg);
        }

        auto updateManager = 
          std::any_cast<UpdateManager*>(fwUpdateMachine->myMachineContext.GetData(keyUpdateManager));

        updateManager->progress(90, msg);

        if(!updateInProgress)
        {
           DoTransition(static_cast<uint8_t>(States::STATE_TERMINATE));
        }
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("APFW:StateCheckUpdateStatus - Failed in state transition.",
                        entry("EXCEPTION=%s", e.what()));

        throw std::runtime_error(
            "APFW:StateCheckUpdateStatus: Failed in state transition");
    }
}

void PrisAPFWStateMachine::StateTerminate([[maybe_unused]]MachineContext& ctx)
{

    bool stateSuccessful{true};
    std::string msg{" "};
    std::string errMsg{" "};


    std::tuple<bool, std::string> ret = myMachineContext.GetMachineRunStatus();
    bool smRunSuceeded = std::get<0>(ret);

    if (!smRunSuceeded)
    {
        std::string msgFromOtherState = std::get<1>(ret);
        log<level::ERR>(
            "APFW:StateTerminate - Firmware update failed in other state.",
            entry("FAILURE=%s", msgFromOtherState.c_str()));
        errMsg += msgFromOtherState;
        stateSuccessful = false;
    }
    else
    {
        try
        {
            log<level::DEBUG>("APFW:StateTerminate - Firmware update succeeded.");
        }
        catch (const std::exception& e)
        {
            msg += e.what();
            stateSuccessful = false;
            log<level::ERR>("APFW:StateTerminate - EXCEPTION.",
                            entry("EXCEPTION=%s", msg.c_str()));
        }
    }

    try
    {
        if (!stateSuccessful)
        {
            errMsg += msg;
            myMachineContext.SetMachineFailed(errMsg);
            return;
        }
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("APFW:StateTerminate - Failed in state transition.",
                        entry("EXCEPTION=%s", e.what()));
        throw std::runtime_error("APFW:StateTerminate: Failed in state transition");
    }
}

} // namespace updater
} // namespace software
} // namespace phosphor
