#pragma once

#include "xyz/openbmc_project/Common/DownloadProgress/server.hpp"
#include "xyz/openbmc_project/Common/HTTP/server.hpp"
#include "xyz/openbmc_project/Common/SCP/server.hpp"
#include "xyz/openbmc_project/Common/TFTP/server.hpp"

#include <sdbusplus/bus.hpp>

#include <string>

namespace phosphor
{
namespace software
{
namespace manager
{

using DownloadInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Common::server::TFTP,
    sdbusplus::xyz::openbmc_project::Common::server::SCP,
    sdbusplus::xyz::openbmc_project::Common::server::HTTP,
    sdbusplus::xyz::openbmc_project::Common::server::DownloadProgress>;

/** @class Download
 *  @brief OpenBMC download software management implementation.
 *  @details A concrete implementation for xyz.openbmc_project.Common.TFTP
 *  DBus API.
 */
class Download : public DownloadInherit
{
  public:
    /** @brief Constructs Download Software Manager
     *
     * @param[in] bus       - The Dbus bus object
     * @param[in] objPath   - The Dbus object path
     */
    Download(sdbusplus::bus_t& bus, const std::string& objPath) :
        DownloadInherit(bus, (objPath).c_str()){};

    /**
     * @brief Download the specified image via TFTP
     *
     * @param[in] fileName      - The name of the file to transfer.
     * @param[in] serverAddress - The TFTP Server IP Address.
     **/
    void downloadViaTFTP(std::string fileName,
                         std::string serverAddress) override;

    /**
     * @brief Implements the DownloadViaSCP dbus method functionality.
     * User's password in not needed as a key-based authentication is used.
     *
     * @param[in] serverAddress - The SCP Server IP Address.
     * @param[in] username - The username to authenticate the file transfer.
     * @param[in] sourceFilePath - The file path on the remote server.
     * @param[in] target - The target directory (local path) to apply the image.
     **/
    void downloadViaSCP(std::string serverAddress, std::string username,
                        std::string sourceFilePath,
                        std::string target) override;

    /**
     * @brief Add remote server public key to SSH known_host file
     *
     * @param[in] serverAddress  - The server IP address.
     * @param[in] publicKeyStr   - The server's public key string ("<type>
     *<key>").
     **/
    void addRemoteServerPublicKey(const std::string serverAddress,
                                  const std::string publicKeyStr) override;

    /**
     * @brief Removes all the public keys of a remote server from SSH known_host
     *file
     *
     * @param[in] serverAddress  - The server IP address.
     **/
    void revokeAllRemoteServerPublicKeys(
        const std::string serverAddress) override;

    /**
     * @brief Generates self key pair by using dropbearkey
     * "dropbearkey -t ed25519 -f ~/.ssh/id_dropbear". In case the key pair
     *already exists it returns the existing public key.
     *
     * @returns The generated public key string ("<type> <key>"), an empty
     *string on failure.
     **/
    std::string generateSelfKeyPair() override;

    /**
     * @brief Implements the DownloadViaHTTP D-Bus method functionality.
     *
     * @param[in] serverAddress - The HTTP/HTTPS Server IP Address.
     * @param[in] secure - The option to use secure HTTPS or not.
     * @param[in] sourceFile - The source file on the remote server.
     * @param[in] destDir - The destination directory (local path) to apply the
     *image
     **/
    void downloadViaHTTP(std::string serverAddress, bool secure,
                         std::string sourceFile, std::string destDir) override;

  private:
    /**
     * @brief Updates download status properties
     *
     * @param[in] sourceFile   - The name of the file to download.
     * @param[in] destDir      - The destination directory to apply the image.
     * @param[in] status       - The current status of the download.
     **/
    inline void updateDownloadStatusProperties(std::string& sourceFile,
                                               std::string& destDir,
                                               DownloadStatus status)
    {
        DownloadInherit::sourceFile(sourceFile);
        DownloadInherit::destDir(destDir);
        DownloadInherit::status(status);
    }
};

} // namespace manager
} // namespace software
} // namespace phosphor
