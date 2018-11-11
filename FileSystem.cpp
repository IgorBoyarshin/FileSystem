#include "FileSystem.h"


uint16_t Block::SIZE = 8;
const uint16_t Block::INVALID = UINT16_MAX;

Block::Block() : bytes(SIZE) {}
Block::Block(std::vector<uint8_t> bytes) : bytes(bytes) {
    assert(bytes.size() == SIZE);
}
Block::Block(uint8_t* bytes) {
    for (unsigned int i = 0; i < SIZE; i++) this->bytes.push_back(bytes[i]);
}


void writeBlock(std::fstream& device, unsigned int index, const Block& block) {
    device.seekg(Block::SIZE * index);
    device.write(reinterpret_cast<const char*>(block.asArray()), Block::SIZE);
}

void writeBlocks(std::fstream& device, unsigned int shift, const std::vector<Block>& blocks) {
    device.seekg(Block::SIZE * shift);
    device.write(reinterpret_cast<const char*>(blocks.data()), Block::SIZE * blocks.size());
}

Block readBlock(std::fstream& device, unsigned int index) {
    uint8_t bytes[Block::SIZE];
    device.seekg(Block::SIZE * index);
    device.read(reinterpret_cast<char*>(bytes), Block::SIZE);

    return Block{bytes};
}

std::vector<Block> readBlocks(std::fstream& device, unsigned int shift, unsigned int amount) {
    uint8_t bytes[Block::SIZE * amount];
    device.seekg(Block::SIZE * shift);
    device.read(reinterpret_cast<char*>(bytes), Block::SIZE * amount);

    std::vector<Block> blocks;
    for (unsigned int i = 0; i < amount; i++) {
        blocks.emplace_back(bytes + i * Block::SIZE);
    }

    return blocks;
}


DeviceBlockMap::DeviceBlockMap() {}
DeviceBlockMap::DeviceBlockMap(std::vector<uint8_t> map) : m_BlocksUsageMap(map) {}

bool DeviceBlockMap::operator[](unsigned int blockIndex) const {
    if (blockIndex >= m_BlocksUsageMap.size())
        throw std::out_of_range("blockIndex >= map size");
    const unsigned int byte = blockIndex / sizeof(uint8_t);
    const unsigned int shift = blockIndex % sizeof(uint8_t);
    return (m_BlocksUsageMap[byte] & (1 << shift)) == 0;
}

// The tail of the last stored block is unspecified
void DeviceBlockMap::flush(std::fstream& device) const {
    std::vector<Block> blocks;
    std::vector<uint8_t> blockContent(Block::SIZE, 0);
    for (unsigned int i = 0; i < m_BlocksUsageMap.size(); i++) {
        const unsigned int shift = i % Block::SIZE;
        blockContent[shift] = (m_BlocksUsageMap[i]);
        if ((shift == 0 && i != 0) || (i == m_BlocksUsageMap.size() - 1)) {
            blocks.emplace_back(blockContent);
            blockContent = std::vector<uint8_t>(Block::SIZE, 0);
        }
    }

    writeBlocks(device, ceil(sizeof(DeviceHeader), Block::SIZE), blocks);
}

void DeviceBlockMap::clear() {
    m_BlocksUsageMap.clear();
}

void DeviceBlockMap::add(uint8_t byte) {
    m_BlocksUsageMap.push_back(byte);
}


// Dummy value. Proper one is set at mount
unsigned int DeviceFileDescriptor::BLOCKS_PER_FILE = 8;

DeviceFileDescriptor::DeviceFileDescriptor()
    : fileType(DeviceFileType::Empty), size(0), linksCount(0), blocks(std::vector<uint16_t>(BLOCKS_PER_FILE, 0)) {
    blocks = std::vector<uint16_t>(BLOCKS_PER_FILE, Block::INVALID);
}
DeviceFileDescriptor::DeviceFileDescriptor(DeviceFileType fileType, uint16_t size,
        uint8_t linksCount, std::vector<uint16_t> blocks)
        : fileType(fileType), size(size), linksCount(linksCount), blocks(blocks) {}






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


