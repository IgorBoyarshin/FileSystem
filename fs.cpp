#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <bitset>
#include "FileSystem.h"


std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) elems.push_back(item);

    return elems;
}


int main() {
    /* std::fstream file("test", file.binary | file.out | file.in); */
    /* unsigned short a = 5 << 8 | 7; */
    /* file.write(reinterpret_cast<char*>(&a), 1); */
    /* file.write(reinterpret_cast<char*>(&a) + 1, 1); */
    /* file.close(); */
    /*  */
    /* std::fstream file2("test", file.binary | file.out | file.in); */
    /* unsigned int b = 0; */
    /* file2.read(reinterpret_cast<char*>(&b), 2); */
    /* std::cout << std::bitset<16>(a) << std::endl; */
    /* std::cout << std::bitset<16>(b) << std::endl; */
    /* file2.close(); */

    FileSystem fs;
    fs.createEmptyDevice("def3");
    std::string input;
    bool lastFailed = false;
    do {
        if (lastFailed) std::cout << "##";
        std::cout << std::endl;
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
