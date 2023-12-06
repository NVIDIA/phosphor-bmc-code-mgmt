#include "config.h"

#include <string>
#include <vector>

namespace phosphor
{
namespace software
{
namespace image
{

#ifdef NVIDIA_SECURE_BOOT
constexpr auto SECURE_IMAGE_NAME = "image-bmc";

const std::vector<std::string> bmcImages = {"image-kernel", "image-rofs",
                                            "image-rwfs", "image-u-boot",
                                            SECURE_IMAGE_NAME};
#else

// BMC flash image file name list.
const std::vector<std::string> bmcImages = {"image-kernel", "image-rofs",
                                            "image-rwfs", "image-u-boot"};
#endif
// BMC flash image file name list for full flash image (image-bmc)
const std::string bmcFullImages = {"image-bmc"};

std::vector<std::string> getOptionalImages();

} // namespace image
} // namespace software
} // namespace phosphor
