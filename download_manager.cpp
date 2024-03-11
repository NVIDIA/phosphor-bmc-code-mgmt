#include "config.h"

#include "download_manager.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

#include <unistd.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace phosphor
{
namespace software
{
namespace manager
{

using namespace sdbusplus::error::xyz::openbmc_project::common;
PHOSPHOR_LOG2_USING;
using namespace phosphor::logging;
namespace fs = std::filesystem;

sdbusplus::bus::bus bus = sdbusplus::bus::new_default();

constexpr auto scpArgsFile = "/tmp/scp.args";
constexpr auto scpTransferService = "scp-transfer";
constexpr auto knownHostsFilePath = "/home/root/.ssh/known_hosts";

constexpr auto httpArgsFile = "/tmp/http.args";
constexpr auto httpDownloadService = "http-download.service";

constexpr auto ACTIVE_STATE = "active";
constexpr auto ACTIVATING_STATE = "activating";

void Download::downloadViaTFTP(std::string fileName, std::string serverAddress)
{
    using Argument = xyz::openbmc_project::common::InvalidArgument;

    // Sanitize the fileName string
    if (!fileName.empty())
    {
        fileName.erase(std::remove(fileName.begin(), fileName.end(), '/'),
                       fileName.end());
        fileName = fileName.substr(fileName.find_first_not_of('.'));
    }

    if (fileName.empty())
    {
        error("Filename is empty");
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("FileName"),
                              Argument::ARGUMENT_VALUE(fileName.c_str()));
        return;
    }

    if (serverAddress.empty())
    {
        error("ServerAddress is empty");
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("ServerAddress"),
                              Argument::ARGUMENT_VALUE(serverAddress.c_str()));
        return;
    }

    info("Downloading {PATH} via TFTP: {SERVERADDRESS}", "PATH", fileName,
         "SERVERADDRESS", serverAddress);

    // Check if IMAGE DIR exists
    fs::path imgDirPath(IMG_UPLOAD_DIR);
    if (!fs::is_directory(imgDirPath))
    {
        error("Image Dir {PATH} does not exist", "PATH", imgDirPath);
        elog<InternalFailure>();
        return;
    }

    pid_t pid = fork();

    if (pid == 0)
    {
        pid_t nextPid = fork();
        if (nextPid == 0)
        {
            // child process
            execl("/usr/bin/tftp", "tftp", "-g", "-r", fileName.c_str(),
                  serverAddress.c_str(), "-l",
                  (std::string{IMG_UPLOAD_DIR} + '/' + fileName).c_str(),
                  (char*)0);
            // execl only returns on fail
            error("Error ({ERRNO}) occurred during the TFTP call", "ERRNO",
                  errno);
            elog<InternalFailure>();
        }
        else if (nextPid < 0)
        {
            error("Error ({ERRNO}) occurred during fork", "ERRNO", errno);
            elog<InternalFailure>();
        }
        // do nothing as parent if all is going well
        // when parent exits, child will be reparented under init
        // and then be reaped properly
        exit(0);
    }
    else if (pid < 0)
    {
        error("Error ({ERRNO}) occurred during fork", "ERRNO", errno);
        elog<InternalFailure>();
    }
    else
    {
        int status;
        if (waitpid(pid, &status, 0) < 0)
        {
            error("Error ({ERRNO}) occurred during waitpid", "ERRNO", errno);
        }
        else if (WEXITSTATUS(status) != 0)
        {
            error("Failed ({STATUS}) to launch tftp", "STATUS", status);
        }
    }

    return;
}

bool isDownloadServiceRunning(const std::string& service)
{
    std::variant<std::string> currentState;
    sdbusplus::message::object_path unitTargetPath;

    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "GetUnit");
    method.append(service);

    try
    {
        auto result = bus.call(method);
        result.read(unitTargetPath);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        error("Error in GetUnit call: {ERROR}", "ERROR", e);
        return false;
    }

    method = bus.new_method_call(
        SYSTEMD_BUSNAME,
        static_cast<const std::string&>(unitTargetPath).c_str(),
        SYSTEMD_PROPERTY_IFACE, "Get");

    method.append(SYSTEMD_INTERFACE_UNIT, "ActiveState");

    try
    {
        auto result = bus.call(method);
        result.read(currentState);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        error("Error in ActiveState Get: {ERROR}", "ERROR", e);
        return false;
    }

    const auto& currentStateStr = std::get<std::string>(currentState);
    return currentStateStr == ACTIVE_STATE ||
           currentStateStr == ACTIVATING_STATE;
}

bool isScpTransferServiceRunning()
{
    // Use the 'pgrep' command to check if the scp-transfer service is running
    std::string command = "pgrep " + std::string(scpTransferService);
    int result = std::system(command.c_str());

    // If 'pgrep' returns 0, the service is running
    if (result == 0)
        return true;

    return false;
}

