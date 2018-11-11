#include "FileSystem.h"


Block::Block() {}
Block::Block(std::array<char, SIZE> bytes) : bytes(bytes) {}
Block::Block(char bytes[SIZE]) {
    for (unsigned int i = 0; i < SIZE; i++) this->bytes[i] = bytes[i];
}


void writeBlock(std::fstream& device, unsigned int index, const Block& block) {
    device.seekg(Block::SIZE * index);
    device.write(block.asArray(), Block::SIZE);
}

void writeBlocks(std::fstream& device, unsigned int shift, const std::vector<Block>& blocks) {
    device.seekg(Block::SIZE * shift);
    device.write(reinterpret_cast<const char*>(blocks.data()), Block::SIZE * blocks.size());
}

Block readBlock(std::fstream& device, unsigned int index) {
    char bytes[Block::SIZE];
    device.seekg(Block::SIZE * index);
    device.read(bytes, Block::SIZE);

    return Block{bytes};
}

std::vector<Block> readBlocks(std::fstream& device, unsigned int shift, unsigned int amount) {
    char bytes[Block::SIZE * amount];
    device.seekg(Block::SIZE * shift);
    device.read(bytes, Block::SIZE * amount);

    std::vector<Block> blocks;
    for (unsigned int i = 0; i < amount; i++) {
        blocks.emplace_back(bytes + i * Block::SIZE);
    }

    return blocks;
}


BlockMap::BlockMap(std::vector<char> map) : m_BlockAvailabilityMap(map) {}

bool BlockMap::operator[](unsigned int blockIndex) const {
    if (blockIndex >= m_BlockAvailabilityMap.size())
        throw std::out_of_range("blockIndex >= map size");
    const unsigned int byte = blockIndex / sizeof(char);
    const unsigned int shift = blockIndex % sizeof(char);
    return (m_BlockAvailabilityMap[byte] & (1 << shift)) == 1;
}

// The tail of the last block stored is unspecified
void BlockMap::flushBlockMap(std::fstream& device) const {
    std::vector<Block> blocks;
    std::array<char, Block::SIZE> blockContent;
    for (unsigned int i = 0; i < m_BlockAvailabilityMap.size(); i++) {
        if (i % 8 == 0 && i != 0) blocks.emplace_back(blockContent);
        blockContent[i % Block::SIZE] = m_BlockAvailabilityMap[i];
    }

    writeBlocks(device, 0, blocks);
}

void BlockMap::clear() {
    m_BlockAvailabilityMap.clear();
}

void BlockMap::add(char block) {
    m_BlockAvailabilityMap.push_back(block);
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
    if (actualDeviceSize <= Block::SIZE) {
        std::cout << "Invalid header found. Cannot mount" << std::endl;
        m_Device.release();
        return false;
    }

    const unsigned int blocks = actualDeviceSize / Block::SIZE; // floored
    const unsigned int blockMapSizeBytes = std::ceil(1.0 * blocks / sizeof(char));
    const unsigned int blockMapSizeBlocks = std::ceil(1.0 * blockMapSizeBytes / Block::SIZE);

    const std::vector<Block> map = readBlocks(*m_Device, 0, blockMapSizeBlocks);
    unsigned int blocksCount = 0;
    m_BlockMap.clear();
    for (const Block& block : map) {
        for (unsigned int i = 0; i < Block::SIZE; i++) {
            if (blocksCount++ == blocks) break; // assumes it will also break from outter loop
            m_BlockMap.add(block[i]);
        }
    }

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


/* void FileSystem::writeDefaultHeader() { */
/*     if (!m_Device) { */
/*         std::cout << "Cannot write default header: no mounted device" << std::endl; */
/*         return; */
/*     } */
/*  */
/*     static const unsigned int defaultBlockSize = 8; */
/*     static const unsigned int defaultMaxFiles = 10; */
/*  */
/*     DevHeader emptyHeader {defaultBlockSize, defaultMaxFiles}; */
/*     m_Device->seekg(0); */
/*     m_Device->write(reinterpret_cast<char*>(&emptyHeader), sizeof(DevHeader)); */
/* } */


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
    return "<invalid>";
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


std::ostream& operator<<(std::ostream& stream, const DevHeader& devHeader) {
    stream << "Block size = " << devHeader.blockSize << std::endl;
    stream << "Max files = " << devHeader.maxFiles << std::endl;
    return stream;
}
