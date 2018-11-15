#include "Device.h"


uint16_t Device::BLOCK_SIZE = 8;
uint16_t Device::FD_BLOCKS_PER_FILE = 10;
uint16_t Device::MAP_START = 1;
uint16_t Device::FDS_START = 1;
uint16_t Device::DATA_START = 1;


void Device::writeBlock(std::fstream& file, unsigned int index, const Block& block) {
    file.seekg(BLOCK_SIZE * index);
    file.write(reinterpret_cast<const char*>(block.asArray()), BLOCK_SIZE);
}

void Device::writeBlocks(std::fstream& file, unsigned int shift, const std::vector<Block>& blocks) {
    file.seekg(BLOCK_SIZE * shift);
    for (const Block& block : blocks)
        file.write(reinterpret_cast<const char*>(block.asArray()), BLOCK_SIZE);
}

Block Device::readBlock(std::fstream& file, unsigned int index) {
    uint8_t bytes[BLOCK_SIZE];
    file.seekg(BLOCK_SIZE * index);
    file.read(reinterpret_cast<char*>(bytes), BLOCK_SIZE);

    return Block{bytes};
}

std::vector<Block> Device::readBlocks(std::fstream& file, unsigned int shift, unsigned int amount) {
    uint8_t bytes[BLOCK_SIZE * amount];
    file.seekg(BLOCK_SIZE * shift);
    file.read(reinterpret_cast<char*>(bytes), BLOCK_SIZE * amount);

    std::vector<Block> blocks;
    for (unsigned int i = 0; i < amount; i++) {
        blocks.emplace_back(bytes + i * BLOCK_SIZE);
    }

    return blocks;
}

void Device::writeBlock(unsigned int index, const Block& block) {
    return writeBlock(m_Device, index, block);
}

void Device::writeBlocks(unsigned int shift, const std::vector<Block>& blocks) {
    return writeBlocks(m_Device, shift, blocks);
}

Block Device::readBlock(unsigned int index) {
    return readBlock(m_Device, index);
}

std::vector<Block> Device::readBlocks(unsigned int shift, unsigned int amount) {
    return readBlocks(m_Device, shift, amount);
}

void Device::createEmpty(const std::string& name) {
    std::fstream file(name, file.binary | file.out | file.in);
    if (!file.is_open()) {
        std::cout << "Failed to create empty device" << std::endl;
        return;
    }

    DeviceHeader header;
    header.blockSize = 8;
    header.maxFiles = 12;
    header.blocksPerFile = 10;
    Device::BLOCK_SIZE = header.blockSize;
    Device::FD_BLOCKS_PER_FILE = header.blocksPerFile;
    const unsigned int dataCapacityBlocks = 32;

    Device::MAP_START = 0 + ceil(sizeof(DeviceHeader), Device::BLOCK_SIZE);
    DeviceBlockMap map(dataCapacityBlocks);

    Device::FDS_START = Device::MAP_START + map.sizeBlocks();
    std::vector<DeviceFileDescriptor> fds;
    for (unsigned int i = 0; i < header.maxFiles; i++) {
        if (i == 1) {
            fds.emplace_back(DeviceFileType::Regular, 0, 0,
                    std::vector<uint16_t>(header.blocksPerFile, DeviceFileDescriptor::FREE_BLOCK));
        } else if (i == 2) {
            std::vector<uint16_t> addresses(header.blocksPerFile, DeviceFileDescriptor::FREE_BLOCK);
            addresses[0] = 2;
            fds.emplace_back(DeviceFileType::Regular, 3, 0, addresses);
            map.setTaken(2);
        } else if (i == 0) {
            std::vector<uint16_t> addresses(header.blocksPerFile, DeviceFileDescriptor::FREE_BLOCK);
            addresses[0] = 3; // where name is
            addresses[1] = 2; // fd
            addresses[2] = 4; // where name is
            addresses[3] = 1; // fd
            fds.emplace_back(DeviceFileType::Directory, 2, 0, addresses);
            map.setTaken(3);
            map.setTaken(4);
        } else {
            fds.push_back({});
        }
    }


    Device::DATA_START = Device::FDS_START + fds.size() * DeviceFileDescriptor::sizeInBlocks();
    header.firstLogicalBlockShift = Device::DATA_START;
    std::vector<uint8_t> data;
    for (unsigned int i = 0; i < dataCapacityBlocks; i++) {
        data.push_back(0);
    }

    data[2 * header.blockSize + 0] = 66;
    data[2 * header.blockSize + 1] = 65;
    data[2 * header.blockSize + 2] = 67;

    data[3 * header.blockSize + 0] = 'f';
    data[3 * header.blockSize + 1] = 'i';
    data[3 * header.blockSize + 2] = 'l';
    data[3 * header.blockSize + 3] = 'e';
    data[3 * header.blockSize + 4] = '1';

    data[4 * header.blockSize + 0] = 'f';
    data[4 * header.blockSize + 1] = '_';
    data[4 * header.blockSize + 2] = 'e';
    data[4 * header.blockSize + 3] = 'm';
    data[4 * header.blockSize + 4] = 'p';
    data[4 * header.blockSize + 5] = 't';
    data[4 * header.blockSize + 6] = 'y';

    file.write(reinterpret_cast<char*>(&header), 8);
    writeBlocks(file, MAP_START, map.serialize());
    for (unsigned int i = 0; i < fds.size(); i++) {
        writeBlocks(file, FDS_START + i * DeviceFileDescriptor::sizeInBlocks(), fds[i].serialize());
    }
    file.write(reinterpret_cast<char*>(data.data()), data.size());
}




