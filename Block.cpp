#include "Block.h"
#include "Device.h"


unsigned int ceil(unsigned int a, unsigned int b) noexcept {
    if (a % b == 0) return a / b;
    else return (a / b + 1);
}


const uint16_t Block::INVALID = UINT16_MAX;

Block::Block() : bytes(Device::BLOCK_SIZE, 0) {}

Block::Block(const std::vector<uint8_t>& bytes) {
    assert(bytes.size() == Device::BLOCK_SIZE);
    for (unsigned int i = 0; i < Device::BLOCK_SIZE; i++)
        this->bytes.push_back(bytes[i]);
}

Block::Block(uint8_t* bytes) {
    for (unsigned int i = 0; i < Device::BLOCK_SIZE; i++)
        this->bytes.push_back(bytes[i]);
}

uint8_t& Block::operator[](unsigned int index) {
    return bytes.at(index);
}

const uint8_t& Block::operator[](unsigned int index) const {
    return bytes.at(index);
}

const uint8_t* Block::asArray() const {
    return bytes.data();
}