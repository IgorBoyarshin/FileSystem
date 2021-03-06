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

std::pair<std::optional<uint16_t>, std::string>
        FileSystem::extractPath(std::string path) const {
    const bool absolutePath = path[0] == '/';
    uint16_t currDirIndex = (absolutePath) ? 0 : m_WorkingDirectory;
    DeviceFileDescriptor currDir = DeviceFileDescriptor::read(*m_Device, currDirIndex);
    if (absolutePath) path.erase(0, 1); // remove '/'

    static const unsigned int MAX_SUBSEQUENT_RESOLUTIONS = 4;
    unsigned int subsequentSymlinkResolutionCount = 0;
    while (true) {
    /* while (path.find_first_of('/') != std::string::npos || enterLast) { */
        const unsigned int sepIndex = path.find_first_of('/');
        const std::string part = path.substr(0, sepIndex); // coult be till the end
        std::cout << "part=" << part << std::endl;
        path.erase(0, sepIndex); // leave '/' for now
        const auto fdIndexOpt = getFdOfFileWithName(currDir, part);
        const bool atLast = path.find_first_of('/') == std::string::npos;
        if (!fdIndexOpt) {
            if (atLast) {
                return {{currDirIndex}, part};
            } else {
                std::cout << "Invalid path entry: " << part << std::endl;
                return {std::nullopt, ""};
            }
        }
        const DeviceFileDescriptor fd = DeviceFileDescriptor::read(*m_Device, *fdIndexOpt);
        if (fd.fileType == DeviceFileType::Symlink) {
            if (subsequentSymlinkResolutionCount++ > MAX_SUBSEQUENT_RESOLUTIONS) {
                std::cout << "Reached limit of symlink resolution" << std::endl;
                return {std::nullopt, ""};
            }
            const std::string resolvedName = resolveSymlink(fd);
            path = resolvedName + path;
            continue;
        } else if (fd.fileType == DeviceFileType::Directory) {
            if (atLast) {
                return {{currDirIndex}, part};
            }
            currDirIndex = *fdIndexOpt;
            currDir = fd;
        } else if (fd.fileType == DeviceFileType::Regular) {
            return {{currDirIndex}, part};
        } else {
            assert(false);
        }

        path.erase(0, 1); // remove '/'
    }
}

std::string FileSystem::extractName(std::string path) {
    const auto lastSlash = path.find_last_of('/');
    if (lastSlash >= path.size()) return path;
    return path.substr(lastSlash + 1);
}

std::optional<uint16_t> FileSystem::getFdOfFileWithName(
        const DeviceFileDescriptor& dir,
        const std::string& name) const {
    for (unsigned int i = 0; i < dir.blocks.size(); i += 2) {
        const uint16_t fileNamePtr = dir.blocks[i];
        if (fileNamePtr == DeviceFileDescriptor::FREE_BLOCK) continue;
        const std::string currName = m_Device->readBlock(Device::DATA_START + fileNamePtr).asString();
        if (currName == name) return {dir.blocks[i + 1]};
    }
    return std::nullopt;
}

std::string FileSystem::resolveSymlink(const DeviceFileDescriptor& fd) const {
    assert(fd.fileType == DeviceFileType::Symlink);
    std::string result;
    for (uint16_t addr : fd.blocks) {
        if (addr == DeviceFileDescriptor::FREE_BLOCK) break;
        result += m_Device->readBlock(Device::DATA_START + addr).asString();
    }

    return result;
}

bool FileSystem::remove(const DeviceFileDescriptor& fd, uint16_t fdIndex) {
    for (uint16_t addr : fd.blocks) {
        if (addr != DeviceFileDescriptor::FREE_BLOCK) {
            m_DeviceBlockMap.setFree(addr);
        }
    }
    m_DeviceBlockMap.write(*m_Device);

    DeviceFileDescriptor::write(*m_Device, fdIndex, {});

    return true;
}

