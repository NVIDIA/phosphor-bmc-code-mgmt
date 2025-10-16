#include "pris_state_machine.hpp"

#include "state_machine.hpp"

#include <sys/inotify.h>
#include <unistd.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/exception.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>

namespace phosphor
{
namespace software
{
namespace updater
{
extern std::unique_ptr<StateMachine> fwUpdateMachine;

using namespace phosphor::logging;
using namespace std::chrono;
using namespace phosphor::software::updater;
namespace match_rules = sdbusplus::bus::match::rules;

PrisStateMachine::PrisStateMachine(MachineContext& ctx) :
    StateMachine(static_cast<uint8_t>(States::MAX_STATES), ctx,
                 static_cast<uint8_t>(States::STATE_IDLE))
{
    deviceLayer = std::make_unique<I2CCommLib>(
        PrisStateMachine::busIdentifier, PrisStateMachine::deviceAddrress);

    stateFlowSequence = {
        &FuncIdle,      &FuncCheckCECStatus,   &FuncStartFwUpdate,
        &FuncCopyImage, &FuncSendCopyComplete, &FuncCheckUpdateStatus,
        &FuncTerminate};
}

const std::vector<StateFunc*>& PrisStateMachine::GetStateFlow()
{
    return stateFlowSequence;
}

void PrisStateMachine::TriggerFWUpdate()
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

void PrisStateMachine::TriggerState(uint8_t newState)
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

void PrisStateMachine::HandleCecInterrupt(
    [[maybe_unused]] sdbusplus::message::message& msg)
{
    bool flashSuceeded = false;

    try
    {
        if (GetCurrentState() ==
            (static_cast<uint8_t>(States::STATE_SEND_COPY_COMPLETE)))
        {
            flashSuceeded = RunCheckUpdateStatus();
            if (flashSuceeded)
            {
                myMachineContext.activationObject->failActivation(false);
                return;
            }
        }
        if (!flashSuceeded)
        {
            myMachineContext.activationObject->failActivation();
        }
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("HandleCecInterrupt ", entry("EXCEPTION=%s", e.what()));
    }
}

bool PrisStateMachine::RunCheckUpdateStatus()
{
    bool flashSuceeded = false;

    try
    {
        if (fwUpdateMachine &&
            fwUpdateMachine->GetCurrentState() ==
                (static_cast<uint8_t>(States::STATE_SEND_COPY_COMPLETE)))
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

void PrisStateMachine::TimerCallBack(const boost::system::error_code& ec)
{
    /* The TimerCallback function is necessary only when the timer has expired.
    Returns when the timer is cancelled */
    if (ec == boost::asio::error::operation_aborted)
    {
        return;
    }

    log<level::ERR>("Secure Timer has expired");
    bool flashSuceeded = false;

    try
    {
        if (fwUpdateMachine &&
            fwUpdateMachine->GetCurrentState() ==
                (static_cast<uint8_t>(States::STATE_SEND_COPY_COMPLETE)))
        {
            flashSuceeded = RunCheckUpdateStatus();
            if (flashSuceeded)
            {
                fwUpdateMachine->myMachineContext.activationObject
                    ->failActivation(false);
                return;
            }
        }
        if (!flashSuceeded)
        {
            fwUpdateMachine->myMachineContext.activationObject
                ->failActivation();
        }
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("TimerCallBack ", entry("EXCEPTION=%s", e.what()));
    }
}

void PrisStateMachine::StateIdle([[maybe_unused]] MachineContext& ctx)
{
    log<level::DEBUG>("StateIdle Function ");
}

void PrisStateMachine::StateCheckCECStatus([[maybe_unused]] MachineContext& ctx)
{
    bool stateSuccessful{true};
    uint8_t retVal = static_cast<uint8_t>(I2CCommLib::CommandStatus::UNKNOWN);

    std::string msg = "StateCheckCECStatus: ";

    try
    {
        retVal = deviceLayer->GetCECState();

        if (retVal != static_cast<uint8_t>(I2CCommLib::CommandStatus::SUCCESS))
        {
            stateSuccessful = false;
            if (retVal ==
                static_cast<uint8_t>(I2CCommLib::CommandStatus::ERR_BUSY))
            {
                log<level::ERR>("StateCheckCECStatus - CEC FW is BUSY.",
                                entry("ERR=0x%x", retVal));
                msg += "StateCheckCECStatus - CEC FW is BUSY.";
            }
            else
            {
                log<level::ERR>("StateCheckCECStatus - CEC FW OTHER ERR.",
                                entry("ERR=0x%x", retVal));
                msg += "StateCheckCECStatus - CEC FW OTHER ERR.";
            }
        }
        myMachineContext.activationObject->activationProgress->progress(10);
    }
    catch (const std::exception& e)
    {
        msg += e.what();
        stateSuccessful = false;
        log<level::ERR>("StateCheckCECStatus - EXCEPTION.",
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
        log<level::ERR>("StateCheckCECStatus - Failed in state transition.",
                        entry("EXCEPTION=%s", e.what()));

        throw std::runtime_error(
            "StateCheckCECStatus: Failed in state transition");
    }
}

void PrisStateMachine::StateStartFwUpdate(MachineContext& ctx)
{
    bool stateSuccessful{true};
    uint8_t retVal = static_cast<uint8_t>(I2CCommLib::CommandStatus::UNKNOWN);
    std::string msg = "StateStartFwUpdate ";

    try
    {
        uint32_t imgFileSize =
            std::any_cast<uint32_t>(ctx.GetData(keyBmcImgSize));

        deviceLayer->SendStartFWUpdate(imgFileSize);

        constexpr auto setWaitInSecs =
            std::chrono::milliseconds(PrisStateMachine::sleepBeforeRead);

        std::this_thread::sleep_for(setWaitInSecs);

        retVal = deviceLayer->GetLastCmdStatus();

        if (retVal != static_cast<uint8_t>(I2CCommLib::CommandStatus::SUCCESS))
        {
            stateSuccessful = false;
            log<level::ERR>(
                "StateStartFwUpdate - SendStartFWUpdate command failed.",
                entry("ERR=0x%x", retVal));
            msg += "StateStartFwUpdate - SendStartFWUpdate command failed.";
        }
        myMachineContext.activationObject->activationProgress->progress(20);
    }
    catch (const std::exception& e)
    {
        msg += e.what();
        stateSuccessful = false;
        log<level::ERR>("StateStartFwUpdate - EXCEPTION.",
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
        log<level::ERR>("StateStartFwUpdate - Failed in state transition.",
                        entry("EXCEPTION=%s", e.what()));
        throw std::runtime_error(
            "StateStartFwUpdate: Failed in state transition");
    }
}

void PrisStateMachine::StateCopyImage([[maybe_unused]] MachineContext& ctx)
{
    bool stateSuccessful{true};
    std::string msg{"StateCopyImage "};

    try
    {
        if (!myMachineContext.activationObject->secureUpdateTimer)
        {
            sdbusplus::asio::connection& connectionBus =
                static_cast<sdbusplus::asio::connection&>(
                    myMachineContext.activationObject->bus);

            myMachineContext.activationObject->secureUpdateTimer =
                std::make_unique<boost::asio::steady_timer>(
                    connectionBus.get_io_context());
        }

        auto copyImageServiceFile =
            "obmc-secure-copy-image@" +
            myMachineContext.activationObject->versionId + ".service";
        auto method = myMachineContext.activationObject->bus.new_method_call(
            SYSTEMD_BUSNAME, SYSTEMD_PATH, SYSTEMD_INTERFACE, "StartUnit");
        method.append(copyImageServiceFile, "replace");
        myMachineContext.activationObject->bus.call_noreply(method);
    }
    catch (const std::exception& e)
    {
        msg += e.what();
        stateSuccessful = false;
        log<level::ERR>("StateCopyImage - EXCEPTION.",
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
        myMachineContext.activationObject->activationProgress->progress(30);

        myMachineContext.activationObject->secureUpdateTimer->expires_after(
            std::chrono::seconds(timerExpirySecs));
        myMachineContext.activationObject->secureUpdateTimer->async_wait(
            TimerCallBack);
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("StateCopyImage - Failed in state transition.",
                        entry("EXCEPTION=%s", e.what()));
        throw std::runtime_error("StateCopyImage: Failed in state transition");
    }
}

void PrisStateMachine::StateSendCopyComplete(
    [[maybe_unused]] MachineContext& ctx)
{
    bool stateSuccessful{true};
    std::string msg{"StateSendCopyComplete "};
    uint8_t retVal = static_cast<uint8_t>(I2CCommLib::CommandStatus::UNKNOWN);

    try
    {
        deviceLayer->SendCopyImageComplete();

        constexpr auto setWaitInSecs =
            std::chrono::milliseconds(PrisStateMachine::sleepBeforeRead);

        std::this_thread::sleep_for(setWaitInSecs);

        retVal = deviceLayer->GetLastCmdStatus();

        if (retVal != static_cast<uint8_t>(I2CCommLib::CommandStatus::SUCCESS))
        {
            stateSuccessful = false;
            log<level::ERR>(
                "StateSendCopyComplete - SendCopyImageComplete command failed.",
                entry("ERR=0x%x", retVal));
            msg +=
                "StateSendCopyComplete - SendCopyImageComplete command failed.";
        }
    }
    catch (const std::exception& e)
    {
        msg += e.what();
        stateSuccessful = false;
        log<level::ERR>("StateSendCopyComplete - EXCEPTION.",
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

        myMachineContext.activationObject->activationProgress->progress(50);

        if (!activeCecInterruptSignal)
        {
            activeCecInterruptSignal =
                std::make_unique<sdbusplus::bus::match::match>(
                    myMachineContext.activationObject->bus,
                    match_rules::propertiesChanged("/com/nvidia/secureboot",
                                                   "com.nvidia.Secureboot.Cec"),
                    std::bind(
                        std::mem_fn(&PrisStateMachine::HandleCecInterrupt),
                        this, std::placeholders::_1));
        }
        myMachineContext.activationObject->secureUpdateTimer->expires_after(
            std::chrono::seconds(timerExpirySecs));
        myMachineContext.activationObject->secureUpdateTimer->async_wait(
            TimerCallBack);
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("StateSendCopyComplete - Failed in state transition.",
                        entry("EXCEPTION=%s", e.what()));

        throw std::runtime_error(
            "StateSendCopyComplete: Failed in state transition");
    }
}

void PrisStateMachine::StateCheckUpdateStatus(
    [[maybe_unused]] MachineContext& ctx)
{
    bool stateSuccessful{true};
    std::string msg{" "};
    uint8_t retVal = static_cast<uint8_t>(
        I2CCommLib::FirmwareUpdateStatus::STATUS_CODE_OTHER);

    try
    {
        myMachineContext.activationObject->secureUpdateTimer->cancel();

        retVal = deviceLayer->GetFWUpdateStatus();

        if (retVal !=
            static_cast<uint8_t>(
                I2CCommLib::FirmwareUpdateStatus::STATUS_UPDATE_FINISH))
        {
            stateSuccessful = false;
            log<level::ERR>("StateCheckUpdateStatus - Firmware update failed.",
                            entry("ERR=0x%x", retVal));
            msg += "StateCheckUpdateStatus - Firmware update failed.";
        }
        if (stateSuccessful)
        {
            retVal =
                static_cast<uint8_t>(I2CCommLib::CECInterruptStatus::UNKNOWN);
            retVal = deviceLayer->QueryAboutInterrupt();
            if (retVal ==
                static_cast<uint8_t>(
                    I2CCommLib::CECInterruptStatus::BMC_FW_UPDATE_FAIL))
            {
                stateSuccessful = false;
                log<level::ERR>(
                    "StateCheckUpdateStatus - Firmware update failed.",
                    entry("ERR=0x%x", retVal));
                msg += "StateCheckUpdateStatus - Firmware update failed.";
            }
            else if (retVal ==
                     static_cast<uint8_t>(I2CCommLib::CECInterruptStatus::
                                              BMC_FW_UPDATE_REQUEST_RESET_NOW))
            {
                log<level::DEBUG>("StateCheckUpdateStatus - Firmware update "
                                  "succeeded,immediate reset expected.");
                doReset = true;
            }
            else
            {
                log<level::DEBUG>(
                    "StateCheckUpdateStatus - Firmware update succeeded.");
            }
        }
    }
    catch (const std::exception& e)
    {
        msg += e.what();
        stateSuccessful = false;
        log<level::ERR>("StateCheckUpdateStatus - EXCEPTION.",
                        entry("EXCEPTION=%s", msg.c_str()));
    }

    try
    {
        if (!stateSuccessful)
        {
            myMachineContext.SetMachineFailed(msg);
        }

        myMachineContext.activationObject->activationProgress->progress(90);
        DoTransition(static_cast<uint8_t>(States::STATE_TERMINATE));
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("StateCheckUpdateStatus - Failed in state transition.",
                        entry("EXCEPTION=%s", e.what()));

        throw std::runtime_error(
            "StateCheckUpdateStatus: Failed in state transition");
    }
}

void PrisStateMachine::StateTerminate([[maybe_unused]] MachineContext& ctx)
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
            "StateTerminate - Firmware update failed in other state.",
            entry("FAILURE=%s", msgFromOtherState.c_str()));
        errMsg += msgFromOtherState;
        stateSuccessful = false;
    }
    else
    {
        try
        {
            log<level::DEBUG>("StateTerminate - Firmware update succeeded.");
            if (doReset)
            {
                // TODO: this will not happen in InBand update.
                // myMachineContext.activationObject->rebootBmc();
            }
        }
        catch (const std::exception& e)
        {
            msg += e.what();
            stateSuccessful = false;
            log<level::ERR>("StateTerminate - EXCEPTION.",
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
        log<level::ERR>("StateTerminate - Failed in state transition.",
                        entry("EXCEPTION=%s", e.what()));
        throw std::runtime_error("StateTerminate: Failed in state transition");
    }
}

} // namespace updater
} // namespace software
} // namespace phosphor
