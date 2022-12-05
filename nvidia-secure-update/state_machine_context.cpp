#include "state_machine_context.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{

MachineContext::MachineContext(Activation* activationObj)
{
    activationObject = activationObj;
    statemachineSuceeded = true;
}

void MachineContext::SetMachineFailed(std::string& msg)
{
    statemachineSuceeded = false;
    messageStr = msg;
}

void MachineContext::SetMachineSucceeded(std::string& msg)
{
    statemachineSuceeded = true;
    messageStr = msg;
}


void MachineContext::SetData(const std::string& key, const std::any& obj)
{
    dataMap[key] = obj;
}

std::any& MachineContext::GetData(const std::string& key)
{
    return dataMap[key];
}

} // namespace updater
} // namespace software
} // namespace phosphor
