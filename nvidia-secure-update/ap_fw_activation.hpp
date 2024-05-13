#pragma once

#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/ActivationProgress/server.hpp>

#include <string>

namespace phosphor
{
namespace software
{
namespace firmwareupdater
{

class UpdateManager;

using ActivationIntf = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::Activation>;
using ActivationProgressIntf = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::ActivationProgress>;

/** @class ActivationProgress
 *
 *  Concrete implementation of xyz.openbmc_project.Software.ActivationProgress
 *  D-Bus interface
 */
class ApFwActivationProgress : public ActivationProgressIntf
{
  public:
    /** @brief Constructor
     *
     * @param[in] bus - Bus to attach to
     * @param[in] objPath - D-Bus object path
     */
    ApFwActivationProgress(sdbusplus::bus::bus& bus,
                           const std::string& objPath) :
        ActivationProgressIntf(bus, objPath.c_str(),
                               action::emit_interface_added)
    {
        progress(0);
    }
};

/** @class Activation
 *
 *  Concrete implementation of xyz.openbmc_project.Object.Activation D-Bus
 *  interface
 */
class ApFwActivation : public ActivationIntf
{
  public:
    using sdbusplus::xyz::openbmc_project::Software::server::Activation::
        activation;
    using sdbusplus::xyz::openbmc_project::Software::server::Activation::
        requestedActivation;

    /** @brief Constructor
     *
     *  @param[in] bus - Bus to attach to
     *  @param[in] objPath - D-Bus object path
     *  @param[in] activationState - Activation state
     *  @param[in] requestedActivationState - Requested activation state
     *  @param[in] updateManager - Reference to FW update manager
     */
    ApFwActivation(sdbusplus::bus::bus& bus, std::string objPath,
                   Activations activationState,
                   RequestedActivations requestedActivationState,
                   UpdateManager* updateManager) :
        ActivationIntf(bus, objPath.c_str(),
                       ActivationIntf::action::defer_emit),
        bus(bus), objPath(objPath), updateManager(updateManager)
    {
        activation(activationState);
        requestedActivation(requestedActivationState);
        emit_object_added();
    }

    /** @brief Overriding RequestedActivations property setter function
     */
    RequestedActivations
        requestedActivation(RequestedActivations value) override
    {
        if ((value == RequestedActivations::Active) &&
            (requestedActivation() != RequestedActivations::Active))
        {
            if ((ActivationIntf::activation() == Activations::Ready))
            {
                activation(Activations::Activating);
            }
            else if ((ActivationIntf::activation() == Activations::Invalid))
            {
                activation(Activations::Failed);
            }
        }
        return ActivationIntf::requestedActivation(value);
    }

  private:
    sdbusplus::bus::bus& bus;
    const std::string objPath;
    UpdateManager* updateManager;
};

} // namespace firmwareupdater
} // namespace software
} // namespace phosphor