DeviceBlockMap::DeviceBlockMap(unsigned int size) : m_BlocksUsageMap(size / 8, 0xFF), size(size) {}
DeviceBlockMap::DeviceBlockMap(const std::vector<uint8_t>& map, unsigned int size)
        : m_BlocksUsageMap(map), size(size) {}

bool DeviceBlockMap::operator[](unsigned int blockIndex) const {
    return at(blockIndex);
}

void DeviceBlockMap::setFree(unsigned int blockIndex) {
    if (blockIndex >= size)
        throw std::out_of_range("blockIndex >= map size");
    const unsigned int byte = blockIndex / 8;
    const unsigned int shift = blockIndex % 8;
    m_BlocksUsageMap[byte] |= (1 << shift);
}

void DeviceBlockMap::setTaken(unsigned int blockIndex) {
    if (blockIndex >= size)
        throw std::out_of_range("blockIndex >= map size");
    const unsigned int byte = blockIndex / 8;
    const unsigned int shift = blockIndex % 8;
    m_BlocksUsageMap[byte] ^= (m_BlocksUsageMap[byte] & (1 << shift));
}

bool DeviceBlockMap::at(unsigned int blockIndex) const {
    if (blockIndex >= size)
        throw std::out_of_range("blockIndex >= map size");
    const unsigned int byte = blockIndex / 8;
    const unsigned int shift = blockIndex % 8;
    return (m_BlocksUsageMap[byte] & (1 << shift)) != 0;
}

// The tail of the last stored block is unspecified
std::vector<Block> DeviceBlockMap::serialize() const {
    std::vector<Block> blocks;
    std::vector<uint8_t> blockContent(Device::BLOCK_SIZE, 0);
    for (unsigned int i = 0; i < m_BlocksUsageMap.size(); i++) {
        const unsigned int shift = i % Device::BLOCK_SIZE;
        blockContent[shift] = m_BlocksUsageMap[i];
        if ((shift == 0 && i != 0) || (i == m_BlocksUsageMap.size() - 1)) {
            blocks.emplace_back(blockContent);
            blockContent = std::vector<uint8_t>(Device::BLOCK_SIZE, 0);
        }
    }

    return blocks;
}

void DeviceBlockMap::clear() {
    m_BlocksUsageMap.clear();
}

void DeviceBlockMap::add(uint8_t byte) {
    m_BlocksUsageMap.push_back(byte);
}

void DeviceBlockMap::write(Device& device) const {
    device.writeBlocks(Device::MAP_START, serialize());
}


const uint16_t DeviceFileDescriptor::FREE_BLOCK = 0xFFFF;

DeviceFileDescriptor::DeviceFileDescriptor()
        : fileType(DeviceFileType::Empty), size(0), linksCount(0),
        blocks(std::vector<uint16_t>(Device::FD_BLOCKS_PER_FILE, FREE_BLOCK)) {}

DeviceFileDescriptor::DeviceFileDescriptor(DeviceFileType fileType, uint16_t size,
        uint8_t linksCount, const std::vector<uint16_t>& blocks)
        : fileType(fileType), size(size), linksCount(linksCount), blocks(blocks) {}

DeviceFileDescriptor::DeviceFileDescriptor(const std::vector<Block>& rawBlocks) {
    assert(rawBlocks.size() == sizeInBlocks());
    fileType = toDeviceFileType(rawBlocks[0][0]);
    size = static_cast<uint16_t>(rawBlocks[0][2]) << 8 | rawBlocks[0][1];
    linksCount = rawBlocks[0][3];
    unsigned int shift = 4;
    for (const Block& block : rawBlocks) {
        while (shift < Device::BLOCK_SIZE) {
            blocks.push_back(static_cast<uint16_t>(block[shift + 1]) << 8 | block[shift]);
            shift += 2;
        }
        shift = 0;
    }

}

DeviceFileDescriptor DeviceFileDescriptor::read(Device& device, unsigned int index) {
    const unsigned int fdSizeBlocks = DeviceFileDescriptor::sizeInBlocks();
    return DeviceFileDescriptor(device.readBlocks(Device::FDS_START + index * fdSizeBlocks, fdSizeBlocks));
}

void DeviceFileDescriptor::write(
        Device& device, unsigned int index, const DeviceFileDescriptor& dfd) {
    device.writeBlocks(Device::FDS_START + index * sizeInBlocks(), dfd.serialize());
}

std::optional<unsigned int> DeviceFileDescriptor::findFree(Device& device) {
    unsigned int i = 0;
    const unsigned int fdSizeBlocks = DeviceFileDescriptor::sizeInBlocks();
    for (unsigned int blockIndex = Device::FDS_START; blockIndex < Device::DATA_START;
            blockIndex += fdSizeBlocks) {
        DeviceFileDescriptor fd = DeviceFileDescriptor::read(device, i);
        if (fd.fileType == DeviceFileType::Empty) {
            return {i};
        }

        i++;
    }

    return std::nullopt;
}

std::vector<Block> DeviceFileDescriptor::serialize() const {
    std::vector<Block> result;

    Block current;
    current[0] = toInt(fileType);
    current[1] = (size & 0xFF);
    current[2] = (size >> 8);
    current[3] = linksCount;

    unsigned int index = 4;
    for (const uint16_t& b : blocks) {
        current[index] = (b & 0xFF);
        current[index + 1] = (b >> 8);

        index += 2;
        if (index % Device::BLOCK_SIZE == 0) {
            index = 0;
            result.push_back(current);
            current = {};
        }
    }

    return result;
}
