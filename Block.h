#ifndef BLOCK_H
#define BLOCK_H

#include <vector>
#include <cstdint>
#include <cassert>
#include <string>

unsigned int ceil(unsigned int a, unsigned int b) noexcept;


struct Block {
    public:
        static const uint16_t INVALID;
    private:
        std::vector<uint8_t> bytes;
    public:
        uint8_t& operator[](unsigned int index);
        const uint8_t& operator[](unsigned int index) const;
        const uint8_t* asArray() const;
        std::string asString() const;

        Block();
        Block(const std::vector<uint8_t>& bytes);
        Block(uint8_t* bytes);
        Block(const std::string& str);
};


#endif
