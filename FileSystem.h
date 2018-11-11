#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <memory>
#include <cassert>
#include <unordered_map>
#include <cstdint>


constexpr inline unsigned int ceil(unsigned int a, unsigned int b) noexcept {
    if (a % b == 0) return a / b;
    else return (a / b + 1);
}


struct Block {
    public:
        static uint16_t SIZE;
        static const uint16_t INVALID;
    private:
        std::vector<uint8_t> bytes;
    public:
        inline uint8_t& operator[](unsigned int index) {
            return bytes.at(index);
        }

        inline const uint8_t& operator[](unsigned int index) const {
            return bytes.at(index);
        }

        inline const uint8_t* asArray() const {
            return bytes.data();
        }

        Block();
        Block(std::vector<uint8_t> bytes);
        Block(uint8_t* bytes);
};

void writeBlock(std::fstream& device, unsigned int index, const Block& block);
void writeBlocks(std::fstream& device, unsigned int shift, const std::vector<Block>& blocks);
Block readBlock(std::fstream& device, unsigned int index);
std::vector<Block> readBlocks(std::fstream& device, unsigned int shift, unsigned int amount);

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
        void flush(std::fstream& device) const;

        void clear();
        void add(uint8_t byte);

        DeviceBlockMap();
        DeviceBlockMap(std::vector<uint8_t> map);
};

enum class DeviceFileType : uint8_t {
    Empty,
    Regular,
    Directory,
    Symlink
};

struct DeviceFileDescriptor {
    public:
        static unsigned int BLOCKS_PER_FILE;

        DeviceFileType fileType;
        uint16_t size; // in bytes
        uint8_t linksCount;
        std::vector<uint16_t> blocks;

        DeviceFileDescriptor();
        DeviceFileDescriptor(DeviceFileType fileType, uint16_t size,
                uint8_t linksCount, std::vector<uint16_t> blocks);

        inline static unsigned int sizeInBytes() {
            return sizeof(fileType) + sizeof(size) + sizeof(linksCount)
                + BLOCKS_PER_FILE * sizeof(uint16_t);
        }

        inline static unsigned int sizeInBlocks() {
            const unsigned int bytes = sizeInBytes();
            assert(bytes % Block::SIZE == 0);
            return (bytes / Block::SIZE);
        }
};


/*  */
/* class File { */
/*     const FileType m_FileType; */
/*  */
/*     public: */
/*         File(FileType fileType) : m_FileType(fileType) {} */
/* }; */
/*  */
/* class RegularFile : public File { */
/*     unsigned int linksCount; */
/*     unsigned int size; */
/*     std::vector<unsigned int> takenBlocks; */
/*     unsigned int extendedTakenBlocks; */
/* }; */
/*  */
/* class Directory : public File { */
/*     std::vector<DirectoryEntry> entries; */
/* }; */
/*  */
/* struct FileDescriptor { */
/*     FileType fileType; */
/* }; */
/*  */
/* struct DirectoryEntry { */
/*     std::string fileName; */
/*     FileDescriptor fd; */
/*  */
/*     DirectoryEntry(const std::string& fileName, const FileDescriptor& fd); */
/* }; */





class FileSystem {
    private:
        std::unique_ptr<std::fstream> m_Device;
        std::string m_DeviceName;
        DeviceHeader m_DeviceHeader;

        DeviceBlockMap m_DeviceBlockMap;


        // constexpr static unsigned int MAX_OPEN_FILES = 4;
        // std::array<std::optional<FileDescriptor>, MAX_OPEN_FILES> m_OpenFiles;



    public:
        bool process(Command command, std::vector<std::string>& arguments);

        void createEmptyDevice();

    private:

        bool mount(const std::string& deviceName);
        bool umount();
        bool filestat(unsigned int fd);
        bool ls();
        bool create(const std::string& name);
        bool open(const std::string& name, unsigned int& fd);
        bool close(unsigned int fd);
        bool read(unsigned int fd, unsigned int shift, unsigned int size, std::string& buff);
        bool write(unsigned int fd, unsigned int shift, unsigned int size, const std::string& buff);
        bool link(const std::string& name1, const std::string& name2);
        bool unlink(const std::string& name);
        bool truncate(const std::string& name, unsigned int size);
};


#endif