bool FileSystem::create(uint16_t dirIndex, DeviceFileDescriptor& dir,
                std::string name, uint16_t fdIndex) {
    assert(dir.fileType == DeviceFileType::Directory);
    if (const unsigned int lastIndex = 2 * dir.size;
            lastIndex >= Device::FD_BLOCKS_PER_FILE) {
        std::cout << "Maximum number of files for this dir reached" << std::endl;
        return false;
    }
    const auto blockIndexForFileNameOpt = m_DeviceBlockMap.findFree();
    if (!blockIndexForFileNameOpt) {
        std::cout << "No empty data blocks left (to store file name), "
            << "cannot create a new file" << std::endl;
        return false;
    }
    if (name.size() > 8) {
        std::cout << "File name is too long, will be trimmed" << std::endl;
        name.erase(name.begin() + 8, name.end());
    }

    // Indices inside FD for file name and file descriptor index
    const unsigned int fdIndexForFileName = std::distance(dir.blocks.begin(),
            std::find(dir.blocks.begin(), dir.blocks.end(),
                        DeviceFileDescriptor::FREE_BLOCK));
    const unsigned int fdIndexForFileFd = fdIndexForFileName + 1;

    // Store file name data block
    m_Device->writeBlock(Device::DATA_START + *blockIndexForFileNameOpt, {name});
    m_DeviceBlockMap.setTaken(*blockIndexForFileNameOpt);
    m_DeviceBlockMap.write(*m_Device);

    // Put entry into working dir
    dir.blocks[fdIndexForFileName] = *blockIndexForFileNameOpt; // where name is stored
    dir.blocks[fdIndexForFileFd] = fdIndex; // fd of file
    dir.size++;
    DeviceFileDescriptor::write(*m_Device, dirIndex, dir);

    return true;
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
    for (unsigned int fileIndex = 0; fileIndex < Device::FD_BLOCKS_PER_FILE / 2; fileIndex++) {
        const uint16_t nameBlock = dir.blocks[fileIndex * 2 + 0];
        const uint16_t fd = dir.blocks[fileIndex * 2 + 1];
        if (nameBlock == DeviceFileDescriptor::FREE_BLOCK) {
            assert(fd == DeviceFileDescriptor::FREE_BLOCK);
            continue;
        }
        const std::string name = m_Device->readBlock(Device::DATA_START + nameBlock).asString();

        std::cout << "-- " << name << " : fd=" << fd << std::endl;
    }

    m_DeviceBlockMap.printState();

    return true;
}