bool FileSystem::mount(const std::string& deviceName) {
    if (m_Device) {
        std::cout << "Device " << m_DeviceName << " is mounted. Unmount it first" << std::endl;
        return false;
    }

    m_Device = std::make_unique<std::fstream>(
            deviceName, m_Device->binary | m_Device->in | m_Device->out | m_Device->ate);
    if (!m_Device->is_open()) {
        std::cout << "Could not open device " << deviceName << std::endl;
        m_Device.release();
        return false;
    }

    m_DeviceName = deviceName;

    const unsigned int actualDeviceSize = m_Device->tellg(); // because was openned at the end
    if (actualDeviceSize < sizeof(DeviceHeader)) {
        std::cout << "Invalid header found. Cannot mount" << std::endl;
        m_Device.release();
        return false;
    }
    std::cout << "Processing header..." << std::endl;

    // The only direct (byte-wise, as opposed to block-wise) interaction with device
    m_Device->seekg(0);
    m_Device->read(reinterpret_cast<char*>(&m_DeviceHeader), sizeof(DeviceHeader));
    Block::SIZE = m_DeviceHeader.blockSize;
    DeviceFileDescriptor::BLOCKS_PER_FILE = m_DeviceHeader.blocksPerFile;

    const unsigned int blocksTotal = actualDeviceSize / Block::SIZE; // floored
    const unsigned int blocksForHeader = ceil(sizeof(DeviceHeader), Block::SIZE);
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

    const unsigned int mapShift = ceil(sizeof(DeviceHeader), Block::SIZE);
    const std::vector<Block> map = readBlocks(*m_Device, mapShift, blocksForMap);
    unsigned int addedBlocksCount = 0;
    m_DeviceBlockMap.clear();
    for (const Block& block : map) {
        assert(addedBlocksCount <= blocksLeft);
        for (unsigned int i = 0; i < Block::SIZE; i++) {
            if (addedBlocksCount++ == blocksLeft) break; // assumes it won't continue the outter loop
            m_DeviceBlockMap.add(block[i]);
        }
    }

    std::cout << "Block size=" << m_DeviceHeader.blockSize << std::endl;
    std::cout << "Max files=" << m_DeviceHeader.maxFiles << std::endl;
    std::cout << "Blocks per file=" << m_DeviceHeader.blocksPerFile << std::endl;
    std::cout << "Blocks for header=" << blocksForHeader << std::endl;
    std::cout << "Blocks for file descriptors=" << blocksForFileDescriptors
        << "(" << DeviceFileDescriptor::sizeInBlocks() << " per FD)" << std::endl;
    std::cout << "Blocks for map=" << blocksForMap << std::endl;
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

bool FileSystem::filestat(unsigned int fd) {
    return true;
}

bool FileSystem::ls() {
    return true;
}

bool FileSystem::create(const std::string& name) {
    return true;
}

bool FileSystem::open(const std::string& name, unsigned int& fd) {
    return true;
}

bool FileSystem::close(unsigned int fd) {
    return true;
}

bool FileSystem::read(unsigned int fd, unsigned int shift, unsigned int size, std::string& buff) {
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


void FileSystem::createEmptyDevice() {
    std::fstream file("defdevice", file.binary | file.out);
    if (!file.is_open()) {
        std::cout << "Failed to create empty device" << std::endl;
        return;
    }

    DeviceHeader header;
    header.blockSize = 8;
    header.maxFiles = 12;
    header.blocksPerFile = 10;
    Block::SIZE = header.blockSize;
    DeviceFileDescriptor::BLOCKS_PER_FILE = header.blocksPerFile;

    DeviceBlockMap map;
    map.m_BlocksUsageMap = {2, 0, 0, 0};

    std::vector<DeviceFileDescriptor> fds;
    for (unsigned int i = 0; i < header.maxFiles; i++) {
        if (i == 0) {
            fds.emplace_back(DeviceFileType::Regular, 0, 0,
                    std::vector<uint16_t>(header.blocksPerFile, 0));
        } else if (i == 1) {
            fds.emplace_back(DeviceFileType::Regular, 6, 0,
                    std::vector<uint16_t>{2, 0, 0, 0, 0, 0, 0, 0, 0, 0});
        } else {
            fds.push_back({});
        }
    }

    header.firstLogicalBlockShift = 1 + 1 + header.maxFiles * 3;

    std::vector<char> data;
    const unsigned int dataCapacity = map.m_BlocksUsageMap.size() * sizeof(uint8_t) * 8 * header.blockSize;
    for (unsigned int i = 0; i < dataCapacity; i++) {
        data.push_back(0);
    }
    data[2 * header.blockSize + 0] = 65;
    data[2 * header.blockSize + 1] = 66;
    data[2 * header.blockSize + 2] = 67;

    file.write(reinterpret_cast<char*>(&header), 8);
    map.flush(file);
    for (unsigned int i = 0; i < fds.size(); i++) {
        file.write(reinterpret_cast<char*>(&fds[i]), 4); // head
        file.write(reinterpret_cast<char*>(fds[i].blocks.data()), 2 * fds[i].blocks.size());
    }
    file.write(data.data(), data.size());
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
