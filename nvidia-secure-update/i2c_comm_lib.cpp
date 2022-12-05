#include "i2c_comm_lib.hpp"

#include <fstream>
#include <numeric>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/exception.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <chrono>

#include <cstdlib>
#include <ctime>
#include "../openssl_alloc.hpp"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/sha.h>

namespace phosphor
{
namespace software
{
namespace updater
{

using namespace phosphor::logging;
using namespace std::chrono;
using EVP_PKEY_Ptr = std::unique_ptr<EVP_PKEY, decltype(&::EVP_PKEY_free)>;

using EVP_MD_CTX_Ptr =
      std::unique_ptr<EVP_MD_CTX, decltype(&::EVP_MD_CTX_free)>;



void I2CCommLib::VerifyCheckSum(const std::vector<uint8_t>& data)
{
    uint8_t checksum = 0;
    std::string dataStr{" "};

    checksum = (std::accumulate(data.begin() + READ_CKSUM_LOCATION + 1,
                               data.end(), 0) & 0xff);

    if (data[READ_CKSUM_LOCATION] != checksum)
    {
        std::string msg = "VerifyChecksum failed, Expected: ";
        msg += std::to_string(checksum) + std::string("Received: ") +
               std::to_string(data[READ_CKSUM_LOCATION]);
        throw std::runtime_error(msg.c_str());
    }
}

void I2CCommLib::UpdateCheckSum(std::vector<uint8_t>& data)
{
    uint8_t checksum = 0;
    std::string dataStr{" "};

    checksum = (std::accumulate(data.begin() + WRITE_CKSUM_LOCATION + 1,
                               data.end(), 0) & 0xff);
    data[WRITE_CKSUM_LOCATION] = checksum;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// READ Calls
uint8_t I2CCommLib::GetCECState()
{
    uint8_t deviceOffset = RD_CMD_STATUS_REG;
    uint8_t retVal = static_cast<uint8_t>(CommandStatus::UNKNOWN);

    struct ReadData
    {
        uint8_t checkSum;
        uint8_t lastCmdSent;
        uint8_t statusBit1;
        uint8_t statusBit2;
    } __attribute__((packed));

    ReadData readStruct{};
    std::vector<uint8_t> buf(sizeof(readStruct), 0);

    memcpy(&buf[0], &readStruct, sizeof(readStruct));

    try
    {
        std::unique_ptr<I2CInterface> myDevice =
            create(busId, deviceAddr, true);
        uint8_t size = buf.size();
        myDevice->readCustom(deviceOffset, size, &buf[0]);

        VerifyCheckSum(buf);

        memcpy(&readStruct, &buf[0], sizeof(readStruct));
        retVal = readStruct.statusBit2;

    }
    catch (const std::exception& e)
    {
        std::string msg = "GetCECState: ";
        msg += e.what();
        log<level::ERR>("I2CCommLib - GetCECState command failed.",
                        entry("EXCEPTION=%s", msg.c_str()));
        throw std::runtime_error(msg.c_str());
    }
    return retVal;
}

uint8_t I2CCommLib::GetLastCmdStatus()
{
    uint8_t deviceOffset = RD_CMD_STATUS_REG;
    uint8_t retVal = static_cast<uint8_t>(CommandStatus::UNKNOWN);

    struct ReadData
    {
        uint8_t checkSum;
        uint8_t lastCmdSent;
        uint8_t statusBit1;
        uint8_t statusBit2;
    } __attribute__((packed));

    ReadData readStruct{};
    std::vector<uint8_t> buf(sizeof(readStruct), 0);

    memcpy(&buf[0], &readStruct, sizeof(readStruct));

    try
    {
        std::unique_ptr<I2CInterface> myDevice =
            create(busId, deviceAddr, true);

        uint8_t size = buf.size();
        myDevice->readCustom(deviceOffset, size, &buf[0]);

        VerifyCheckSum(buf);

        memcpy(&readStruct, &buf[0], sizeof(readStruct));
        retVal = readStruct.statusBit2;

    }
    catch (const std::exception& e)
    {
        std::string msg = "GetLastCmdStatus: ";
        msg += e.what();
        log<level::ERR>("I2CCommLib - GetLastCmdStatus command failed.",
                        entry("EXCEPTION=%s", msg.c_str()));
        throw std::runtime_error(msg.c_str());
    }

    return retVal;
}

uint8_t I2CCommLib::QueryAboutInterrupt()
{
    uint8_t deviceOffset = RD_QUERY_INTERRUPT_REG;
    uint8_t retVal = static_cast<uint8_t>(CECInterruptStatus::UNKNOWN);

    struct ReadData
    {
        uint8_t checkSum;
        uint8_t statusBit;
    } __attribute__((packed));

    ReadData readStruct{};
    std::vector<uint8_t> buf(sizeof(readStruct), 0);

    memcpy(&buf[0], &readStruct, sizeof(readStruct));

    try
    {
        std::unique_ptr<I2CInterface> myDevice =
            create(busId, deviceAddr, true);

        uint8_t size = buf.size();
        myDevice->readCustom(deviceOffset, size, &buf[0]);

        VerifyCheckSum(buf);

        memcpy(&readStruct, &buf[0], sizeof(readStruct));
        retVal = readStruct.statusBit;

    }
    catch (const std::exception& e)
    {
        std::string msg = "QueryAboutInterrupt: ";
        msg += e.what();
        log<level::ERR>("I2CCommLib - QueryAboutInterrupt command failed.",
                        entry("EXCEPTION=%s", msg.c_str()));
        throw std::runtime_error(msg.c_str());
    }
    return retVal;
}

uint8_t I2CCommLib::GetFWUpdateStatus()
{
    uint8_t deviceOffset = RD_FW_UPDATE_REG;
    uint8_t retVal =
        static_cast<uint8_t>(FirmwareUpdateStatus::STATUS_CODE_OTHER);

    struct ReadData
    {
        uint8_t checkSum;
        uint8_t progress;
        uint8_t statusBit;
    } __attribute__((packed));

    ReadData readStruct{};
    std::vector<uint8_t> buf(sizeof(readStruct), 0);

    memcpy(&buf[0], &readStruct, sizeof(readStruct));

    try
    {
        std::unique_ptr<I2CInterface> myDevice =
            create(busId, deviceAddr, true);

        uint8_t size = buf.size();

        myDevice->readCustom(deviceOffset, size, &buf[0]);

        VerifyCheckSum(buf);
        memcpy(&readStruct, &buf[0], sizeof(readStruct));
        retVal = readStruct.statusBit;

    }
    catch (const std::exception& e)
    {
        std::string msg = "GetFWUpdateStatus: ";
        msg += e.what();
        log<level::ERR>("I2CCommLib - GetFWUpdateStatus command failed.",
                        entry("EXCEPTION=%s", msg.c_str()));
        throw std::runtime_error(msg.c_str());
    }
    return retVal;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// WRITE Calls

void I2CCommLib::SendBootComplete()
{
    uint8_t deviceOffset = WR_DEVICE_REG;
    uint8_t reg_msb = (deviceOffset & 0xff00) >> 8;
    uint8_t reg_lsb = (deviceOffset & 0xff);

    struct BootCmpltCommand
    {
        uint8_t regMsb;
        uint8_t regLsb;
        uint8_t checkSum;
        uint8_t versionMajor;
        uint8_t versionMinor;
        uint8_t command;
        uint8_t reserved;
        uint8_t length4;
        uint8_t length3;
        uint8_t length2;
        uint8_t length1;
        uint8_t fwClass;
        uint8_t fwID;
        uint8_t fwImage;
    } __attribute__((packed));

    BootCmpltCommand bootCmplt;
    std::vector<uint8_t> buf(sizeof(bootCmplt), 0);

    bootCmplt.regMsb = reg_msb;
    bootCmplt.regLsb = reg_lsb;
    bootCmplt.checkSum = EMPTY;
    bootCmplt.versionMajor = CEC_VERSION_MAJOR;
    bootCmplt.versionMinor = CEC_VERSION_MINOR;
    bootCmplt.command = BOOT_COMPLETE_CMD;
    bootCmplt.reserved = EMPTY;
    bootCmplt.length4 = EMPTY;
    bootCmplt.length3 = EMPTY;
    bootCmplt.length2 = EMPTY;
    bootCmplt.length1 = BOOT_COMPLETE_CMD_LEN;
    bootCmplt.fwClass = FW_CLASS;
    bootCmplt.fwID = BMC_FW_ID;
    bootCmplt.fwImage = EMPTY;

    memcpy(&buf[0], &bootCmplt, sizeof(bootCmplt));

    try
    {
        UpdateCheckSum(buf);

        std::unique_ptr<I2CInterface> myDevice =
            create(busId, deviceAddr, true);

        uint8_t size = buf.size();

        myDevice->writeCustom(deviceOffset, size, &buf[0]);

    }
    catch (const std::exception& e)
    {
        std::string msg = "SendBootComplete: ";
        msg += e.what();
        log<level::ERR>("I2CCommLib - SendBootComplete command failed.",
                        entry("EXCEPTION=%s", msg.c_str()));
        throw std::runtime_error(msg.c_str());
    }
}

void I2CCommLib::SendStartFWUpdate(uint32_t imgFileSize, uint8_t fwType)
{
    uint8_t deviceOffset = WR_DEVICE_REG;
    uint8_t reg_msb = (deviceOffset & 0xff00) >> 8;
    uint8_t reg_lsb = (deviceOffset & 0xff);

    struct StartFWUpdateCommand
    {
        uint8_t regMsb;
        uint8_t regLsb;
        uint8_t checkSum;
        uint8_t versionMajor;
        uint8_t versionMinor;
        uint8_t command;
        uint8_t reserved;
        uint8_t length4;
        uint8_t length3;
        uint8_t length2;
        uint8_t length1;
        uint8_t fwID;
        uint8_t otherOptions;
        uint8_t fwSize4;
        uint8_t fwSize3;
        uint8_t fwSize2;
        uint8_t fwSize1;
    } __attribute__((packed));

    StartFWUpdateCommand startFWUpdatecmd;
    std::vector<uint8_t> buf(sizeof(startFWUpdatecmd), 0);

    startFWUpdatecmd.regMsb = reg_msb;
    startFWUpdatecmd.regLsb = reg_lsb;
    startFWUpdatecmd.checkSum = EMPTY;
    startFWUpdatecmd.versionMajor = CEC_VERSION_MAJOR;
    startFWUpdatecmd.versionMinor = CEC_VERSION_MINOR;
    startFWUpdatecmd.command = START_FW_UPDATE_CMD;
    startFWUpdatecmd.reserved = EMPTY;
    startFWUpdatecmd.length4 = EMPTY;
    startFWUpdatecmd.length3 = EMPTY;
    startFWUpdatecmd.length2 = EMPTY;
    startFWUpdatecmd.length1 = START_FW_CMD_LEN;
    startFWUpdatecmd.fwID = fwType;
    startFWUpdatecmd.otherOptions = EMPTY;
    startFWUpdatecmd.fwSize4 = (imgFileSize & 0xff000000) >> 24;
    startFWUpdatecmd.fwSize3 = (imgFileSize & 0xff0000) >> 16;
    startFWUpdatecmd.fwSize2 = (imgFileSize & 0xff00) >> 8;
    startFWUpdatecmd.fwSize1 = (imgFileSize & 0xff) >> 0;

    memcpy(&buf[0], &startFWUpdatecmd, sizeof(startFWUpdatecmd));

    try
    {
        UpdateCheckSum(buf);

        std::unique_ptr<I2CInterface> myDevice =
            create(busId, deviceAddr, true);

        uint8_t size = buf.size();

        myDevice->writeCustom(deviceOffset, size, &buf[0]);

    }
    catch (const std::exception& e)
    {
        std::string msg = "SendStartFWUpdate: ";
        msg += e.what();
        log<level::ERR>("I2CCommLib - SendStartFWUpdate command failed.",
                        entry("EXCEPTION=%s", msg.c_str()));
        throw std::runtime_error(msg.c_str());
    }
}

void I2CCommLib::SendImageToCEC(std::string& fileName, uint32_t imageSize)
{
    uint8_t deviceOffset = WR_DEVICE_REG;
    uint8_t reg_msb = (deviceOffset & 0xff00) >> 8;
    uint8_t reg_lsb = (deviceOffset & 0xff);
    constexpr uint8_t BLOCK_SIZE{128};
    uint32_t totalPage{0};
    uint32_t otaHeaderOffset{0};

    struct ImageTransferCommand
    {
        uint8_t regMsb;
        uint8_t regLsb;
        uint8_t checkSum;
        uint8_t versionMajor;
        uint8_t versionMinor;
        uint8_t command;
        uint8_t reserved;
        uint8_t length4;
        uint8_t length3;
        uint8_t length2;
        uint8_t length1;
    } __attribute__((packed));

    try
    {
        totalPage = (imageSize+BLOCK_SIZE-1)/ BLOCK_SIZE;

        fsys::path filePath(fileName);

        std::ifstream fwFile(fileName.c_str(), std::ios::in | std::ios::binary);

        std::vector<uint8_t> imageData(OTA_HEADER_SIZE);

        uint32_t fileSize = static_cast<uint32_t>(fsys::file_size(filePath));

        if (filePath.extension() == romExtension)
        {
           imageData.resize(OTA_HEADER_SIZE + GPU_ROM_IMAGE_SIZE);
           if(fileSize > MB_SIZE)
           {
               otaHeaderOffset = OTA_HEADER_OFFSET_2MB_FILE_SIZE;
           }
           else
           {
               otaHeaderOffset = OTA_HEADER_OFFSET_1MB_FILE_SIZE;
           }
           fwFile.seekg(otaHeaderOffset);
           fwFile.read(reinterpret_cast<char*>(imageData.data()), OTA_HEADER_SIZE);
           fwFile.seekg(0, std::ios::beg);
           fwFile.read(reinterpret_cast<char*>(imageData.data()), GPU_ROM_IMAGE_SIZE);

        }
        else if(filePath.extension() == binExtension)
        {
           imageData.resize(fileSize);
           fwFile.seekg(0, std::ios::beg);
           fwFile.read(reinterpret_cast<char*>(imageData.data()), fileSize);
        }
        else
        {
             throw std::runtime_error("I2CCommLib - SendImageToCEC Invalid file format");
        }


        for(uint32_t page=0; page <totalPage; page++)
        { 
            bool lastPageSet{false};
            ImageTransferCommand transferImgCmd;

            transferImgCmd.regMsb = reg_msb;
            transferImgCmd.regLsb = reg_lsb;
            transferImgCmd.checkSum = EMPTY;
            transferImgCmd.versionMajor = CEC_VERSION_MAJOR;
            transferImgCmd.versionMinor = CEC_VERSION_MINOR;
            transferImgCmd.command = COPY_IMG_COMPLETE_CMD;
            transferImgCmd.reserved = EMPTY;
            transferImgCmd.length4 = EMPTY;
            transferImgCmd.length3 = EMPTY;
            transferImgCmd.length2 = EMPTY;
            transferImgCmd.length1 = BLOCK_SIZE;

            std::vector<uint8_t> buf(sizeof(transferImgCmd), 0);

            //send the last page.
            if(page == totalPage -1)
            {
               uint32_t lastBlockSize{0};

               if(page == 0) 
               {
                   lastBlockSize = imageSize;
               }
               else
               {
                   lastBlockSize = imageSize-(page*BLOCK_SIZE);
               }  
               transferImgCmd.length1 = lastBlockSize;


               buf.resize(sizeof(transferImgCmd) + lastBlockSize);
               memcpy(&buf[0], &transferImgCmd, sizeof(transferImgCmd));
               memcpy(&buf[0]+sizeof(transferImgCmd), &imageData[page*BLOCK_SIZE], lastBlockSize);
               lastPageSet = true;
            }
            else
            {
               lastPageSet = false;

               buf.resize(sizeof(transferImgCmd) + BLOCK_SIZE);

               memcpy(&buf[0], &transferImgCmd, sizeof(transferImgCmd));
               memcpy(&buf[0]+sizeof(transferImgCmd), &imageData[page*BLOCK_SIZE], BLOCK_SIZE);
            }
            try
            {
               uint8_t retVal = static_cast<uint8_t>(CommandStatus::UNKNOWN);
               constexpr uint16_t sleepBeforeReadLastPage{2000};
               constexpr uint8_t sleepBeforeRead{100};

               auto  setWaitInSecs =
                         std::chrono::milliseconds(sleepBeforeRead);

               if(lastPageSet)
               {
                   setWaitInSecs =
                         std::chrono::milliseconds(sleepBeforeReadLastPage);
               }

               UpdateCheckSum(buf);

               uint8_t size = buf.size();

               std::unique_ptr<I2CInterface> myDevice =
                    create(busId, deviceAddr, true);
               myDevice->writeCustom(deviceOffset, size, &buf[0]);

               std::this_thread::sleep_for(setWaitInSecs);

               retVal = GetLastCmdStatus();

               if ( (retVal != static_cast<uint8_t>(CommandStatus::SUCCESS)) &&
                     (retVal != static_cast<uint8_t>(CommandStatus::ERR_BUSY)))
               {
                   std::string msg = "I2CCommLib: ";
                   log<level::ERR>(
                     "I2CCommLib - SendImageToCEC Read commands status failed.",
                         entry("ERR=0x%x", retVal));

                   msg += "- SendImageToCEC Read commands status failed.";
                   throw std::runtime_error(msg.c_str());
               }
            }
            catch (const std::exception& e)
            {
                std::string msg = "SendImageToCEC: ";
                msg += e.what();
                log<level::ERR>("I2CCommLib - SendImageToCEC command failed.",
                             entry("EXCEPTION=%s", msg.c_str()));
                throw std::runtime_error(msg.c_str());
            }
        }


        //Sleep for 3 secs after sending the last block to ensure CEC is ready
        //to update f/w update status.
        constexpr auto setWaitInSecs =
           std::chrono::milliseconds(3000);

        std::this_thread::sleep_for(setWaitInSecs);
    }
    catch (const std::exception& e)
    {
        std::string msg = "SendImageToCEC: ";

        msg += e.what();
        log<level::ERR>("I2CCommLib - SendImageToCEC failed, other failures.",
                        entry("EXCEPTION=%s", msg.c_str()));
        throw std::runtime_error(msg.c_str());
    }

}

void I2CCommLib::SendCopyImageComplete()
{
    uint8_t deviceOffset = WR_DEVICE_REG;
    uint8_t reg_msb = (deviceOffset & 0xff00) >> 8;
    uint8_t reg_lsb = (deviceOffset & 0xff);

    struct CopyImageCompleteCommand
    {
        uint8_t regMsb;
        uint8_t regLsb;
        uint8_t checkSum;
        uint8_t versionMajor;
        uint8_t versionMinor;
        uint8_t command;
        uint8_t reserved;
        uint8_t length4;
        uint8_t length3;
        uint8_t length2;
        uint8_t length1;
    } __attribute__((packed));

    CopyImageCompleteCommand copyImgCmd;
    std::vector<uint8_t> buf(sizeof(copyImgCmd), 0);

    copyImgCmd.regMsb = reg_msb;
    copyImgCmd.regLsb = reg_lsb;
    copyImgCmd.checkSum = EMPTY;
    copyImgCmd.versionMajor = CEC_VERSION_MAJOR;
    copyImgCmd.versionMinor = CEC_VERSION_MINOR;
    copyImgCmd.command = COPY_IMG_COMPLETE_CMD;
    copyImgCmd.reserved = EMPTY;
    copyImgCmd.length4 = EMPTY;
    copyImgCmd.length3 = EMPTY;
    copyImgCmd.length2 = EMPTY;
    copyImgCmd.length1 = EMPTY;

    memcpy(&buf[0], &copyImgCmd, sizeof(copyImgCmd));
    try
    {
        UpdateCheckSum(buf);

        uint8_t size = buf.size();

        std::unique_ptr<I2CInterface> myDevice =
            create(busId, deviceAddr, true);
        myDevice->writeCustom(deviceOffset, size, &buf[0]);

    }
    catch (const std::exception& e)
    {
        std::string msg = "SendCopyImageComplete: ";
        msg += e.what();
        log<level::ERR>("I2CCommLib - SendCopyImageComplete command failed.",
                        entry("EXCEPTION=%s", msg.c_str()));
        throw std::runtime_error(msg.c_str());
    }
}

void I2CCommLib::SendBMCReset()
{
    uint8_t deviceOffset = WR_DEVICE_REG;
    uint8_t reg_msb = (deviceOffset & 0xff00) >> 8;
    uint8_t reg_lsb = (deviceOffset & 0xff);

    struct BMCResetCommand
    {
        uint8_t regMsb;
        uint8_t regLsb;
        uint8_t checkSum;
        uint8_t versionMajor;
        uint8_t versionMinor;
        uint8_t command;
        uint8_t reserved;
        uint8_t length4;
        uint8_t length3;
        uint8_t length2;
        uint8_t length1;
    } __attribute__((packed));

    BMCResetCommand bmcResetCmd;
    std::vector<uint8_t> buf(sizeof(bmcResetCmd), 0);

    bmcResetCmd.regMsb = reg_msb;
    bmcResetCmd.regLsb = reg_lsb;
    bmcResetCmd.checkSum = EMPTY;
    bmcResetCmd.versionMajor = CEC_VERSION_MAJOR;
    bmcResetCmd.versionMinor = CEC_VERSION_MINOR;
    bmcResetCmd.command = BMC_RESET_CMD;
    bmcResetCmd.reserved = EMPTY;
    bmcResetCmd.length4 = EMPTY;
    bmcResetCmd.length3 = EMPTY;
    bmcResetCmd.length2 = EMPTY;
    bmcResetCmd.length1 = EMPTY;

    memcpy(&buf[0], &bmcResetCmd, sizeof(bmcResetCmd));
    try
    {
        UpdateCheckSum(buf);

        uint8_t size = buf.size();

        std::unique_ptr<I2CInterface> myDevice =
            create(busId, deviceAddr, true);
        myDevice->writeCustom(deviceOffset, size, &buf[0]);

    }
    catch (const std::exception& e)
    {
        std::string msg = "SendBMCReset: ";
        msg += e.what();
        log<level::ERR>("I2CCommLib - SendBMCReset command failed.",
                        entry("EXCEPTION=%s", msg.c_str()));
        throw std::runtime_error(msg.c_str());
    }
}

int I2CCommLib::HexStringToRandomList(const std::string& str,
                                      std::vector<uint8_t>& randomData)
{
    int size = str.size();
    if(size != (DEFAULT_RANDON_NUMBERS*2))
    {
        log<level::ERR>("HexStringToRandomList: Invalid custom Random Number length");
        return -1;
    }
    try
    {
        if (!std::all_of(str.c_str(), str.c_str()+size , ::isxdigit))
        {
            log<level::ERR>("HexStringToRandomList: Invalid custom Random Number");
            return -1;
        }

        for (int i {0}; i < size; i += 2)
        {
            std::string hexstr{str[i]};
            hexstr += str[i + 1];
            int num = std::stoi(hexstr,0,16);
            randomData.push_back(num);
        }
    }
    catch (const std::exception& e)
    {
        std::string msg = "Exception: HexStringToRandomList: ";
        msg += e.what();
        log<level::ERR>(msg.c_str());
        return -1;
    }
    return randomData.size();
}

int I2CCommLib::CreateRandomList(std::vector<uint8_t>& randomData)
{
    fsys::path userRandomListFile(cecAttestFolder+cecAttestRandomFile);

    if (fsys::exists(userRandomListFile.string()))
    {
        RemovablePath removesignatureFile(userRandomListFile.string());
        std::ifstream randomFile(userRandomListFile.c_str(),std::ifstream::binary);
        std::stringstream randStream;
        randStream << randomFile.rdbuf();
        randomFile.close();

        auto randData = randStream.str();
        int retVal = HexStringToRandomList(randData, randomData);

        if( retVal < 0 || randomData.size() != DEFAULT_RANDON_NUMBERS)
        {
            log<level::ERR>("Failed. Check the custom provided random numbers.");
            return FAILURE;
        }
    }
    else
    {
        std::srand(std::time(nullptr));
        uint8_t cnt{0};
        while(cnt < DEFAULT_RANDON_NUMBERS)
        {
            randomData.push_back((std::rand() % 100));
            cnt++;
        }
    }
    return SUCCESS;
}

void I2CCommLib::CreateDERSignature(const std::vector<uint8_t>& data, uint16_t dataSize, std::string filename)
{
     uint16_t sigStartAddr = dataSize-SIGNATURE_SIZE;
     uint8_t sigTotalLen = SIGNATURE_SIZE+4;
     uint8_t s_len = SIGNATURE_SIZE/2;
     uint8_t r_len= SIGNATURE_SIZE/2;

     uint16_t s_lenStartaddr = sigStartAddr;
     uint16_t r_lenStartaddr = sigStartAddr + (SIGNATURE_SIZE/2);

        if(data[s_lenStartaddr] > 0x7F)
        {
           sigTotalLen += 1;
           r_len += 1;
        }

        if(data[r_lenStartaddr] > 0x7F)
        {
           sigTotalLen += 1;
           s_len += 1;
        }

        std::ofstream signfile(filename, std::ios::binary | std::ios::out);
        uint8_t temp  = 0x30;
        signfile << static_cast<char>(temp);
        signfile << static_cast<char>(sigTotalLen);
        temp  = 0x02;
        signfile << static_cast<char>(temp);
        signfile << static_cast<char>(r_len);
        if(data[s_lenStartaddr] > 0x7F)
        {
            temp  = 0x00;
            signfile << static_cast<char>(temp);
        }
        signfile.write(reinterpret_cast<const char*>(&data[s_lenStartaddr]),SIGNATURE_SIZE/2);
        temp  = 0x02;
        signfile << static_cast<char>(temp);
        signfile << static_cast<char>(s_len);
        if(data[r_lenStartaddr] > 0x7F)
        {
            temp  = 0x00;
            signfile << static_cast<char>(temp);
        }
        signfile.write(reinterpret_cast<const char*>(&data[r_lenStartaddr]),SIGNATURE_SIZE/2);
        signfile.close();
}

bool I2CCommLib::VerifySignature(const fsys::path& dataFile, const fsys::path& sigFile,
                             const fsys::path& publicKeyFile)
{
    EVP_MD_CTX_Ptr md_ctx(EVP_MD_CTX_new(), ::EVP_MD_CTX_free);
    EC_KEY *ECKey = nullptr;
    EVP_PKEY *pubKey = nullptr;
    unsigned int digestLength = 0;
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned char *pTmp = nullptr;
    int derPubkeyLen = 0;
    uint8_t verifyReturn{0};

    try
    {
        // Check existence of the files in the system.
        if (!fsys::exists(dataFile))
        {
           log<level::ERR>("Failed to find the data file.",
                          entry("FILE=%s", dataFile.c_str()));
           throw std::runtime_error("Failed to find the Data file.");
        }
        if (!fsys::exists(sigFile))
        {
           log<level::ERR>("Failed to find the signature file.",
                          entry("FILE=%s", sigFile.c_str()));
           throw std::runtime_error("Failed to find the signature file.");
        }
        if (!fsys::exists(publicKeyFile))
        {
           log<level::ERR>("Failed to find the publickey file.",
                          entry("FILE=%s", publicKeyFile.c_str()));
           throw std::runtime_error("Failed to find the publickey file.");
        }

        std::ifstream pubKeyFile(publicKeyFile.c_str(),std::ifstream::binary);
        std::stringstream pKeyStream;
        pKeyStream << pubKeyFile.rdbuf();
        pubKeyFile.close();

        std::ifstream attestDataFile(dataFile.c_str(),std::ifstream::binary);
        std::stringstream dataStream;
        dataStream << attestDataFile.rdbuf();
        attestDataFile.close();

        std::ifstream signatureFile(sigFile.c_str(),std::ifstream::binary);
        std::stringstream signStream;
        signStream << signatureFile.rdbuf();
        signatureFile.close();

        auto pKeyData = pKeyStream.str();
        auto pKeyDataLen = pKeyData.size();

        auto signData = signStream.str();
        auto signDataLen = signData.size();

        auto bufferData = dataStream.str();
        auto bufferDataLen = bufferData.size();

        BIO_MEM_Ptr pbioKeyFile(BIO_new_mem_buf( &pKeyData[0], pKeyDataLen ),&::BIO_free);

        pubKey = PEM_read_bio_PUBKEY( pbioKeyFile.get(), &pubKey, NULL, NULL );
        if ( pubKey == nullptr )
        {
            throw std::runtime_error("Load key failed.");
        }
        ECKey = EVP_PKEY_get1_EC_KEY(pubKey);
        if ( !ECKey )
        {
           throw std::runtime_error("Get ec key failed.");
        }

        derPubkeyLen = i2d_EC_PUBKEY( ECKey, NULL );
        if ( derPubkeyLen != i2d_EC_PUBKEY( ECKey, &pTmp ) )
        {
           throw std::runtime_error("Get ec public key failed.");
        }

        EVP_MD_CTX_init( md_ctx.get() );
        if ( !EVP_DigestInit( md_ctx.get(), EVP_sha384() ) )
        {
           throw std::runtime_error("Digest init failed.");
        }

        if ( !EVP_DigestUpdate( md_ctx.get(), (const void *)&bufferData[0], bufferDataLen) )
        {
           throw std::runtime_error("Digest update failed.");
        }

        if ( !EVP_DigestFinal( md_ctx.get(), digest, &digestLength ) )
        {
           throw std::runtime_error("Digest final failed.");
        }

        verifyReturn =
          ECDSA_verify( 0, (const unsigned char*)digest, digestLength, (const unsigned char *)&signData[0], signDataLen, ECKey );

    }
    catch (const std::exception& e)
    {
        verifyReturn = 0;
        std::string msg = "Exception: VerifySignature: ";
        msg += e.what();
        log<level::ERR>(msg.c_str());
    }
    if (ECKey)
    {
        EC_KEY_free( ECKey );
        ECKey = nullptr;
    }
    if (pTmp)
    {
        OPENSSL_free(pTmp);
    }
    return (verifyReturn == 1);
}

void I2CCommLib::WriteStatusFile(std::string &status)
{
    std::ofstream statusfile(cecAttestFolder + cecAttestStatusFile,
                              std::ios::binary | std::ios::out);
    statusfile.write(status.c_str(),status.length());
    statusfile.close();
}

std::string I2CCommLib::GetCommandStatusStr(uint8_t status)
{
    if (static_cast<I2CCommLib::CommandStatus>(status) <= CommandStatus::UNKNOWN)
    {
        return I2CCommLib::commandStatusStr[status];
    }
    return std::string{};
}

void I2CCommLib::GetAttestation(uint16_t dataSize, uint16_t blkSize)
{
    uint8_t deviceOffset = WR_DEVICE_REG;
    uint8_t reg_msb = (deviceOffset & 0xff00) >> 8;
    uint8_t reg_lsb = (deviceOffset & 0xff);

    std::map<uint16_t, uint8_t> blockSizeMap{ {BLOCK_SIZE_128_BYTE, BLOCK_SIZE_128_VALUE},
                                              {BLOCK_SIZE_64_BYTE, BLOCK_SIZE_64_VALUE} ,
                                              {BLOCK_SIZE_48_BYTE, BLOCK_SIZE_48_VALUE},
                                              {BLOCK_SIZE_32_BYTE, BLOCK_SIZE_32_VALUE} };

    struct StartAttestationCommand
    {
        uint8_t regMsb;
        uint8_t regLsb;
        uint8_t checkSum;
        uint8_t versionMajor;
        uint8_t versionMinor;
        uint8_t command;
        uint8_t reserved;
        uint8_t length4;
        uint8_t length3;
        uint8_t length2;
        uint8_t length1;
        uint8_t blockLength;
        uint8_t otherOptions;
        uint8_t randomNumber[DEFAULT_RANDON_NUMBERS];
    } __attribute__((packed));

    StartAttestationCommand startAttestcmd;
    std::vector<uint8_t> writeBuf(sizeof(startAttestcmd), 0);
    std::vector<uint8_t> randomNumber;
    std::string status{"Completed successfully."};
    std::ofstream statusfile(cecAttestFolder + cecAttestStatusFile,
                              std::ios::binary | std::ios::out);
    fsys::path publicKeyFile(cecAttestFolder + cecAttestPublicKeyFile);
    fsys::path responseFile(cecAttestFolder + cecAttestPayloadFile);
    fsys::path dataFile(cecAttestFolder + cecAttestDataFile);
    fsys::path signatureFile(cecAttestFolder + cecAttestSignatureFile);
    std::string throwableError = "StartAttestcmd: ";
    RemovablePath removesignatureFile(signatureFile);
    RemovablePath removedataFile(dataFile);
    RemovablePath removePubKeyFile(publicKeyFile);

    try
    {
        if(CreateRandomList(randomNumber) != SUCCESS)
        {
            status = "Failed. Check the custom provided random numbers.";
            throwableError += "Failed. Check the custom provided random numbers.";
            WriteStatusFile(status);
            throw std::runtime_error(throwableError.c_str());
        }
    }
    catch (const std::exception& e)
    {
        status = (status.find("Failed") == std::string::npos)?"Failed. Internal issues.":status;
        WriteStatusFile(status);
        throwableError += e.what();
        log<level::ERR>("I2CCommLib - StartAttestcmd command failed.",
                        entry("EXCEPTION=%s", throwableError.c_str()));
        throw std::runtime_error(throwableError.c_str());
    }
    try
    {
        // add 2 bytes for blockLength and otherOptions attributes.
        uint8_t payloadLen = randomNumber.size() + 2;
        uint8_t blkLen = EMPTY;
        blkLen  = (blockSizeMap.find(blkSize) != blockSizeMap.end())? blockSizeMap[blkSize] : BLOCK_SIZE_128_VALUE;
        blkSize = (blockSizeMap.find(blkSize) == blockSizeMap.end())? BLOCK_SIZE_128_BYTE : blkSize;

        startAttestcmd.regMsb = reg_msb;
        startAttestcmd.regLsb = reg_lsb;
        startAttestcmd.checkSum = EMPTY;
        startAttestcmd.versionMajor = CEC_VERSION_MAJOR;
        startAttestcmd.versionMinor = CEC_VERSION_MINOR;
        startAttestcmd.command = ATTESTATION_CMD;
        startAttestcmd.reserved = EMPTY;
        startAttestcmd.length4 = (payloadLen & 0xff000000) >> 24;
        startAttestcmd.length3 = (payloadLen & 0xff0000) >> 16;
        startAttestcmd.length2 = (payloadLen & 0xff00) >> 8;
        startAttestcmd.length1 = (payloadLen & 0xff) >> 0;
        startAttestcmd.blockLength = blkLen;
        startAttestcmd.otherOptions = EMPTY;

        memcpy(&startAttestcmd.randomNumber[0],&randomNumber[0],randomNumber.size());
        memcpy(&writeBuf[0], &startAttestcmd, sizeof(startAttestcmd));

        UpdateCheckSum(writeBuf);

        std::unique_ptr<I2CInterface> myDevice =
            create(busId, deviceAddr, true);

        uint8_t size = writeBuf.size();
        uint8_t retry{0};

        myDevice->writeCustom(deviceOffset, size, &writeBuf[0]);

        uint16_t sleepBeforeRead{5};
        auto  setWaitInSecs =
                         std::chrono::milliseconds(sleepBeforeRead);

        std::this_thread::sleep_for(setWaitInSecs);

        uint8_t retVal = static_cast<uint8_t>(CommandStatus::UNKNOWN);

        retVal = GetLastCmdStatus();

        while(retVal == static_cast<uint8_t>(CommandStatus::ERR_BUSY) &&
                            retry < 10)
        {
            sleepBeforeRead = 1000;
            setWaitInSecs = std::chrono::milliseconds(sleepBeforeRead);

            retVal = static_cast<uint8_t>(CommandStatus::UNKNOWN);
            std::this_thread::sleep_for(setWaitInSecs);
            retVal = GetLastCmdStatus();
            retry++;
        }
        if (retVal != static_cast<uint8_t>(CommandStatus::SUCCESS))
        {
            std::string msg = "I2CCommLib: ";
            log<level::ERR>(
               "I2CCommLib - StartAttestcmd command status failed.",
               entry("ERR=0x%x", retVal));

            msg += "- StartAttestcmd command status failed.";
            throw std::runtime_error(msg.c_str());
        }

        uint8_t readDeviceOffset = CHALLENGE_RESPONSE_REG;

        uint16_t remainLen = dataSize;
        uint16_t sigCount{0};
        std::vector<uint8_t> completeBuf(dataSize, 0);

        while(remainLen > 0 )
        {
            uint8_t blockLen = (remainLen > blkSize)? blkSize : remainLen;
            uint8_t correctBlockLen = blkSize - blockLen;

            remainLen = (remainLen > blkSize) ? (remainLen - blkSize ): 0;

            correctBlockLen += (blkSize != BLOCK_SIZE_128_BYTE)? (BLOCK_SIZE_128_BYTE-blkSize):0;

            retVal =
                static_cast<uint8_t>(FirmwareUpdateStatus::STATUS_CODE_OTHER);

            struct ReadData
            {
                uint8_t checkSum;
                uint8_t data[BLOCK_SIZE_128_BYTE];
            } __attribute__((packed));

            ReadData readStruct{};
            std::vector<uint8_t> readBuf(sizeof(readStruct) - correctBlockLen, 0);
            memcpy(&readBuf[0], &readStruct, sizeof(readStruct) - correctBlockLen);

            size = readBuf.size();

            myDevice->readCustom(readDeviceOffset, size, &readBuf[0]);

            VerifyCheckSum(readBuf);
            //Ignore checksum and write rest of the data to file.
            memcpy(&completeBuf[sigCount], &readBuf[1], size-1);
            sigCount += (size-1);
        }
        if(!std::equal(randomNumber.begin(),randomNumber.end(),completeBuf.begin()))
        {
            throw std::runtime_error("Failed.Random numbers are different.");
        }
        std::ofstream attestResponseFile(responseFile.string(),std::ios::binary | std::ios::out);
        attestResponseFile.write(reinterpret_cast<char*>(&completeBuf[0]),completeBuf.size());
        attestResponseFile.close();


        if (fsys::exists(publicKeyFile))
        {
            std::ofstream attestDataFile(dataFile.string(), std::ios::binary | std::ios::out);
            attestDataFile.write(reinterpret_cast<char*>(&completeBuf[0]),completeBuf.size()-SIGNATURE_SIZE);
            attestDataFile.close();
            CreateDERSignature(completeBuf, dataSize, signatureFile.string());
            try
            {
               auto valid = VerifySignature(dataFile, signatureFile, publicKeyFile);
               status = (!valid) ? "Failed.Signature validation failure." : status;
            }
            catch (const std::exception& e)
            {
                status = "Failed.Exception during signature validation.";
                log<level::ERR>("Failed. Exception during signature validation.");
            }
        }
        WriteStatusFile(status);
    }
    catch (const std::exception& e)
    {
        status = "Failed.Other internal failures.";
        WriteStatusFile(status);
        throwableError += e.what();
        log<level::ERR>("I2CCommLib - StartAttestcmd command failed.",
                        entry("EXCEPTION=%s", throwableError.c_str()));
        throw std::runtime_error(throwableError.c_str());
    }
}


} // namespace updater
} // namespace software
} // namespace phosphor
