#pragma once

#include "i2c.hpp"
#include "i2c_interface.hpp"

#include <openssl/evp.h>
#include <openssl/pem.h>

#include <experimental/filesystem>
#include <functional>
#include <typeinfo>

namespace phosphor
{
namespace software
{
namespace updater
{

static const std::string romExtension{".rom"};

static const std::string binExtension{".bin"};

static const std::string cecAttestFolder{"/tmp/cec_attest/"};

static const std::string cecAttestStatusFile{"attest_status.txt"};

static const std::string cecAttestPayloadFile{"sign_response.bin"};

static const std::string cecAttestDataFile{"sign_data.bin"};

static const std::string cecAttestSignatureFile{"signature.bin"};

static const std::string cecAttestPublicKeyFile{"public_key.pem"};

static const std::string cecAttestRandomFile{"random_num.bin"};

namespace fsys = std::experimental::filesystem;

// RAII support for openSSL functions.
using BIO_MEM_Ptr = std::unique_ptr<BIO, decltype(&::BIO_free)>;
using EVP_PKEY_Ptr = std::unique_ptr<EVP_PKEY, decltype(&::EVP_PKEY_free)>;

using EVP_MD_CTX_Ptr =
    std::unique_ptr<EVP_MD_CTX, decltype(&::EVP_MD_CTX_free)>;

struct RemovablePath
{
    fsys::path path;

    RemovablePath(const fsys::path& path) : path(path) {}
    ~RemovablePath()
    {
        if (!path.empty())
        {
            std::error_code ec;
            fsys::remove_all(path, ec);
        }
    }
};

class I2CCommLib
{
  public:
    I2CCommLib(uint8_t busIdentifier, uint8_t deviceAddrress)
    {
        busId = busIdentifier;
        deviceAddr = deviceAddrress;
    }

    virtual void SendBootComplete();

    virtual uint8_t GetCECState();

    virtual void SendStartFWUpdate(uint32_t imgFileSize,
                                   uint8_t fwType = BMC_FW_ID);

    virtual uint8_t GetLastCmdStatus();

    virtual void SendImageToCEC(std::string& fileName, uint32_t imageSize);

    virtual void SendCopyImageComplete();

    virtual uint8_t QueryAboutInterrupt();

    virtual uint8_t GetFWUpdateStatus();

    /** @struct ReadCecVersion
     *  @brief CEC version details.
     */
    struct ReadCecVersion
    {
        uint8_t checkSum;
        uint8_t major;
        uint8_t minor;
    } __attribute__((packed));

    virtual void GetCecVersion(ReadCecVersion& version);

    virtual void SendBMCReset();

    virtual void GetAttestation(uint16_t dataSize = ATTESTATION_PAYLOAD_SIZE,
                                uint16_t blkSize = BLOCK_SIZE_128_BYTE);

  private:
    void VerifyCheckSum(const std::vector<uint8_t>& data);

    void UpdateCheckSum(std::vector<uint8_t>& data);

    int CreateRandomList(std::vector<uint8_t>& randomData);

    bool VerifySignature(const fsys::path& dataFile, const fsys::path& sigFile,
                         const fsys::path& publicKey);

    void CreateDERSignature(const std::vector<uint8_t>& data, uint16_t dataSize,
                            std::string filename);

    int HexStringToRandomList(const std::string& str,
                              std::vector<uint8_t>& randomData);

    void WriteStatusFile(std::string& status);

  public:
    virtual ~I2CCommLib() = default;

  public:
    enum class CommandStatus
    {
        SUCCESS = 0,
        ERR_I2C_CHECKSUM,
        ERR_CMD_LENGTH_MISMATCH,
        ERR_CMD_VERSION_SUPPORTED,
        ERR_BUSY,
        ERR_FLASH_ERROR,
        ERR_CMD_INVALID,
        ERR_CMD_INTERNAL,
        ERR_PRIMARY_REGION_DEGRADED,
        ERR_SECONDARY_REGION_DEGRADED,
        ERR_RECOVERY_REGION_DEGRADED,
        ERR_PRIMARY_SECONDARY_MISMATCH,
        UNKNOWN
    };

    enum class FirmwareUpdateStatus
    {
        STATUS_UPDATE_FINISH = 0x0a,
        STATUS_UPDATE_IN_PROGRESS,
        STATUS_ERR_FIRMWARE_HEADER,
        STATUS_ERR_FIRMWARE_ID_MISMATCH,
        STATUS_UPDATE_INIT = 0x16,
        STATUS_CODE_OTHER
    };

