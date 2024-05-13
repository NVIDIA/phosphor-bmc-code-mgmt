#pragma once

#include "i2c_comm_lib.hpp"
#include "images.hpp"
#include "state_machine.hpp"

#include <stdio.h>

#include <sdbusplus/bus/match.hpp>

#include <array>
#include <functional>
#include <typeinfo>

namespace phosphor
{
namespace software
{
namespace updater
{
static const std::string keyBmcImgSize{"bmc_img_size"};

static const std::string keyBmcImgName{"bmc_img_name"};

static const std::string fwSecureBmcImageName{
    phosphor::software::image::SECURE_IMAGE_NAME};

class PrisMachineContext : public MachineContext
{
  public:
    PrisMachineContext(Activation* activationObj) :
        MachineContext(activationObj) {

        };
};

class PrisStateMachine : public StateMachine
{
  public:
    PrisStateMachine(MachineContext& ctx);

    virtual void TriggerFWUpdate();

    virtual void TriggerState(uint8_t newState);

    // State enumeration order must match the order of state method entries
    // in the state map.
    enum class States
    {
        STATE_IDLE = 0,
        STATE_CHECK_CEC_STATUS,
        STATE_START_FW_UPDATE,
        STATE_COPY_IMAGE,
        STATE_SEND_COPY_COMPLETE,
        STATE_CHECK_UPDATE_STATUS,
        STATE_TERMINATE,
        MAX_STATES
    };

  private:
    // State machine functions with MachineContext
    void StateIdle(MachineContext&);

    void StateCheckCECStatus(MachineContext&);

    void StateStartFwUpdate(MachineContext&);

    void StateCopyImage(MachineContext&);

    void StateSendCopyComplete(MachineContext&);

    void StateCheckUpdateStatus(MachineContext&);

    void StateTerminate(MachineContext&);

    // Define StateFunction object which will be called from the StateMachine
    StateFuncEx<PrisStateMachine, MachineContext, &PrisStateMachine::StateIdle>
        FuncIdle;

    StateFuncEx<PrisStateMachine, MachineContext,
                &PrisStateMachine::StateCheckCECStatus>
        FuncCheckCECStatus;

    StateFuncEx<PrisStateMachine, MachineContext,
                &PrisStateMachine::StateStartFwUpdate>
        FuncStartFwUpdate;

    StateFuncEx<PrisStateMachine, MachineContext,
                &PrisStateMachine::StateCopyImage>
        FuncCopyImage;

    StateFuncEx<PrisStateMachine, MachineContext,
                &PrisStateMachine::StateSendCopyComplete>
        FuncSendCopyComplete;

    StateFuncEx<PrisStateMachine, MachineContext,
                &PrisStateMachine::StateCheckUpdateStatus>
        FuncCheckUpdateStatus;

    StateFuncEx<PrisStateMachine, MachineContext,
                &PrisStateMachine::StateTerminate>
        FuncTerminate;

  private:
    static void TimerCallBack(const boost::system::error_code& ec);

    static bool RunCheckUpdateStatus();

    void HandleCecInterrupt(sdbusplus::message::message& msg);

  private:
    virtual const std::vector<StateFunc*>& GetStateFlow();

    std::string i2DeviceName;

    std::unique_ptr<I2CCommLib> deviceLayer;

    static constexpr uint8_t busIdentifier = CEC_BUS_IDENTIFIER;

    static constexpr uint8_t deviceAddrress = CEC_DEVICE_ADDRESS;

    bool doReset = false;

    // in milliseconds
    static constexpr uint8_t sleepBeforeRead{100};

    // in seconds
    static constexpr uint16_t timerExpirySecs{2400};

    std::unique_ptr<sdbusplus::bus::match_t> activeCecInterruptSignal;

    std::vector<StateFunc*> stateFlowSequence;
};

} // namespace updater
} // namespace software
} // namespace phosphor
