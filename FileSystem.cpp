#include "FileSystem.h"


FileSystem::FileSystem()
        : m_DeviceBlockMap(0),
         m_WorkingDirectory(0) {
    for (unsigned int i = 0; i < MAX_OPEN_FILES; i++) {
        m_OpenFiles[i] = std::nullopt;
    }
}

void FileSystem::createEmptyDevice(const std::string& name) {
    Device::createEmpty(name);
}

uint16_t FileSystem::getDirByPath(const std::string& path) const {
    // TODO: parse path
    return m_WorkingDirectory;
}

std::string FileSystem::extractName(std::string path) {
    const auto lastSlash = path.find_last_of('/');
    if (lastSlash >= path.size()) return path;
    return path.substr(lastSlash + 1);
}


bool FileSystem::mount(const std::string& deviceName) {
    if (m_Device) {
        std::cout << "Device " << m_DeviceName << " is mounted. Unmount it first" << std::endl;
        return false;
    }

    m_Device = std::make_unique<Device>(deviceName);
    if (!m_Device->is_open()) {
        std::cout << "Could not open device " << deviceName << std::endl;
        m_Device.release();
        return false;
    }

    m_DeviceName = deviceName;
    const unsigned int actualDeviceSize = m_Device->getSize(); // because was openned at the end
    if (actualDeviceSize < sizeof(DeviceHeader)) {
        std::cout << "Invalid header found. Cannot mount" << std::endl;
        m_Device.release();
        return false;
    }
    std::cout << "Processing header..." << std::endl;

    m_DeviceHeader = {m_Device->readBlock(0)};
    Device::BLOCK_SIZE = m_DeviceHeader.blockSize;
    Device::FD_BLOCKS_PER_FILE = m_DeviceHeader.blocksPerFile;

    const unsigned int blocksTotal = actualDeviceSize / Device::BLOCK_SIZE; // floored
    const unsigned int blocksForHeader = ceil(sizeof(DeviceHeader), Device::BLOCK_SIZE);
    const unsigned int blocksForFileDescriptors =
        m_DeviceHeader.maxFiles * DeviceFileDescriptor::sizeInBlocks();
    const unsigned int blocksForMap = [](
            unsigned int blockSize, unsigned int total,
            unsigned int header, unsigned int fds) {
        const unsigned int blockCovers = blockSize * 8;
        unsigned int mapBlocks = 0;
        int toCover = total - header - fds;
        while (toCover > 0) {
            mapBlocks++;
            toCover -= (blockCovers + 1); // +1 for extra block dedicated to map
        }
        return mapBlocks;
    }(m_DeviceHeader.blockSize, blocksTotal, blocksForHeader, blocksForFileDescriptors);
    const unsigned int blocksLeft =
        blocksTotal - blocksForHeader - blocksForFileDescriptors - blocksForMap;

    const unsigned int mapShift = ceil(sizeof(DeviceHeader), Device::BLOCK_SIZE);
    const std::vector<Block> map = m_Device->readBlocks(mapShift, blocksForMap);
    unsigned int addedCount = 0;
    m_DeviceBlockMap = {blocksLeft};
    m_DeviceBlockMap.clear();
    for (const Block& block : map) {
        assert(addedCount <= blocksLeft);
        for (unsigned int i = 0; i < Device::BLOCK_SIZE; i++) {
            m_DeviceBlockMap.add(block[i]);
            addedCount += 8;
            if (addedCount >= blocksLeft) break; // assumes it won't continue the outter loop
        }
    }

    Device::MAP_START = 0 + blocksForHeader;
    Device::FDS_START = Device::MAP_START + blocksForMap;
    Device::DATA_START = Device::FDS_START + blocksForFileDescriptors;

    std::cout << "Block size=" << m_DeviceHeader.blockSize << std::endl;
    std::cout << "Max files=" << m_DeviceHeader.maxFiles << std::endl;
    std::cout << "Max data blocks per file=" << m_DeviceHeader.blocksPerFile << std::endl;
    std::cout << "Blocks total=" << blocksTotal << std::endl;
    std::cout << "Blocks for header=" << blocksForHeader << std::endl;
    std::cout << "Blocks for map=" << blocksForMap << std::endl;
    std::cout << "Blocks for file descriptors=" << blocksForFileDescriptors
        << "(" << DeviceFileDescriptor::sizeInBlocks() << " per FD)" << std::endl;
    std::cout << "Blocks left for data=" << blocksLeft << std::endl;

    return true;
}