    enum class CECInterruptStatus
    {
        BMC_FW_UPDATE_FAIL = 0x01,
        BMC_FW_UPDATE_REQUEST_RESET_NOW,
        BMC_FW_UPDATE_REQUEST_RESET_LATER,
        UNKNOWN
    };

    std::string GetCommandStatusStr(uint8_t status);
    static constexpr uint8_t CEC_FW_ID{0x01};
    static constexpr uint8_t BMC_FW_ID{0x04};

    static constexpr uint32_t OTA_HEADER_SIZE{0x130};
    static constexpr uint32_t OTA_HEADER_OFFSET_1MB_FILE_SIZE{0xFF000};
    static constexpr uint32_t OTA_HEADER_OFFSET_2MB_FILE_SIZE{0x1FF000};
    static constexpr uint32_t MB_SIZE{0x100000};

    static constexpr uint8_t OTA_OFFSET_SIZE1{0xE8};
    static constexpr uint8_t OTA_OFFSET_SIZE2{0xE9};
    static constexpr uint8_t OTA_OFFSET_SIZE3{0xEA};
    static constexpr uint8_t OTA_OFFSET_SIZE4{0xEB};

    static constexpr uint16_t BLOCK_SIZE_128_BYTE{128};
    static constexpr uint16_t BLOCK_SIZE_64_BYTE{64};
    static constexpr uint16_t BLOCK_SIZE_48_BYTE{48};
    static constexpr uint16_t BLOCK_SIZE_32_BYTE{32};

    static constexpr uint16_t ATTESTATION_PAYLOAD_SIZE{657};

  private:
    std::string deviceName;

    uint8_t busId;

    uint8_t deviceAddr;

  private:
    static constexpr uint8_t READ_CKSUM_LOCATION{0};
    static constexpr uint8_t WRITE_CKSUM_LOCATION{2};
    static constexpr uint8_t RD_CMD_STATUS_REG{0x04};
    static constexpr uint8_t RD_QUERY_INTERRUPT_REG{0x08};
    static constexpr uint8_t RD_FW_UPDATE_REG{0x05};
    static constexpr uint8_t WR_DEVICE_REG{0x03};
    static constexpr uint8_t CHALLENGE_RESPONSE_REG{0x06};
    static constexpr uint8_t FIRMWARE_VERSION_REG{0x01};

    static constexpr uint8_t EMPTY{0x00};
    static constexpr uint8_t CEC_VERSION_MAJOR{0x01};
    static constexpr uint8_t CEC_VERSION_MINOR{0x00};
    static constexpr uint8_t FW_CLASS{0x00};
    static constexpr uint8_t START_FW_UPDATE_CMD{0x00};
    static constexpr uint8_t COPY_IMG_COMPLETE_CMD{0x01};
    static constexpr uint8_t ATTESTATION_CMD{0x02};
    static constexpr uint8_t BOOT_COMPLETE_CMD{0x03};
    static constexpr uint8_t BMC_RESET_CMD{0x05};

    static constexpr uint8_t START_FW_CMD_LEN{0x06};
    static constexpr uint8_t COPY_IMG_CMD_LEN{0x01};
    static constexpr uint8_t BOOT_COMPLETE_CMD_LEN{0x03};

    static constexpr uint8_t BLOCK_SIZE_128_VALUE{0x0};
    static constexpr uint8_t BLOCK_SIZE_64_VALUE{0x1};
    static constexpr uint8_t BLOCK_SIZE_48_VALUE{0x2};
    static constexpr uint8_t BLOCK_SIZE_32_VALUE{0x3};

    static constexpr uint8_t DEFAULT_RANDON_NUMBERS{32};
    static constexpr uint8_t SIGNATURE_SIZE{96};
    static constexpr int SUCCESS{0};
    static constexpr int FAILURE{-1};
    std::vector<std::string> commandStatusStr{"SUCCESS",
                                              "ERR_I2C_CHECKSUM",
                                              "ERR_CMD_LENGTH_MISMATCH",
                                              "ERR_CMD_VERSION_SUPPORTED",
                                              "ERR_BUSY",
                                              "ERR_FLASH_ERROR",
                                              "ERR_CMD_INVALID",
                                              "ERR_CMD_INTERNAL",
                                              "ERR_PRIMARY_REGION_DEGRADED",
                                              "ERR_SECONDARY_REGION_DEGRADED",
                                              "ERR_RECOVERY_REGION_DEGRADED",
                                              "ERR_PRIMARY_SECONDARY_MISMATCH",
                                              "UNKNOWN"};
};

} // namespace updater
} // namespace software
} // namespace phosphor
