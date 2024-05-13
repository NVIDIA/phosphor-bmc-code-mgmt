#include "config.h"

#include "item_updater_helper.hpp"

#include "utils.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{
// vendorfieldmode=disabled
#define ENV_DISABLE_VENDOR_FIELD_MODE "vendorfieldmode\\x3ddisabled"
#define SERVICE_DISABLE_VENDOR_FIELD_MODE                                      \
    "obmc-flash-bmc-setenv@" ENV_DISABLE_VENDOR_FIELD_MODE ".service"
// openbmconce=clean-rwfs-filesystem factory-reset
#define ENV_FACTORY_RESET "openbmconce\\x3dfactory\\x2dreset"
#define SERVICE_FACTORY_RESET                                                  \
    "obmc-flash-bmc-setenv@" ENV_FACTORY_RESET ".service"

void Helper::setEntry([[maybe_unused]] const std::string& entryId,
                      [[maybe_unused]] uint8_t value)
{
    // Empty
}

void Helper::clearEntry([[maybe_unused]] const std::string& entryId)
{
    // Empty
}

void Helper::cleanup()
{
    // Empty
}

void Helper::factoryReset()
{
    // The shutdown will cleanup rwfs during reboot.
    utils::execute("/bin/touch", "/run/factory-reset");
}

void Helper::removeVersion([[maybe_unused]] const std::string& versionId)
{
    // Empty
}

void Helper::updateUbootVersionId([[maybe_unused]] const std::string& versionId)
{
    // Empty
}

void Helper::mirrorAlt()
{
    // Empty
}

} // namespace updater
} // namespace software
} // namespace phosphor