bool FileSystem::umount() {
    if (!m_Device) {
        std::cout << "No device currently mounted" << std::endl;
        return false;
    }

    std::cout << "Successfully unmounted device " << m_DeviceName << std::endl;

    m_Device.release();
    return true;
}

bool FileSystem::filestat(unsigned int id) {
    if (!m_Device) {
        std::cout << "No device currently mounted" << std::endl;
        return false;
    }

    std::cout << "File descriptor " << id << ":" << std::endl;
    DeviceFileDescriptor dfd = DeviceFileDescriptor::read(*m_Device, id);
    std::cout << dfd;
    if (dfd.size == 0) {
        if (dfd.fileType == DeviceFileType::Directory) {
            std::cout << "No files" << std::endl;
        } else {
            std::cout << "No Data" << std::endl;
        }
    }

    return true;
}

bool FileSystem::ls() {
    if (!m_Device) {
        std::cout << "No device currently mounted" << std::endl;
        return false;
    }

    DeviceFileDescriptor dir = DeviceFileDescriptor::read(*m_Device, m_WorkingDirectory);
    assert(dir.fileType == DeviceFileType::Directory);
    for (unsigned int fileIndex = 0; fileIndex < dir.size; fileIndex++) {
        const uint16_t nameBlock = dir.blocks[fileIndex * 2 + 0];
        const uint16_t fd = dir.blocks[fileIndex * 2 + 1];
        const std::string name = m_Device->readBlock(Device::DATA_START + nameBlock).asString();

        std::cout << "-- " << name << " : fd=" << fd << std::endl;
    }

    return true;
}

bool FileSystem::create(std::string path) {
    if (!m_Device) {
        std::cout << "No device currently mounted" << std::endl;
        return false;
    }

    DeviceFileDescriptor dir = DeviceFileDescriptor::read(*m_Device, getDirByPath(path));
    assert(dir.fileType == DeviceFileType::Directory);
    const unsigned int lastIndex = 2 * dir.size;
    if (lastIndex >= Device::FD_BLOCKS_PER_FILE) {
        std::cout << "Maximum number of files for this dir reached" << std::endl;
        return false;
    }

    const auto blockForFileNameOpt = m_DeviceBlockMap.findFree();
    if (!blockForFileNameOpt) {
        std::cout << "No empty data blocks left, cannot create a new file" << std::endl;
        return false;
    }
    const unsigned int fdForFileName = std::distance(dir.blocks.begin(),
            std::find(dir.blocks.begin(), dir.blocks.end(),
                        DeviceFileDescriptor::FREE_BLOCK));
    const unsigned int fdForFileFd = fdForFileName + 1;
    const auto emptyFdOpt = DeviceFileDescriptor::findFree(*m_Device);
    if (!emptyFdOpt) {
        std::cout << "No empty FD left, cannot create a new file" << std::endl;
        return false;
    }

    // Create file
    // Write file name
    std::string name = extractName(path);
    if (name.size() > 8) {
        std::cout << "File name is too long, will be trimmed" << std::endl;
        name.erase(name.begin() + 8, name.end());
    }
    m_Device->writeBlock(Device::DATA_START + *blockForFileNameOpt, {name});
    m_DeviceBlockMap.setTaken(*blockForFileNameOpt);
    m_DeviceBlockMap.write(*m_Device);
    // Write file FD
    DeviceFileDescriptor fileFd(DeviceFileType::Regular, 0, 0,
            std::vector<uint16_t>(Device::FD_BLOCKS_PER_FILE, DeviceFileDescriptor::FREE_BLOCK));
    DeviceFileDescriptor::write(*m_Device, *emptyFdOpt, fileFd);

    std::cout << "Created new file with FD=" << *emptyFdOpt << std::endl;

    // Put entry into working dir
    dir.blocks[fdForFileName] = *blockForFileNameOpt; // where name is stored
    dir.blocks[fdForFileFd] = *emptyFdOpt; // fd of file
    dir.size++;
    DeviceFileDescriptor::write(*m_Device, m_WorkingDirectory, dir);

    return true;
}

