#include "FileSystem.h"


bool FileSystem::processMount(const std::string& deviceName) {
    if (m_Device) {
        std::cout << "A device " << m_DeviceName << " is mounted. Unmount it first" << std::endl;
        return false;
    }

    m_Device = std::make_unique<std::fstream>(
            deviceName, m_Device->binary | m_Device->in | m_Device->out | m_Device->ate);
    if (!m_Device->is_open()) {
        std::cout << "Could not open device " << deviceName << std::endl;
        return false;
    }

    m_DeviceName = deviceName;

    const unsigned int actualDeviceSize = m_Device->tellg(); // because was openned at the end
    if (actualDeviceSize < sizeof(DevHeader)) {
        std::cout << "Invalid header found. Recreating empty" << std::endl;
        writeDefaultHeader();
    }
    DevHeader header;
    m_Device->seekg(0);
    m_Device->read(reinterpret_cast<char*>(&header), sizeof(DevHeader));
    std::cout << header;

    return true;
}

bool FileSystem::processUmount() {
    if (!m_Device) {
        std::cout << "No device currently mounted" << std::endl;
        return false;
    }

    std::cout << "Successfully unmounted device " << m_DeviceName << std::endl;

    m_Device.release();
    return true;
}

void FileSystem::writeDefaultHeader() {
    if (!m_Device) {
        std::cout << "Cannot write default header: no mounted device" << std::endl;
        return;
    }

    static const unsigned int defaultBlockSize = 8;
    static const unsigned int defaultMaxFiles = 10;

    DevHeader emptyHeader {defaultBlockSize, defaultMaxFiles};
    m_Device->seekg(0);
    m_Device->write(reinterpret_cast<char*>(&emptyHeader), sizeof(DevHeader));
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
