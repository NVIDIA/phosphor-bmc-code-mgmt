#pragma once

#include "i2c_comm_lib.hpp"
// #include "images.hpp"
#include "ap_fw_updater.hpp"
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
namespace firmwareupdater
{

static const std::string keyFWFileSize{"fw_file_size"};

static const std::string keyFWImgName{"fw_img_name"};

static const std::string keyActualFWSize{"fw_actual_size"};

static const std::string keyUpdateManager{"fw_update_manager"};

static const std::string keyCopyServiceNameString{
    "fw_copy_service_name_string"};

static const std::string checkUpdateInProgress{"AP FIRMWARE UPDATE IN PROGESS"};

static const std::string cecFWFolder{"/tmp/cec_images/"};

static const std::string romExtension{".rom"};

static const std::string binExtension{".bin"};

using namespace phosphor::software::updater;

class PrisAPFWStateMachine : public StateMachine
{
  public:
    PrisAPFWStateMachine(MachineContext& ctx);

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

    void StateCheckUpdateStatus(MachineContext&);

    void StateTerminate(MachineContext&);

    // Define StateFunction object which will be called from the StateMachine
    StateFuncEx<PrisAPFWStateMachine, MachineContext,
                &PrisAPFWStateMachine::StateIdle>
        FuncIdle;

    StateFuncEx<PrisAPFWStateMachine, MachineContext,
                &PrisAPFWStateMachine::StateCheckCECStatus>
        FuncCheckCECStatus;

    StateFuncEx<PrisAPFWStateMachine, MachineContext,
                &PrisAPFWStateMachine::StateStartFwUpdate>
        FuncStartFwUpdate;

    StateFuncEx<PrisAPFWStateMachine, MachineContext,
                &PrisAPFWStateMachine::StateCopyImage>
        FuncCopyImage;

    StateFuncEx<PrisAPFWStateMachine, MachineContext,
                &PrisAPFWStateMachine::StateCheckUpdateStatus>
        FuncCheckUpdateStatus;

    StateFuncEx<PrisAPFWStateMachine, MachineContext,
                &PrisAPFWStateMachine::StateTerminate>
        FuncTerminate;

  private:
    static void TimerCallBack();

    static bool RunCheckUpdateStatus();

    // void HandleCecInterrupt(sdbusplus::message::message& msg);

  private:
    virtual const std::vector<StateFunc*>& GetStateFlow();

    std::string i2DeviceName;

    std::unique_ptr<I2CCommLib> deviceLayer;

    // variable to track timer counter
    inline static uint8_t maxtimerCounter{0};

    std::vector<StateFunc*> stateFlowSequence;

  public:
    static constexpr uint8_t busIdentifier = CEC_BUS_IDENTIFIER;

    static constexpr uint8_t deviceAddrress = CEC_DEVICE_ADDRESS;

    // in milliseconds
    static constexpr uint8_t sleepBeforeRead{100};

    // in seconds
    static constexpr uint16_t timerExpirySecs{2100};

    // in seconds
    static constexpr uint16_t timerCheckFWUpdateSecs{60};

    // no of times timer will get re-armed
    static constexpr uint8_t maxtimerCheck{20};

    UpdateManager* fwUpdateManager;
};

} // namespace firmwareupdater
} // namespace software
} // namespace phosphor
