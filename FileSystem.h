#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <cassert>


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
                    return processMount(arguments[0]);
                case Command::Umount:
                    if (arguments.size() != 0) {
                        std::cout << "Expecting no arguments" << std::endl;
                        return false;
                    }
                    return processUmount();
            }
        }

    private:
        bool processMount(const std::string& deviceName);
        bool processUmount();

        void writeDefaultHeader();
};


#endif