void Download::downloadViaSCP(std::string serverAddress, std::string username,
                              std::string sourceFilePath, std::string target)
{
    using Argument = xyz::openbmc_project::Common::InvalidArgument;
    std::string fileName = sourceFilePath;

    if (isScpTransferServiceRunning())
    {
        error("SCP tranfer is already in progress");
        elog<NotAllowed>(xyz::openbmc_project::Common::NotAllowed::REASON(
            ("SCP tranfer is already in progress")));
        return;
    }

    // Clean the previous status
    updateStatusProperties(sourceFilePath, target, Status::None);

    if (sourceFilePath.empty())
    {
        error("sourceFilePath is empty");
        updateStatusProperties(sourceFilePath, target, Status::Invalid);
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("sourceFilePath"),
                              Argument::ARGUMENT_VALUE(sourceFilePath.c_str()));
        return;
    }

    // Find the last occurrence of the directory separator character
    size_t lastSeparatorPos = sourceFilePath.find_last_of("/");
    if (lastSeparatorPos != std::string::npos)
    {
        // Extract the substring after the last separator position
        fileName = sourceFilePath.substr(lastSeparatorPos + 1);
    }

    if (serverAddress.empty())
    {
        error("ServerAddress is empty");
        updateStatusProperties(fileName, target, Status::Invalid);
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("ServerAddress"),
                              Argument::ARGUMENT_VALUE(serverAddress.c_str()));
        return;
    }

    if (target.empty())
    {
        error("Target is empty");
        updateStatusProperties(fileName, target, Status::Invalid);
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("Target"),
                              Argument::ARGUMENT_VALUE(target.c_str()));
        return;
    }

    if (!fs::exists(target))
    {
        error("Target does not exist");
        updateStatusProperties(fileName, target, Status::Invalid);
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("Target"),
                              Argument::ARGUMENT_VALUE(target.c_str()));
        return;
    }

    if (username.empty())
    {
        error("Username is empty");
        updateStatusProperties(fileName, target, Status::Invalid);
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("username"),
                              Argument::ARGUMENT_VALUE(username.c_str()));
        return;
    }

    // Create an args file used by scp-transfer service
    std::ofstream argfile(scpArgsFile, std::ios::out | std::ios::trunc);
    if (!argfile.is_open())
    {
        error("Failed to open an args file");
        updateStatusProperties(fileName, target, Status::Invalid);
        elog<InternalFailure>();
        return;
    }

    std::string cmdContent = "serverAddress=" + serverAddress + "\n" +
                             "username=" + username + "\n" +
                             "sourceFilePath=" + sourceFilePath + "\n" +
                             "target=" + target + "\n";

    // Write cmdContent the args file
    argfile << cmdContent;
    argfile.close();

    try
    {
        auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                          SYSTEMD_INTERFACE, "StartUnit");
        method.append(scpTransferService + std::string(".service"), "replace");
        bus.call_noreply(method);
    }
    catch (const std::exception& e)
    {
        error("Failed to start scp-transfer service");
        updateStatusProperties(fileName, target, Status::Failed);
        elog<InternalFailure>();
        return;
    }
}

void Download::addRemoteServerPublicKey(const std::string serverAddress,
                                        const std::string publicKeyStr)
{
    // Create the entire directory path if it doesn't exist
    fs::create_directories(fs::path(knownHostsFilePath).parent_path());

    // Open known_hosts file in append mode, creating a new file if it doesn't
    // exist
    std::fstream knownHostsFile(knownHostsFilePath,
                                std::ios::in | std::ios::out | std::ios::app);

    if (!knownHostsFile.is_open())
    {
        error("Failed to update the known_hosts file");
        elog<InternalFailure>();
        return;
    }

    // Check if the combination already exists
    std::string newLine = serverAddress + " " + publicKeyStr;
    std::string line;
    while (std::getline(knownHostsFile, line))
    {
        if (line == newLine)
        {
            error("Combination already exists, no need to add again");
            knownHostsFile.close(); // Close the file
        }
    }

    // Combination doesn't exist, so add it
    knownHostsFile.clear();
    knownHostsFile << newLine << std::endl;
    knownHostsFile.close();
}

void Download::revokeAllRemoteServerPublicKeys(const std::string serverAddress)
{
    // Check if the known_hosts file exists
    std::ifstream file(knownHostsFilePath);
    if (!file.is_open())
    {
        error("Failed to open the known_hosts file");
        elog<InternalFailure>();
        return;
    }

    std::vector<std::string> lines;
    std::string line;

    // Read lines from the known_hosts file and exclude lines containing
    // serverAddress
    while (std::getline(file, line))
    {
        if (line.find(serverAddress) == std::string::npos)
        {
            lines.push_back(line);
        }
    }

    file.close();

    // Reopen the file for writing, remove the existing content and write the
    // updated content
    std::ofstream outfile(knownHostsFilePath,
                          std::ofstream::out | std::ofstream::trunc);
    if (!outfile.is_open())
    {
        error("Failed to reopen the known_hosts file for writing");
        elog<InternalFailure>();
        return;
    }

    for (const std::string& newLine : lines)
    {
        outfile << newLine << std::endl;
    }

    outfile.close();
}