bool FileSystem::create(std::string path) {
    if (!m_Device) {
        std::cout << "No device currently mounted" << std::endl;
        return false;
    }

    // Find dir
    const auto dir_fdName = extractPath(path);
    if (!dir_fdName.first) {
        std::cout << "Invalid path, cannot create file" << std::endl;
        return false;
    }
    DeviceFileDescriptor dir = DeviceFileDescriptor::read(*m_Device, *dir_fdName.first);
    const std::string name = dir_fdName.second;

    // Find FD for future file
    const auto freeFdOpt = DeviceFileDescriptor::findFree(*m_Device);
    if (!freeFdOpt) {
        std::cout << "No empty FD left, cannot create a new file" << std::endl;
        return false;
    }

    // Create file inside found FD
    /* const std::string name = extractName(path); */
    DeviceFileDescriptor fd(DeviceFileType::Regular, 0, 1,
            std::vector<uint16_t>(Device::FD_BLOCKS_PER_FILE, DeviceFileDescriptor::FREE_BLOCK));
    DeviceFileDescriptor::write(*m_Device, *freeFdOpt, fd);

    const bool result = create(*dir_fdName.first, dir, name, *freeFdOpt);
    if (!result) return false;
    std::cout << "Created new file with FD=" << *freeFdOpt << std::endl;

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

    const auto dir_fdName = extractPath(path);
    if (!dir_fdName.first) {
        std::cout << "Invalid path, cannot create file" << std::endl;
        return false;
    }
    DeviceFileDescriptor dir = DeviceFileDescriptor::read(*m_Device, *dir_fdName.first);
    const std::string name = dir_fdName.second;

    const auto fileFdOpt = getFdOfFileWithName(dir, name);
    if (!fileFdOpt) {
        std::cout << "No file with this name exists" << std::endl;
        return false;
    }
    if (const auto i = std::find_if(m_OpenFiles.begin(), m_OpenFiles.end(),
            [fileFd = *fileFdOpt](const std::optional<uint16_t>& o){ return o && *o == fileFd; });
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
    if (!m_Device) {
        std::cout << "No device currently mounted" << std::endl;
        return false;
    }

    if (!m_OpenFiles[fd]) {
        std::cout << "No file with os_fd=" << fd << " currently openned" << std::endl;
        return false;
    }
    m_OpenFiles[fd] = std::nullopt;
    std::cout << "Closed file with os_fd=" << fd << std::endl;

    return true;
}

bool FileSystem::read(unsigned int fd, unsigned int shift, unsigned int size, std::string& buff) {
    if (!m_Device) {
        std::cout << "No device currently mounted" << std::endl;
        return false;
    }
    if (!m_OpenFiles[fd]) {
        std::cout << "No file with os_fd=" << fd << " currently open" << std::endl;
        return false;
    }

    DeviceFileDescriptor dfd = DeviceFileDescriptor::read(*m_Device, *m_OpenFiles[fd]);
    assert(dfd.fileType == DeviceFileType::Regular || dfd.fileType == DeviceFileType::Symlink);
    const unsigned int farEnd = shift + size;
    if (farEnd > dfd.size) {
        std::cout << "Requested pointer is beyond the file" << std::endl;
        return false;
    }

    buff.clear();
    unsigned int readSize = 0;
    bool finished = false;
    for (uint16_t addr : dfd.blocks) {
        if (addr == Block::INVALID) break;
        const Block data = m_Device->readBlock(Device::DATA_START + addr);
        for (unsigned int i = 0; i < Device::BLOCK_SIZE; i++) {
            if (readSize == farEnd) {
                finished = true;
                break;
            }
            assert(readSize < dfd.size); // can't happen, checked earlier
            if (shift <= readSize)
                buff += data[i];
            readSize++;
        }
        if (finished) break;
    }

    return true;
}

bool FileSystem::write(unsigned int fd, unsigned int shift, const std::string& buff) {
    if (!m_Device) {
        std::cout << "No device currently mounted" << std::endl;
        return false;
    }
    if (!m_OpenFiles[fd]) {
        std::cout << "No file with os_fd=" << fd << " currently open" << std::endl;
        return false;
    }
    if (buff.size() == 0) {
        std::cout << "Write 0 bytes" << std::endl;
        return true;
    }

    DeviceFileDescriptor dfd = DeviceFileDescriptor::read(*m_Device, *m_OpenFiles[fd]);
    assert(dfd.fileType == DeviceFileType::Regular);
    if (shift > dfd.size) {
        std::cout << "Shift is beyond the end of the file" << std::endl;
        return false;
    }

    unsigned int ptr = 0;
    unsigned int buffPtr = 0;
    bool finished = false;
    for (unsigned int blockIndex = 0; blockIndex < dfd.blocks.size(); blockIndex++) {
        unsigned int addr = dfd.blocks[blockIndex];
        if (addr == DeviceFileDescriptor::FREE_BLOCK) {
            const auto freeOpt = m_DeviceBlockMap.findFree();
            if (!freeOpt) {
                std::cout << "No free data blocks left, cannot write" << std::endl;
                return false;
            }
            dfd.blocks[blockIndex] = *freeOpt;
            addr = dfd.blocks[blockIndex];
            m_DeviceBlockMap.setTaken(*freeOpt);
            m_DeviceBlockMap.write(*m_Device);
        }
        if (ptr + Device::BLOCK_SIZE <= shift) {
            ptr += Device::BLOCK_SIZE;
            continue;
        }
        Block data = m_Device->readBlock(Device::DATA_START + addr);
        for (unsigned int i = 0; i < Device::BLOCK_SIZE; i++) {
            if (ptr++ < shift) continue;
            data[ptr - 1] = buff[buffPtr++]; // ptr was inced at the prev line
            if (ptr == shift + buff.size()) {
                finished = true;
                break;
            }
        }
        m_Device->writeBlock(Device::DATA_START + addr, data);
        if (finished) break;
    }

    std::cout << "Wrote " << buff.size() << " bytes" << std::endl;
    dfd.size += (shift + buff.size() > dfd.size) ? (shift + buff.size() - dfd.size) : 0;
    DeviceFileDescriptor::write(*m_Device, *m_OpenFiles[fd], dfd);

    return true;
}

bool FileSystem::link(const std::string& name1, const std::string& name2) {
    if (!m_Device) {
        std::cout << "No device currently mounted" << std::endl;
        return false;
    }

    const auto dir_fdName = extractPath(name1);
    if (!dir_fdName.first) {
        std::cout << "Invalid path, cannot create file" << std::endl;
        return false;
    }
    DeviceFileDescriptor dir = DeviceFileDescriptor::read(*m_Device, *dir_fdName.first);
    const std::string fileName = dir_fdName.second;

    const auto fdIndexOpt = getFdOfFileWithName(dir, fileName);
    if (!fdIndexOpt) {
        std::cout << "No file with name " << name1
            << " exists, cannot create hard link" << std::endl;
        return false;
    }
    auto fd = DeviceFileDescriptor::read(*m_Device, *fdIndexOpt);
    fd.linksCount++;
    DeviceFileDescriptor::write(*m_Device, *fdIndexOpt, fd);

    const bool result = create(*dir_fdName.first, dir, name2, *fdIndexOpt);
    if (!result) return false;
    std::cout << "Creaing a hard link: " << name2 << "=>" << fileName << std::endl;

    return true;
}

bool FileSystem::unlink(const std::string& name) {
    if (!m_Device) {
        std::cout << "No device currently mounted" << std::endl;
        return false;
    }

    const auto dir_fdName = extractPath(name);
    if (!dir_fdName.first) {
        std::cout << "Invalid path, cannot create file" << std::endl;
        return false;
    }
    DeviceFileDescriptor dir = DeviceFileDescriptor::read(*m_Device, *dir_fdName.first);
    const std::string fileName = dir_fdName.second;

    const auto fdIndexOpt = getFdOfFileWithName(dir, fileName);
    if (!fdIndexOpt) {
        std::cout << "No file with name " << name
            << " exists, cannot unlink hard link" << std::endl;
        return false;
    }

    // Remove link file (dir etry) from dir
    for (unsigned int i = 0; i < dir.blocks.size(); i += 2) {
        if (dir.blocks[i + 1] == *fdIndexOpt) {
            const uint16_t fileNameBlockAddr = dir.blocks[i];
            const std::string fileName =
                m_Device->readBlock(Device::DATA_START + fileNameBlockAddr).asString();
            if (fileName != name) {
                // Found another hard link for this FD => keep looking
                continue;
            }
            m_DeviceBlockMap.setFree(fileNameBlockAddr);
            m_DeviceBlockMap.write(*m_Device);

            dir.blocks[i] = DeviceFileDescriptor::FREE_BLOCK;
            dir.blocks[i + 1] = DeviceFileDescriptor::FREE_BLOCK;
            dir.size--;
            DeviceFileDescriptor::write(*m_Device, *dir_fdName.first, dir);
            break;
        }
    }

    std::cout << "Successfully unlinked hard" << std::endl;

    auto fd = DeviceFileDescriptor::read(*m_Device, *fdIndexOpt);
    fd.linksCount--;
    if (fd.linksCount == 0) {
        // Need to remove FD as well
        remove(fd, *fdIndexOpt);
        std::cout << "Hard links count reached 0 => removed the FD as well" << std::endl;
    }
    DeviceFileDescriptor::write(*m_Device, *fdIndexOpt, fd);


    return true;
}

bool FileSystem::mkdir(const std::string& name) {
    if (!m_Device) {
        std::cout << "No device currently mounted" << std::endl;
        return false;
    }

    const auto dir_fdName = extractPath(name);
    if (!dir_fdName.first) {
        std::cout << "Invalid path, cannot create file" << std::endl;
        return false;
    }
    DeviceFileDescriptor parent = DeviceFileDescriptor::read(*m_Device, *dir_fdName.first);
    const std::string fileName = dir_fdName.second;

    // Find FD for future file
    const auto freeFdOpt = DeviceFileDescriptor::findFree(*m_Device);
    if (!freeFdOpt) {
        std::cout << "No empty FD left, cannot create a new directory here" << std::endl;
        return false;
    }

    // Create file inside found FD
    const std::string dirName = extractName(name);
    DeviceFileDescriptor fd(DeviceFileType::Directory, 2, 2,
            std::vector<uint16_t>(Device::FD_BLOCKS_PER_FILE, DeviceFileDescriptor::FREE_BLOCK));

    // Parent link
    const auto link1FileNameAddrOpt = m_DeviceBlockMap.findFree();
    assert(link1FileNameAddrOpt); // TODO: handle no mem left
    m_DeviceBlockMap.setTaken(*link1FileNameAddrOpt);
    m_DeviceBlockMap.write(*m_Device);
    m_Device->writeBlock(Device::DATA_START + *link1FileNameAddrOpt, {".."});
    fd.blocks[0] = *link1FileNameAddrOpt;
    fd.blocks[1] = *dir_fdName.first;

    // Self link
    const auto link2FileNameAddrOpt = m_DeviceBlockMap.findFree();
    assert(link2FileNameAddrOpt); // TODO: handle no mem left
    m_DeviceBlockMap.setTaken(*link2FileNameAddrOpt);
    m_DeviceBlockMap.write(*m_Device);
    m_Device->writeBlock(Device::DATA_START + *link2FileNameAddrOpt, {"."});
    fd.blocks[2] = *link2FileNameAddrOpt;
    fd.blocks[3] = *freeFdOpt;

    DeviceFileDescriptor::write(*m_Device, *freeFdOpt, fd);

    const bool result = create(*dir_fdName.first, parent, dirName, *freeFdOpt);
    if (!result) return false;
    std::cout << "Created new directory with FD=" << *freeFdOpt << std::endl;

    return true;
}

bool FileSystem::rmdir(const std::string& name) {
    if (!m_Device) {
        std::cout << "No device currently mounted" << std::endl;
        return false;
    }

    const auto dir_fdName = extractPath(name);
    if (!dir_fdName.first) {
        std::cout << "Invalid path" << std::endl;
        return false;
    }
    DeviceFileDescriptor parent = DeviceFileDescriptor::read(*m_Device, *dir_fdName.first);
    const std::string fileName = dir_fdName.second;
    auto dirIndexOpt = getFdOfFileWithName(parent, dir_fdName.second);
    if (!dirIndexOpt) {
        std::cout << "No directory with specified name exists" << std::endl;
        return false;
    }
    DeviceFileDescriptor dir = DeviceFileDescriptor::read(*m_Device, *dirIndexOpt);
    if (dir.size > 2) { // more than two mandatory links
        std::cout << "Directory must be empty in order to be able to remove it" << std::endl;
        return false;
    }

    // Remove from parent
    for (unsigned int i = 0; i < Device::FD_BLOCKS_PER_FILE; i += 2) {
        const uint16_t addr = parent.blocks[i];
        if (addr == DeviceFileDescriptor::FREE_BLOCK) continue;
        const std::string childName =
            m_Device->readBlock(Device::DATA_START + addr).asString();
        if (childName == dir_fdName.second) {
            // Free mem for child name
            m_DeviceBlockMap.setFree(addr);
            m_DeviceBlockMap.write(*m_Device);

            parent.blocks[i] = DeviceFileDescriptor::FREE_BLOCK;
            parent.blocks[i + 1] = DeviceFileDescriptor::FREE_BLOCK;
            parent.size--;
            DeviceFileDescriptor::write(*m_Device, *dir_fdName.first, parent);
            break;
        }
    }

    // Clear dir contents (release memory for links)
    remove(dir, *dirIndexOpt);

    std::cout << "Successfully removed dir " << dir_fdName.second << std::endl;

    return true;
}

bool FileSystem::cd(std::string path) {
    if (!m_Device) {
        std::cout << "No device currently mounted" << std::endl;
        return false;
    }

    const auto dir_fdName = extractPath(path);
    if (!dir_fdName.first) {
        std::cout << "Invalid path" << std::endl;
        return false;
    }
    DeviceFileDescriptor parent = DeviceFileDescriptor::read(*m_Device, *dir_fdName.first);
    const std::string name = dir_fdName.second;
    auto childOpt = getFdOfFileWithName(parent, name);
    if (!childOpt) {
        std::cout << "Invalid path entry: " << name << std::endl;
        return false;
    }
    m_WorkingDirectory = *childOpt;
    std::cout << "Changed working directory to " << name
        << " (FD=" << *childOpt << ")" << std::endl;

    return true;
}

bool FileSystem::symlink(std::string target, const std::string& linkName) {
    const std::string targetCopy = target;
    if (!m_Device) {
        std::cout << "No device currently mounted" << std::endl;
        return false;
    }

    // Always inside working dir
    DeviceFileDescriptor dir = DeviceFileDescriptor::read(*m_Device, m_WorkingDirectory);

    // Find FD for future file
    const auto freeFdOpt = DeviceFileDescriptor::findFree(*m_Device);
    if (!freeFdOpt) {
        std::cout << "No empty FD left, cannot create a new file" << std::endl;
        return false;
    }

    // Create file inside found FD
    DeviceFileDescriptor fd(DeviceFileType::Symlink, target.size(), 1,
            std::vector<uint16_t>(Device::FD_BLOCKS_PER_FILE, DeviceFileDescriptor::FREE_BLOCK));
    unsigned int counter = 0;
    while (target.size() > 0) {
        const bool toEnd = target.size() <= Device::BLOCK_SIZE;
        const Block data{target.substr(0, toEnd ? target.size() : Device::BLOCK_SIZE)};
        auto freeIndexOpt = m_DeviceBlockMap.findFree();
        assert(freeIndexOpt); // TODO
        m_Device->writeBlock(Device::DATA_START + *freeIndexOpt, data);
        m_DeviceBlockMap.setTaken(*freeIndexOpt);
        m_DeviceBlockMap.write(*m_Device);
        fd.blocks[counter++] = *freeIndexOpt;
        target.erase(0, Device::BLOCK_SIZE);
    }
    DeviceFileDescriptor::write(*m_Device, *freeFdOpt, fd);

    const bool result = create(m_WorkingDirectory, dir, linkName, *freeFdOpt);
    if (!result) return false;
    std::cout << "Created new symlink " << linkName << " => " << targetCopy << std::endl;

    return true;
}

bool FileSystem::pwd() {
    std::cout << "Working director FD=" << m_WorkingDirectory << std::endl;
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
        case Command::Mkdir: return "mkdir";
        case Command::Rmdir: return "rmdir";
        case Command::Cd: return "cd";
        case Command::Pwd: return "pwd";
        case Command::Symlink: return "symlink";
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
    else if (str == "mkdir") return Command::Mkdir;
    else if (str == "rmdir") return Command::Rmdir;
    else if (str == "cd") return Command::Cd;
    else if (str == "pwd") return Command::Pwd;
    else if (str == "symlink") return Command::Symlink;

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
                if (result)
                    std::cout << "Data:\"" << buff << "\"";
                return result;
            } catch (std::exception& e) {
                std::cout << "Expecting an int argument" << std::endl;
                return false;
            }
        case Command::Write:
            if (arguments.size() != 3) {
                std::cout << "Expecting 3 arguments: file descriptor, shift, string" << std::endl;
                return false;
            }
            try {
                const unsigned int fd = std::stoi(arguments[0]);
                const unsigned int shift = std::stoi(arguments[1]);
                return write(fd, shift, arguments[2]);
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
            std::cout << "Operation not supported for now" << std::endl;
            return true;
            /* if (arguments.size() != 2) { */
            /*     std::cout << "Expecting 2 arguments: file name, size" << std::endl; */
            /*     return false; */
            /* } */
            /* try { */
            /*     const unsigned int size = std::stoi(arguments[1]); */
            /*     return truncate(arguments[0], size); */
            /* } catch (std::exception& e) { */
            /*     std::cout << "Expecting an int argument" << std::endl; */
            /*     return false; */
            /* } */
        case Command::Mkdir:
            if (arguments.size() != 1) {
                std::cout << "Expecting 1 argument: directory name" << std::endl;
                return false;
            }
            return mkdir(arguments[0]);
        case Command::Rmdir:
            if (arguments.size() != 1) {
                std::cout << "Expecting 1 argument: directory name" << std::endl;
                return false;
            }
            return rmdir(arguments[0]);
        case Command::Cd:
            if (arguments.size() != 1) {
                std::cout << "Expecting 1 argument: directory name" << std::endl;
                return false;
            }
            return cd(arguments[0]);
        case Command::Pwd:
            if (arguments.size() != 0) {
                std::cout << "Expecting no arguments" << std::endl;
                return false;
            }
            return pwd();
        case Command::Symlink:
            if (arguments.size() != 2) {
                std::cout << "Expecting 2 arguments: target path, link name" << std::endl;
                return false;
            }
            return symlink(arguments[0], arguments[1]);
        default:
            return false;
    }
}
