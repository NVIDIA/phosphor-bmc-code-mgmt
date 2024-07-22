/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// oem/nvidia/completeReset_utils.hpp
#pragma once

#include "utils.hpp"

namespace completeReset_utils
{
void checkAndSetEmmcLoggingErase()
{
    // Check if the service is active
    auto [rc, result] = utils::execute("/bin/systemctl", "is-active",
                                       "nvidia-emmc-logging.service");

    if (rc == 0)
    {
        // If the service is active, set the U-Boot environment variable
        utils::execute("/sbin/fw_setenv", "emmc_secure_erase_partition",
                       "/var/emmc/user-logs");
    }
}
} // namespace completeReset_utils