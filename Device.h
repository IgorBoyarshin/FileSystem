#ifndef DEVICE_H
#define DEVICE_H

#include "Block.h"
#include <fstream>
#include <iostream>


struct Device {
    private:
        std::fstream m_Device;
        std::string m_DeviceName;
        const unsigned int size;

        static void writeBlock(std::fstream& file, unsigned int index, const Block& block);
        static void writeBlocks(std::fstream& file, unsigned int shift, const std::vector<Block>& blocks);
        static Block readBlock(std::fstream& file, unsigned int index);
        static std::vector<Block> readBlocks(std::fstream& file, unsigned int shift, unsigned int amount);

    public:
        static uint16_t BLOCK_SIZE;
        static uint16_t FD_BLOCKS_PER_FILE;
        static uint16_t MAP_START;
        static uint16_t FDS_START;
        static uint16_t DATA_START;

        void writeBlock(unsigned int index, const Block& block);
        void writeBlocks(unsigned int shift, const std::vector<Block>& blocks);
        Block readBlock(unsigned int index);
        std::vector<Block> readBlocks(unsigned int shift, unsigned int amount);

        inline explicit operator bool() const noexcept {
            return static_cast<bool>(m_Device);
        }

        inline bool is_open() const {
            return m_Device.is_open();
        }

        inline unsigned int getSize() {
            return size;
        }

        inline Device(const std::string& deviceName)
                : m_Device(deviceName, m_Device.binary | m_Device.in
                                        | m_Device.out | m_Device.ate),
                m_DeviceName(deviceName),
                size(m_Device.tellg()) {}

        static void createEmpty(const std::string& name);
};


enum class Command {
    Mount,
    Umount,
    Filestat,
    Ls,
    Create,
    Open,
    Close,
    Read,
    Write,
    Link,
    Unlink,
    Truncate,
    INVALID
};

std::string toString(Command command);
Command toCommand(const std::string& str);


struct DeviceHeader {
    public:
        uint16_t blockSize; // in bytes
        uint16_t maxFiles;
        uint16_t blocksPerFile;
        uint16_t firstLogicalBlockShift;

        inline DeviceHeader()
            : DeviceHeader(0, 0, 0, 0) {}
        inline DeviceHeader(uint16_t blockSize, uint16_t maxFiles,
                uint16_t blocksPerFile, uint16_t firstLogicalBlockShift)
            : blockSize(blockSize), maxFiles(maxFiles), blocksPerFile(blocksPerFile),
            firstLogicalBlockShift(firstLogicalBlockShift) {}
        inline DeviceHeader(const Block& block)
            : blockSize(static_cast<uint16_t>(block[1]) << 8 | block[0]),
              maxFiles(static_cast<uint16_t>(block[3]) << 8 | block[2]),
              blocksPerFile(static_cast<uint16_t>(block[5]) << 8 | block[4]),
              firstLogicalBlockShift(static_cast<uint16_t>(block[7]) << 8 | block[6]) {}
};

class DeviceBlockMap {
    // private:
    public:
        std::vector<uint8_t> m_BlocksUsageMap;

    // public:
        /* static unsigned int SIZE_IN_BLOCKS; */
        // Returns whether is free
        bool operator[](unsigned int blockIndex) const;

        // The tail of the last block stored is unspecified
        std::vector<Block> serialize() const;

        void clear();
        void add(uint8_t byte);
        inline unsigned int sizeBlocks() const {
            return ceil(m_BlocksUsageMap.size(), Device::BLOCK_SIZE);
        }

        DeviceBlockMap();
        DeviceBlockMap(std::vector<uint8_t> map);
};

enum class DeviceFileType : uint8_t {
    Empty = 0,
    Regular,
    Directory,
    Symlink
};

inline uint8_t toInt(DeviceFileType deviceFileType) {
    return static_cast<uint8_t>(deviceFileType);
}

inline DeviceFileType toDeviceFileType(uint8_t fileType) {
    if (fileType == 0) return DeviceFileType::Empty;
    else if (fileType == 1) return DeviceFileType::Regular;
    else if (fileType == 2) return DeviceFileType::Directory;
    else if (fileType == 3) return DeviceFileType::Symlink;
    assert(false);
    exit(-1);
}

inline std::ostream& operator<<(std::ostream& stream, const DeviceFileType& dft) {
    switch (dft) {
        case DeviceFileType::Empty:
            stream << "Empty";
            break;
        case DeviceFileType::Regular:
            stream << "Regular";
            break;
        case DeviceFileType::Directory:
            stream << "Directory";
            break;
        case DeviceFileType::Symlink:
            stream << "Symlink";
            break;
    }

    return stream;
}

struct DeviceFileDescriptor {
    public:
        /* static unsigned int BLOCKS_PER_FILE; */

        DeviceFileType fileType;
        uint16_t size; // in bytes
        uint8_t linksCount;
        std::vector<uint16_t> blocks;

        DeviceFileDescriptor();
        DeviceFileDescriptor(DeviceFileType fileType, uint16_t size,
                uint8_t linksCount, std::vector<uint16_t> blocks);
        DeviceFileDescriptor(const std::vector<Block>& rawBlocks);

        static DeviceFileDescriptor read(Device& device, unsigned int index);

        std::vector<Block> serialize() const;

        inline static unsigned int sizeInBytes() {
            return sizeof(fileType) + sizeof(size) + sizeof(linksCount)
                + Device::FD_BLOCKS_PER_FILE * sizeof(uint16_t);
        }

        inline static unsigned int sizeInBlocks() {
            const unsigned int bytes = sizeInBytes();
            assert(bytes % Device::BLOCK_SIZE == 0);
            return (bytes / Device::BLOCK_SIZE);
        }
};

inline std::ostream& operator<<(std::ostream& stream, const DeviceFileDescriptor& dfd) {
    stream << "Filetype=" << dfd.fileType << std::endl;
    stream << "Size=" << dfd.size << " bytes" << std::endl;
    stream << "Hard links=" << static_cast<int>(dfd.linksCount) << std::endl;
    return stream;
}


#endif
