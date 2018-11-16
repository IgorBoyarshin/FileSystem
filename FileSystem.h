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
#include <bitset>
#include <algorithm>

#include "Device.h"
#include "Block.h"


/* struct DirectoryEntry { */
/*     std::string fileName; */
/*     DeviceFileDescriptor fd; */
/*  */
/*     DirectoryEntry(const std::string& fileName, const DeviceFileDescriptor& fd); */
/* }; */
/*  */
/* class Directory { */
/*     std::vector<DirectoryEntry> entries; */
/* }; */


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
    Mkdir,
    Rmdir,
    Cd,
    Pwd,
    Symlink,
    INVALID
};

std::string toString(Command command);
Command toCommand(const std::string& str);


class FileSystem {
    private:
        std::unique_ptr<Device> m_Device;
        std::string m_DeviceName;
        DeviceHeader m_DeviceHeader;

        DeviceBlockMap m_DeviceBlockMap;

        inline constexpr static unsigned int MAX_OPEN_FILES = 4;
        std::array<std::optional<uint16_t>, MAX_OPEN_FILES> m_OpenFiles;

        uint16_t m_WorkingDirectory; // descriptor index

    public:
        bool process(Command command, std::vector<std::string>& arguments);

        void createEmptyDevice(const std::string& name);

        FileSystem();

        std::optional<uint16_t> getDirByPath(std::string path) const;
        static std::string extractName(std::string path);

        std::optional<uint16_t> getFdOfFileWithName(
                const DeviceFileDescriptor& dir,
                const std::string& name) const;

        bool create(uint16_t dirIndex, DeviceFileDescriptor& dir,
                std::string name, uint16_t fdIndex);

        std::string resolveSymlink(const DeviceFileDescriptor& fd) const;

        bool remove(const DeviceFileDescriptor& fd, uint16_t fdIndex);

    private:
        bool mount(const std::string& deviceName);
        bool umount();
        bool filestat(unsigned int id);
        bool ls();
        bool create(std::string path);
        bool open(const std::string& path, unsigned int& fd_out);
        bool close(unsigned int fd);
        bool read(unsigned int fd, unsigned int shift, unsigned int size, std::string& buff);
        bool write(unsigned int fd, unsigned int shift, const std::string& buff);
        bool link(const std::string& name1, const std::string& name2);
        bool unlink(const std::string& name);
        bool truncate(const std::string& name, unsigned int size);
        bool mkdir(const std::string& name);
        bool rmdir(const std::string& name);
        bool cd(const std::string& name);
        bool pwd();
        bool symlink(std::string target, const std::string& linkName);
};


#endif
