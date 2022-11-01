#include <iostream>
#include <fstream>
#include <cstdio>
#include <string>

#include "config.h"

#include <sdbusplus/test/sdbus_mock.hpp>

#include "inventory_manager.hpp"
#include "version_inv_entry.hpp"

#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::StrEq;
using ::testing::StartsWith;

/* verifies that inventory manager starts up, adds a version object and populates
   the basic fields in the object as well as all 4 expected associations are
   generated (active, activation, functional, and updateable) */
TEST(InventoryTest, InventoryManager) {
    sdbusplus::SdBusMock sdbus_mock;
    auto bus_mock = sdbusplus::get_mocked_new(&sdbus_mock);

EXPECT_CALL(sdbus_mock,
        sd_bus_emit_properties_changed_strv(IsNull(), StartsWith("/xyz/openbmc_project/software/"),
                                            StrEq("xyz.openbmc_project.Association.Definitions"), NotNull()))
    .Times(3).WillRepeatedly(Invoke(
        [=](sd_bus*, const char*, const char*, const char** names) {
            EXPECT_STREQ("Associations", names[0]);
            return 0;
        }));

EXPECT_CALL(sdbus_mock,
        sd_bus_emit_properties_changed_strv(IsNull(), StartsWith("/xyz/openbmc_project/software/"),
                                            StrEq("xyz.openbmc_project.Software.Version"), NotNull()))
    .Times(2).WillRepeatedly(Invoke(
        [=](sd_bus*, const char*, const char*, const char** names) {
            std::vector<std::string> expectedNames = {"Purpose", "Version"};
            if (std::none_of(expectedNames.begin(), expectedNames.end(),
                        [names](const std::string s) {return s == names[0];})) {
                    ADD_FAILURE();
                }
            return 0;
        }));
    phosphor::software::manager::InventoryManager manager(bus_mock);
}
