#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include "FileSystem.h"


std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) elems.push_back(item);

    return elems;
}


int main() {
    std::cout << sizeof(DeviceHeader) << std::endl;
    FileSystem fs;
    std::string input;
    bool lastFailed = false;
    do {
        if (lastFailed) std::cout << "##";
        std::cout << ">>>";
        std::getline(std::cin, input);

        std::vector<std::string> arguments = split(input, ' ');
        if (arguments.size() == 0) continue;

        if (arguments[0] == "exit" || arguments[0] == "q") return 0;

        Command command = toCommand(arguments[0]);
        if (command == Command::INVALID) {
            std::cout << "## Invalid command. Try again. " << std::endl;
            lastFailed = true;
            continue;
        }

        arguments.erase(arguments.begin());
        lastFailed = !fs.process(command, arguments);
    } while (true);

    return 0;
}
