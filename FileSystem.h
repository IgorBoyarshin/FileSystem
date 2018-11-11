#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <memory>
#include <cassert>


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


class FileSystem {
    private:
        std::unique_ptr<std::fstream> m_Device;
        std::string m_DeviceName;

    public:
        bool process(Command command, std::vector<std::string>& arguments) {
            switch (command) {
                case Command::Mount:
                    if (arguments.size() != 1) {
                        std::cout << "Expecting 1 argument: device name" << std::endl;
                        return false;
                    }
                    return mount(arguments[0]);
                case Command::Umount:
                    if (arguments.size() != 0) {
                        std::cout << "Expecting no arguments" << std::endl;
                        return false;
                    }
                    return umount();
                case Command::Filestat:
                    if (arguments.size() != 1) {
                        std::cout << "Expecting 1 argument: descriptor id" << std::endl;
                        return false;
                    }
                    try {
                        const unsigned int fd = std::stoi(arguments[0]);
                        return filestat(fd);
                    } catch (std::exception& e) {
                        std::cout << "Expection an int argument" << std::endl;
                        return false;
                    }
                case Command::Ls:
                    if (arguments.size() != 0) {
                        std::cout << "Expecting no arguments" << std::endl;
                        return false;
                    }
                    return ls();
                case Command::Create:
                    if (arguments.size() != 1) {
                        std::cout << "Expecting 1 argument: file name" << std::endl;
                        return false;
                    }
                    return create(arguments[0]);
                case Command::Open:
                    if (arguments.size() != 1) {
                        std::cout << "Expecting 1 argument: file name" << std::endl;
                        return false;
                    }
                    {
                        unsigned int fd;
                        const bool result = open(arguments[0], fd);
                        std::cout << "Opened file " << arguments[0] << " with fd=" << fd << std::endl;
                        return result;
                    }
                case Command::Close:
                    if (arguments.size() != 1) {
                        std::cout << "Expecting 1 argument: file descriptor" << std::endl;
                        return false;
                    }
                    try {
                        return close(std::stoi(arguments[0]));
                    } catch (std::exception& e) {
                        std::cout << "Excepting an int argument" << std::endl;
                        return false;
                    }
                case Command::Read:
                    if (arguments.size() != 3) {
                        std::cout << "Expecting 3 arguments: file descriptor, shift, size" << std::endl;
                        return false;
                    }
                    try {
                        const unsigned int fd = std::stoi(arguments[0]);
                        const unsigned int shift = std::stoi(arguments[1]);
                        const unsigned int size = std::stoi(arguments[2]);
                        std::string buff;
                        const bool result = read(fd, shift, size, buff);
                        std::cout << buff << std::endl;
                        return result;
                    } catch (std::exception& e) {
                        std::cout << "Expecting an int argument" << std::endl;
                        return false;
                    }
                case Command::Write:
                    if (arguments.size() != 4) {
                        std::cout << "Expecting 4 arguments: file descriptor, shift, size, string" << std::endl;
                        return false;
                    }
                    try {
                        const unsigned int fd = std::stoi(arguments[0]);
                        const unsigned int shift = std::stoi(arguments[1]);
                        const unsigned int size = std::stoi(arguments[2]);
                        return write(fd, shift, size, arguments[4]);
                    } catch (std::exception& e) {
                        std::cout << "Expecting an int argument" << std::endl;
                        return false;
                    }
                case Command::Link:
                    if (arguments.size() != 2) {
                        std::cout << "Expecting 2 arguments: target name, link name" << std::endl;
                        return false;
                    }
                    return link(arguments[0], arguments[1]);
                case Command::Unlink:
                    if (arguments.size() != 1) {
                        std::cout << "Expecting 1 argument: link name" << std::endl;
                        return false;
                    }
                    return unlink(arguments[0]);
                case Command::Truncate:
                    if (arguments.size() != 2) {
                        std::cout << "Expecting 2 arguments: file name, size" << std::endl;
                        return false;
                    }
                    try {
                        const unsigned int size = std::stoi(arguments[1]);
                        return truncate(arguments[0], size);
                    } catch (std::exception& e) {
                        std::cout << "Expecting an int argument" << std::endl;
                        return false;
                    }
                default:
                    return false;
            }
        }

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
