#pragma once

#include "../activation.hpp"

#include <stdio.h>

#include <any>
#include <functional>
#include <typeinfo>

namespace phosphor
{
namespace software
{
namespace updater
{

class MachineContext
{
  public:
    MachineContext(Activation* activationObj);

    virtual ~MachineContext(){};

    void SetMachineFailed(std::string& msg);

    void SetMachineSucceeded(std::string& msg);

    std::tuple<bool, std::string> GetMachineRunStatus()
    {
        return std::make_tuple(statemachineSuceeded, messageStr);
    }

    void SetData(const std::string& key, const std::any& obj);

    void ClearData()
    {
        dataMap.clear();
    }

    std::any& GetData(const std::string& key);

  public:
    Activation* activationObject;

  protected:
    std::string messageStr;

    bool statemachineSuceeded;

    std::map<std::string, std::any> dataMap;
};

} // namespace updater
} // namespace software
} // namespace phosphor
