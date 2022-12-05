#pragma once

#include "state_machine_context.hpp"

#include <stdio.h>

#include <functional>
#include <typeinfo>

namespace phosphor
{
namespace software
{
namespace updater
{

class StateMachine;

class StateFunc
{
  public:
    virtual void RunState(StateMachine& machine,
                          MachineContext& context) const = 0;
    virtual ~StateFunc() = default;
};

template <class StateMachineClass, class MachineContextClass,
          void (StateMachineClass::*Method)(MachineContextClass&)>

class StateFuncEx : public StateFunc
{
  public:
    virtual void RunState(StateMachine& machine, MachineContext& context) const
    {
        auto derivedSM = dynamic_cast<StateMachineClass*>(&machine);

        if (derivedSM == nullptr)
        {
            throw std::runtime_error(
                "RunState: Error, the state class object \
                                               is of different type.");
        }

        auto derivedContext =
            dynamic_cast<const MachineContextClass*>(&context);

        if (derivedContext == nullptr)
        {
            throw std::runtime_error(
                "RunState: Error, the state context object \
                                               is of different type.");
        }

        (derivedSM->*Method)(context);
    }
};

/** @class StateMachine
 *
 *  @brief StateMachine for secure firmware
 *
 *  Handles the secure BMC firmware update flow.
 *  Interacts with CEC controller and accompolishes the secure firmware update.
 */
class StateMachine
{

  public:
    StateMachine(uint8_t maxStates, MachineContext& ctx,
                 uint8_t initialState = 0);

    virtual ~StateMachine()
    {
    }

    uint8_t GetCurrentState()
    {
        return currentState;
    }

    uint8_t GetMaxStates()
    {
        return maxNumStates;
    }

    virtual void TriggerFWUpdate() = 0;

    virtual void TriggerState(uint8_t newState) = 0;

  protected:
    void StartMachine(uint8_t newState);

    void DoTransition(uint8_t nState);

  private:
    const uint8_t maxNumStates;

    uint8_t currentState;

    uint8_t newState;

    bool transitionFired;

  public:
    MachineContext& myMachineContext;

    enum class StateCapacity
    {
        MAX_STATE_CAPACITY = 0xFE,
        STATE_BOUNDARY_ERR
    };

  private:
    virtual const std::vector<StateFunc*>& GetStateFlow() = 0;

    void SetCurrentState(uint8_t nState)
    {
        currentState = nState;
    }

    void RunMachine();
};

} // namespace updater
} // namespace software
} // namespace phosphor