bool FileSystem::open(const std::string& path, unsigned int& fd_out) {
    if (!m_Device) {
        std::cout << "No device currently mounted" << std::endl;
        return false;
    }
    const unsigned int freeOsFd = std::distance(m_OpenFiles.begin(),
            std::find_if(m_OpenFiles.begin(), m_OpenFiles.end(),
                [](const std::optional<uint16_t>& o){ return !o; }));
    if (freeOsFd >= m_OpenFiles.size()) {
        std::cout << "Cannot open file: max limit for OS file descriptors reached. "
            << "Close some files first" << std::endl;
        return false;
    }

    DeviceFileDescriptor dir = DeviceFileDescriptor::read(*m_Device, getDirByPath(path));
    const std::string name = extractName(path);

    const auto fileFdOpt = [&dir](Device& device, const std::string& name) -> std::optional<uint16_t> {
        for (unsigned int i = 0; i < dir.blocks.size(); i += 2) {
            const uint16_t fileNamePtr = dir.blocks[i];
            if (fileNamePtr == DeviceFileDescriptor::FREE_BLOCK) continue;
            const std::string currName = device.readBlock(Device::DATA_START + fileNamePtr).asString();
            if (currName == name) return {dir.blocks[i + 1]};
        }
        return std::nullopt;
    }(*m_Device, name);
    if (!fileFdOpt) {
        std::cout << "No file with this name exists" << std::endl;
        return false;
    }
    if (const auto i = std::find_if(m_OpenFiles.begin(), m_OpenFiles.end(),
            [fileFd = *fileFdOpt](const std::optional<uint16_t>& o){ return *o == fileFd; });
                i != m_OpenFiles.end()) {
        std::cout << "This file is already open with os_fd="
            << std::distance(m_OpenFiles.begin(), i) << std::endl;
        return false;
    }
    /* std::cout << "File descriptor of this file is " << *fileFdOpt << std::endl; */
    m_OpenFiles[freeOsFd] = {*fileFdOpt};
    fd_out = freeOsFd;

    return true;
}

bool FileSystem::close(unsigned int fd) {
    return true;
}

bool FileSystem::read(unsigned int fd, unsigned int shift, unsigned int size, std::string& buff) {
    /* if (!m_Device) { */
    /*     std::cout << "No device currently mounted" << std::endl; */
    /*     return false; */
    /* } */
    /*  */
    /* std::cout << "Data:\""; */
    /* unsigned int readSize = 0; */
    /* for (uint16_t addr : dfd.blocks) { */
    /*     if (addr == Block::INVALID) break; */
    /*     Block data = m_Device->readBlock(Device::DATA_START + addr); */
    /*     for (unsigned int i = 0; i < Device::BLOCK_SIZE; i++) { */
    /*         if (readSize >= dfd.size) break; */
    /*         std::cout << data[i]; */
    /*         readSize++; */
    /*     } */
    /* } */
    /* std::cout << "\"" << std::endl; */

    return true;
}

bool FileSystem::write(unsigned int fd, unsigned int shift, unsigned int size, const std::string& buff) {
    return true;
}

bool FileSystem::link(const std::string& name1, const std::string& name2) {
    return true;
}

bool FileSystem::unlink(const std::string& name) {
    return true;
}

bool FileSystem::truncate(const std::string& name, unsigned int size) {
    return true;
}


std::string toString(Command command) {
    switch (command) {
        case Command::Mount: return "mount";
        case Command::Umount: return "umount";
        case Command::Filestat: return "filestat";
        case Command::Ls: return "ls";
        case Command::Create: return "create";
        case Command::Open: return "open";
        case Command::Close: return "close";
        case Command::Read: return "read";
        case Command::Write: return "write";
        case Command::Link: return "link";
        case Command::Unlink: return "unlink";
        case Command::Truncate: return "truncate";
        case Command::INVALID: return "<invalid>";
    }
    return "<undefined>";
}


Command toCommand(const std::string& str) {
    if (str == "mount") return Command::Mount;
    else if (str == "umount") return Command::Umount;
    else if (str == "filestat") return Command::Filestat;
    else if (str == "ls") return Command::Ls;
    else if (str == "create") return Command::Create;
    else if (str == "open") return Command::Open;
    else if (str == "close") return Command::Close;
    else if (str == "read") return Command::Read;
    else if (str == "write") return Command::Write;
    else if (str == "link") return Command::Link;
    else if (str == "unlink") return Command::Unlink;
    else if (str == "truncate") return Command::Truncate;

    return Command::INVALID;
}

bool FileSystem::process(Command command, std::vector<std::string>& arguments) {
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
                if (result)
                    std::cout << "Opened file " << arguments[0]
                        << " with os_fd=" << fd << std::endl;
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
