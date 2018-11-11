#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <memory>
#include <cassert>
#include <cmath>
#include <unordered_map>
#include <cstdint>


struct Block {
    public:
        static unsigned int SIZE;
    private:
        std::vector<char> bytes;
    public:
        inline char& operator[](unsigned int index) {
            return bytes.at(index);
        }

        inline const char& operator[](unsigned int index) const {
            return bytes.at(index);
        }

        inline const char* asArray() const {
            return bytes.data();
        }

        Block();
        Block(std::vector<char> bytes);
        Block(char* bytes);
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
        uint16_t maxFiles; // max amount of files

        inline DeviceHeader()
            : DeviceHeader(0, 0) {}
        inline DeviceHeader(unsigned int blockSize, unsigned int maxFiles)
            : blockSize(blockSize), maxFiles(maxFiles) {}
};

class DeviceBlockMap {
    private:
        std::vector<char> m_BlockAvailabilityMap;

    public:
        // Returns whether is free
        bool operator[](unsigned int blockIndex) const;

        // The tail of the last block stored is unspecified
        void flush(std::fstream& device) const;

        void clear();
        void add(char block);

        DeviceBlockMap();
        DeviceBlockMap(std::vector<char> map);
};


/* enum class FileType { */
/*     Regular, */
/*     Directory */
/* }; */
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
