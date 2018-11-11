#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <memory>
#include <cassert>
#include <cmath>


struct Block {
    public:
        static const unsigned int SIZE = 8;
    private:
        std::array<char, SIZE> bytes;
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
        Block(std::array<char, SIZE> bytes);
        Block(char bytes[SIZE]);
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


struct DevHeader {
    public:
        /* unsigned int deviceSize; // in bytes */
        unsigned int blockSize; // in bytes
        unsigned int maxFiles; // max amount of files

        inline DevHeader()
            : DevHeader(0, 0) {}
        inline DevHeader(unsigned int blockSize, unsigned int maxFiles)
            : blockSize(blockSize), maxFiles(maxFiles) {}
};

std::ostream& operator<<(std::ostream& stream, const DevHeader& devHeader);


enum class FileType {
    Regular,
    Directory
};

class File {
    const FileType m_FileType;

    public:
        File(FileType fileType) : m_FileType(fileType) {}
};

struct FileDescriptor {
    FileType fileType;
    unsigned int linksCount;
    unsigned int size;
    std::vector<unsigned int> blocksMap;
    unsigned int extendedBlocksMapPtr;
};

struct DirectoryEntry {
    std::string fileName;
    FileDescriptor fd;

    DirectoryEntry(const std::string& fileName, const FileDescriptor& fd);
};

class RegularFile : public File {

};

class Directory : public File {
    std::vector<DirectoryEntry> entries;
};


class BlockMap {
    private:
        std::vector<char> m_BlockAvailabilityMap;

    public:
        // Returns whether is free
        bool operator[](unsigned int blockIndex) const;

        // The tail of the last block stored is unspecified
        void flushBlockMap(std::fstream& device) const;

        void clear();
        void add(char block);

        BlockMap(std::vector<char> map);
};


class FileSystem {
    private:
        std::unique_ptr<std::fstream> m_Device;
        std::string m_DeviceName;

        BlockMap m_BlockMap;

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
