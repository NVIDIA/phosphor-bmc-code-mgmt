#include "item_updater_helper.hpp"

#include "utils.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{

void Helper::setEntry(const std::string& /* entryId */, uint8_t /* value */)
{
    // Empty
}

void Helper::clearEntry(const std::string& /* entryId */)
{
    // Empty
}

void Helper::cleanup()
{
    // Empty
}

void Helper::factoryReset()
{
#ifdef NVIDIA_SECURE_BOOT
    // The shutdown will cleanup rwfs during reboot.
    utils::execute("/bin/touch", "/run/factory-reset");

    // Set vendorfieldmode=disabled env in U-Boot.
    // This will disable the vendor field mode settings.
    utils::execute("/sbin/fw_setenv", "vendorfieldmode", "disabled");
#else
    // Set openbmconce=factory-reset env in U-Boot.
    // The init will cleanup rwfs during boot.
    utils::execute("/sbin/fw_setenv", "openbmconce", "factory-reset");
#endif
}

void Helper::removeVersion(const std::string& /* flashId */)
{
    // Empty
}

void Helper::updateUbootVersionId(const std::string& /* flashId */)
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
