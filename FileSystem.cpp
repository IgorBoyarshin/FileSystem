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