std::string Download::generateSelfKeyPair()
{
    const char* command;
    std::string res, selfPublicKeyStr;
    std::string selfKeyFilePath = "/home/root/.ssh/id_dropbear";

    // Create the entire directory path if it doesn't exist
    fs::create_directories(fs::path(selfKeyFilePath).parent_path());
    if (!fs::exists(selfKeyFilePath))
    {
        // If the key does not exist, create it
        command = "dropbearkey -t ed25519 -f ~/.ssh/id_dropbear";
    }
    else
    {
        // If the key already exists, print it
        command = "dropbearkey -y -f ~/.ssh/id_dropbear";
    }

    FILE* pipe = popen(command, "r");

    if (!pipe)
    {
        error("Failed to open pipe to command");
        return std::string();
    }

    char buffer;
    while (fscanf(pipe, "%c", &buffer) != EOF)
    {
        res += buffer;
    }

    pclose(pipe);

    // Res is: "Public key portion is:\n
    // <type> <key> <username>@<hostname>\n
    // Fingerprint: <Fingerprint>"
    // selfPublicKeyStr contains "<type> <key>"

    size_t startPos = res.find('\n') + 1;
    size_t endTypePos = res.find(" ", startPos);
    size_t endKeyPos = res.find(" ", endTypePos + 1);

    if (startPos != std::string::npos && endKeyPos != std::string::npos)
    {
        selfPublicKeyStr = res.substr(startPos, endKeyPos - startPos);
    }

    return selfPublicKeyStr;
}

void Download::downloadViaHTTP(std::string serverAddress, bool secure,
                               std::string sourceFile, std::string destDir)
{
    using Argument = xyz::openbmc_project::Common::InvalidArgument;
    std::string fileName = sourceFile;

    if (isDownloadServiceRunning(httpDownloadService))
    {
        error("HTTP download is already in progress");
        elog<NotAllowed>(xyz::openbmc_project::Common::NotAllowed::REASON(("HTTP download is already in progress")));
        return;
    }

    // Clean the previous status
    updateDownloadStatusProperties(sourceFile, destDir, DownloadStatus::Init);

    if (sourceFile.empty())
    {
        error("sourceFile is empty");
        updateDownloadStatusProperties(sourceFile, destDir, DownloadStatus::Invalid);
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("sourceFile"),
                              Argument::ARGUMENT_VALUE(sourceFile.c_str()));
        return;
    }

    // Find the last occurrence of the directory separator character
    size_t lastSeparatorPos = sourceFile.find_last_of("/");
    if (lastSeparatorPos != std::string::npos)
    {
        // Extract the substring after the last separator position
        fileName = sourceFile.substr(lastSeparatorPos + 1);
    }

    if (serverAddress.empty())
    {
        error("ServerAddress is empty");
        updateDownloadStatusProperties(sourceFile, destDir, DownloadStatus::Invalid);
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("ServerAddress"),
                              Argument::ARGUMENT_VALUE(serverAddress.c_str()));
        return;
    }

    if (destDir.empty())
    {
        error("DestFile is empty");
        updateDownloadStatusProperties(sourceFile, destDir, DownloadStatus::Invalid);
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("destDir"),
                              Argument::ARGUMENT_VALUE(destDir.c_str()));
        return;
    }

    if (!fs::exists(destDir))
    {
        error("Destination file including path does not exist");
        updateDownloadStatusProperties(sourceFile, destDir, DownloadStatus::Invalid);
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("Target"),
                              Argument::ARGUMENT_VALUE(destDir.c_str()));
        return;
    }

    // Create an args file used by http-download service
    std::ofstream argfile(httpArgsFile, std::ios::out | std::ios::trunc);
    if (!argfile.is_open())
    {
        error("Failed to open an args file for http download");
        updateDownloadStatusProperties(sourceFile, destDir, DownloadStatus::Invalid);
        elog<InternalFailure>();
        return;
    }

    std::string cmdContent = "serverAddress=" + serverAddress + "\n"
                             + "sourceFile=" + sourceFile+ "\n"
                             + "destDir=" + destDir + "\n"
                             + "protocol=" + (secure ? "HTTPS" : "HTTP") + "\n";

    // Write cmdContent the args file
    argfile << cmdContent;
    argfile.close();

    try
    {
        auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                          SYSTEMD_INTERFACE, "StartUnit");
        method.append(httpDownloadService, "replace");
        bus.call_noreply(method);
    }
    catch (const std::exception& e)
    {
        error("Failed to start http-download service");
        updateDownloadStatusProperties(fileName, destDir, DownloadStatus::Failed);
        elog<InternalFailure>();
        return;
    }
}

} // namespace manager
} // namespace software
} // namespace phosphor
