#include "config.h"

#include "i2c_comm_lib.hpp"

#include <CLI/CLI.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <filesystem>
#include <fstream>

using namespace phosphor::logging;

using namespace phosphor::software::updater;

using sdbusplus::exception::SdBusError;

namespace match_rules = sdbusplus::bus::match::rules;

namespace phosphor
{
namespace NvidiaFWService
{

namespace fs = std::filesystem;

static constexpr auto busIdentifier = CEC_BUS_IDENTIFIER;

static constexpr auto deviceAddrress = CEC_DEVICE_ADDRESS;

struct RemovablePath
{
    fs::path path;

    RemovablePath(const fs::path& path) : path(path) {}
    ~RemovablePath()
    {
        if (!path.empty())
        {
            std::error_code ec;
            fs::remove_all(path, ec);
        }
    }
};

void CleanUpFiles()
{
    if (!fs::exists(cecAttestFolder))
    {
        fs::create_directory(cecAttestFolder);
    }
    else
    {
        fs::path statusFile(cecAttestFolder + cecAttestStatusFile);
        fs::path responseFile(cecAttestFolder + cecAttestPayloadFile);
        fs::path publicKeyFile(cecAttestFolder + cecAttestPublicKeyFile);
        fs::path randomFile(cecAttestFolder + cecAttestRandomFile);

        RemovablePath attestStatusFile(statusFile);
        RemovablePath attestPayloadFile(responseFile);
        RemovablePath attestPublicKeyFile(publicKeyFile);
        RemovablePath attestRandomFile(randomFile);
    }
}

} // namespace NvidiaFWService
} // namespace phosphor

int main(int argc, char** argv)
{
    I2CCommLib deviceLayer(phosphor::NvidiaFWService::busIdentifier,
                           phosphor::NvidiaFWService::deviceAddrress);
    std::string fileName = {};
    uint32_t dataSize{I2CCommLib::ATTESTATION_PAYLOAD_SIZE};
    uint16_t blockSize{I2CCommLib::BLOCK_SIZE_128_BYTE};
    std::string publicKeyFilename;
    std::string randomNumberStr;

    CLI::App app{"Nvidia Frimware Service"};

    // Add an input option
    app.add_option("-d", dataSize, "Attestation data size.");
    app.add_option("-b", blockSize, "Block size at which data has to be read.");
    app.add_option("-f", publicKeyFilename,
                   "Public key filename to be used for signature validation.");
    app.add_option(
        "-n", randomNumberStr,
        "32 byte random number in hex format to be used as challenge.");

    // Parse input parameter
    try
    {
        app.parse(argc, argv);
    }
    catch (CLI::Error& e)
    {
        log<level::ERR>("nvidia firmware service EXECPTION",
                        entry("EXCEPTION=%s", e.what()));
        return -1;
    }

    try
    {
        phosphor::NvidiaFWService::CleanUpFiles();

        fsys::path publicKeyFile(publicKeyFilename);
        fsys::path pKeyFileDestination(cecAttestFolder +
                                       cecAttestPublicKeyFile);

        if (fsys::exists(publicKeyFile.string()) &&
            is_regular_file(publicKeyFile))
        {
            fsys::copy(publicKeyFile, pKeyFileDestination);
        }

        if (randomNumberStr.size())
        {
            fsys::path randFile(cecAttestFolder + cecAttestRandomFile);
            std::ofstream randomNumberFile(randFile.string(),
                                           std::ios::binary | std::ios::out);
            randomNumberFile.write(reinterpret_cast<char*>(&randomNumberStr[0]),
                                   randomNumberStr.size());
            randomNumberFile.close();
        }
        deviceLayer.GetAttestation(dataSize, blockSize);
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("nvidia firmware service - GetAttestation failed.",
                        entry("EXCEPTION=%s", e.what()));

        return -1;
    }

    return 0;
}
