#include "state_machine.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{
StateMachine::StateMachine(uint8_t maxStates, MachineContext& ctx,
                           uint8_t initialState) :
    maxNumStates(maxStates), currentState(initialState), newState(initialState),
    transitionFired(false), myMachineContext(ctx)
{
    if (maxNumStates > static_cast<uint8_t>(StateCapacity::MAX_STATE_CAPACITY))
    {
        std::string msg = "StateMachine: Error, max state count " +
                          maxNumStates;
        msg += "exceeds max value";
        throw std::runtime_error(msg.c_str());
    }
}

void StateMachine::StartMachine(uint8_t newState)
{
    if (newState >= maxNumStates)
    {
        std::string msg = "StartMachine: New State " + newState;
        msg += " is out of bound " + maxNumStates;
        throw std::runtime_error(msg.c_str());
    }

    // Initial Transition, move from IDLE(inital state)
    // to the state being called.
    DoTransition(newState);

    // Run the State Machine.This function will run state function,
    // as directed by DoTransition till reaches the End state where
    // no further transition were made.
    RunMachine();
}

void StateMachine::DoTransition(uint8_t nState)
{
    transitionFired = true;
    newState = nState;
}

void StateMachine::RunMachine()
{
    const std::vector<StateFunc*> pStateFlow = GetStateFlow();
    if (pStateFlow.empty())
    {
        std::string msg = "RunMachine: State Flow object is empty";
        throw std::runtime_error(msg.c_str());
    }

    while (transitionFired)
    {
        if (newState > maxNumStates)
        {
            std::string msg = "RunMachine: New State " + newState;
            msg += " is out of bound " + maxNumStates;
            throw std::runtime_error(msg.c_str());
        }

        const StateFunc* state = pStateFlow[newState];
        // const auto state = pStateFlow[newState];

        transitionFired = false;

        SetCurrentState(newState);

        if (state == NULL)
        {
            std::string msg = "RunMachine: State object is empty";
            throw std::runtime_error(msg.c_str());
        }

        state->RunState(*this, myMachineContext);
    }
}

} // namespace updater
} // namespace software
} // namespace phosphor
