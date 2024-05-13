#include "config.h"

#include "flash.hpp"

#include "activation.hpp"
#include "item_updater.hpp"
#include "pris_state_machine.hpp"
#include "serialize.hpp"
#include "state_machine.hpp"
#include "version.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/exception.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <experimental/filesystem>
#include <filesystem>
#include <regex>

namespace
{
constexpr auto PATH_INITRAMFS = "/run/initramfs";
} // namespace

namespace phosphor
{
namespace software
{
namespace updater
{

using namespace phosphor::logging;
namespace fs = std::filesystem;
namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;
class ItemUpdater;

std::unique_ptr<MachineContext> sUpdateMachineContext = nullptr;
std::unique_ptr<StateMachine> fwUpdateMachine = nullptr;

inline void logAndThrowError(std::string err_str, std::string additional_info)
{
    err_str += additional_info;
    log<level::ERR>(err_str.c_str());
    throw std::runtime_error(err_str);
}

void tokenize(const std::string& strIn, std::vector<std::string>& out)
{
    const std::regex oldFormatReg("(\\d*)[.](\\d*)[.](\\d*)");
    const std::regex newFormatReg("[a-zA-Z0-9]+[-](\\d{2})[.](\\d{2})[-](.*)");
    std::smatch found;

    // If the old format of x.x.x-N
    if (std::regex_search(strIn, found, oldFormatReg))
    {
        for (unsigned i = 1; i < found.size(); ++i)
        {
            out.push_back(found.str(i));
        }
    }
    // new formar ID-yy.mm-N-rcN or ID-yy.mm-N_br
    else if (std::regex_search(strIn, found, newFormatReg))
    {
        for (unsigned i = 1; i < found.size(); ++i)
        {
            out.push_back(found.str(i));
        }
    }
}

// This function blocks the update of BMC image with version lower
// than 2.8.2-XYZ.
//  This is because that from version 2.8.1 we have new architecture,
//  downgrading will brick the device
// Example: In 2.8.2-XYZ , MajorVersion - 2 , MinorVersion - 8 and Version - 2
//  SubVersion - XYZ
// Versions lower than 2.8.2-XYZ like 2.8.1-XYZ , 2.8.0-XYZ etc will be blocked
// from getting upgraded.
//  This function support the new format ID-yy.mm-N-rcN or ID-yy.mm-N-br
//  Example: BF3BMC-22.10-1, MajorVersion - 22 , MinorVersion - 10 and Version -
//  1
bool stopDowngrade(fs::path manifestPath)
{
    try
    {
        if (!fs::is_regular_file(manifestPath))
        {
            logAndThrowError("Error No manifest file, filename: ",
                             manifestPath);
        }

        // Get version
        auto version = phosphor::software::manager::Version::getValue(
            manifestPath.string(), "version");
        if (version.empty())
        {
            logAndThrowError("Error unable to read version from manifest file",
                             version);
        }

        std::vector<std::string> versionVector;
        tokenize(version, versionVector);

        if (versionVector.size() != 3)
        {
            logAndThrowError(
                "Error incorrect version format from manifest file, version: ",
                version);
        }

        int majorVersionInt = std::stoi(versionVector[0]);
        int minorVersionInt = std::stoi(versionVector[1]);

        std::string lastVer = versionVector[2];
        // In case there are no digits at the beginning of lastVer, versionInt
        // is 0
        int versionInt = std::atoi(lastVer.c_str());

        bool cecStatus = utils::checkCECExist();
        if (cecStatus == true)
        {
            if (majorVersionInt < 2 ||
                (majorVersionInt == 2 && minorVersionInt < 8) ||
                (majorVersionInt == 2 && minorVersionInt == 8 &&
                 versionInt < 2))
            {
                std::string err_str{
                    "Error cannot downgrade to lower version: "};
                err_str += version;
                log<level::ERR>(err_str.c_str());

                return true;
            }
        }
        else
        {
            if (majorVersionInt < 2 ||
                (majorVersionInt == 2 && minorVersionInt < 8) ||
                (majorVersionInt == 2 && minorVersionInt == 8 &&
                 versionInt < 4))
            {
                std::string err_str{
                    "Error cannot downgrade to lower version: "};
                err_str += version;
                log<level::ERR>(err_str.c_str());

                return true;
            }
        }
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("Error in checking the version from MANIFEST file",
                        entry("EXCEPTION=%s", e.what()));
        return true;
    }

    return false;
}

void Activation::flashWrite()
{
    bool cecStatus = utils::checkCECExist();
    if (cecStatus == true)
    {
        try
        {
            fs::path uploadDir(IMG_UPLOAD_DIR);

            fs::path manifestPath = uploadDir / versionId / MANIFEST_FILE_NAME;
            if (stopDowngrade(manifestPath))
            {
                throw std::runtime_error(
                    "Error in stop Downgrade - cec status true");
            }

            secureFlashSuceeded = true;
            secureUpdateProgress = SecureUpdate::INPROGRESS;

            if (sUpdateMachineContext)
            {
                sUpdateMachineContext.reset(nullptr);
            }
            if (fwUpdateMachine)
            {
                fwUpdateMachine.reset(nullptr);
            }

            sUpdateMachineContext = std::make_unique<MachineContext>(this);

            fs::path bmcImageName(uploadDir / versionId / fwSecureBmcImageName);

            uint32_t fSize = static_cast<uint32_t>(fs::file_size(bmcImageName));

            sUpdateMachineContext->SetData(keyBmcImgSize, fSize);

            sUpdateMachineContext->SetData(keyBmcImgName,
                                           bmcImageName.string());

            fwUpdateMachine = std::make_unique<PrisStateMachine>(
                *(sUpdateMachineContext.get()));

            fwUpdateMachine->TriggerFWUpdate();

            std::tuple<bool, std::string> ret =
                sUpdateMachineContext->GetMachineRunStatus();
            bool smRunSuceeded = std::get<0>(ret);
            std::string msg{" "};

            if (smRunSuceeded)
            {
                // FW update triggered successfully.
                return;
            }

            msg += std::get<1>(ret);
            log<level::ERR>("SECURE UPDATE FAILED IN A STATE ",
                            entry("FAILURE=%s", msg.c_str()));
        }
        catch (const std::exception& e)
        {
            log<level::ERR>("SECURE UPDATE RUN EXCEPTION ",
                            entry("EXCEPTION=%s", e.what()));
        }
        failActivation();
    }
    else
    {
        fs::path uploadDir(IMG_UPLOAD_DIR);
        fs::path toPath(PATH_INITRAMFS);
        unsecureFlashSuceeded = false;
        try
        {
            fs::path manifestPath = uploadDir / versionId / MANIFEST_FILE_NAME;
            if (stopDowngrade(manifestPath))
            {
                throw std::runtime_error("Error in stop Downgrade");
            }
            for (auto& bmcImage : phosphor::software::image::bmcImages)
            {
                if (fs::exists(uploadDir / versionId / bmcImage))
                {
                    fs::copy_file(uploadDir / versionId / bmcImage,
                                  toPath / bmcImage,
                                  fs::copy_options::overwrite_existing);
                }
            }
            unsecureFlashSuceeded = true;
        }
        catch (const std::exception& e)
        {
            log<level::ERR>("NON-CEC BMC UPDATE RUN EXCEPTION ",
                            entry("EXCEPTION=%s", e.what()));
        }
    }
}

void Activation::onStateChanges(sdbusplus::message::message& msg)
{
    // Empty
    uint32_t newStateID{};
    sdbusplus::message::object_path newStateObjPath;
    std::string newStateUnit{};
    std::string newStateResult{};

    try
    {
        // Read the msg and populate each variable
        msg.read(newStateID, newStateObjPath, newStateUnit, newStateResult);

        auto copyImageServiceFile = "obmc-secure-copy-image@" + versionId +
                                    ".service";

        if (newStateUnit != copyImageServiceFile)
        {
            return;
        }

        secureUpdateTimer->cancel();
        Activation::unsubscribeFromSystemdSignals();

        if (newStateResult == "done")
        {
            if (fwUpdateMachine &&
                fwUpdateMachine->GetCurrentState() ==
                    (static_cast<uint8_t>(
                        PrisStateMachine::States::STATE_COPY_IMAGE)))
            {
                fwUpdateMachine->TriggerState((static_cast<uint8_t>(
                    PrisStateMachine::States::STATE_SEND_COPY_COMPLETE)));

                std::tuple<bool, std::string> ret =
                    sUpdateMachineContext->GetMachineRunStatus();
                bool smRunSuceeded = std::get<0>(ret);
                std::string msg{" "};

                if (!smRunSuceeded)
                {
                    msg += std::get<1>(ret);
                    log<level::ERR>("onStateChanges: STATE Failed",
                                    entry("FAILURE=%s", msg.c_str()));
                }
                else
                {
                    // Image copy succeeded
                    return;
                }
            }
        }
        else
        {
            if (newStateResult == "failed" || newStateResult == "dependency")
            {
                log<level::ERR>(
                    "onStateChanges Secure Image Copy script: failed run.",
                    entry("RESULT=%s", newStateResult.c_str()));
            }
        }
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("onStateChanges Exception",
                        entry("EXCEPTION=%s", e.what()));
    }
    failActivation();

    return;
}

void Activation::failActivation(bool failed)
{
    secureUpdateProgress = Activation::SecureUpdate::IDLE;
    try
    {
        activationBlocksTransition.reset(nullptr);
        if (failed)
        {
            secureFlashSuceeded = false;

            softwareServer::Activation::activation(
                softwareServer::Activation::Activations::Failed);

            // Remove version object from image manager
            Activation::deleteImageManagerObject();
        }
        else
        {
            activationProgress->progress(100);

            softwareServer::Activation::activation(
                softwareServer::Activation::Activations::Active);

            storePurpose(versionId,
                         parent.versions.find(versionId)->second->purpose());

            if (!redundancyPriority)
            {
                redundancyPriority =
                    std::make_unique<RedundancyPriority>(bus, path, *this, 0);
            }

            // Remove version object from image manager
            Activation::deleteImageManagerObject();

            // Create active association
            parent.createActiveAssociation(path);
        }
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("failActivation Exception",
                        entry("EXCEPTION=%s", e.what()));
    }
    if (sUpdateMachineContext)
    {
        sUpdateMachineContext->ClearData();
        sUpdateMachineContext.reset(nullptr);
    }
    if (fwUpdateMachine)
    {
        fwUpdateMachine.reset(nullptr);
    }
}

} // namespace updater
} // namespace software
} // namespace phosphor
