#include "isaacnx/core.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main(int argc, char** argv) {
    if (argc < 3) return 2;
    const std::string cmd = argv[1];
    if (cmd == "id" && argc == 3) {
        std::string id;
        if (!isaacnx::parse_workshop_id(argv[2], id)) return 1;
        std::cout << id << '\n';
        return 0;
    }
    if (cmd == "metadata" && argc == 4) {
        std::ifstream file(argv[3]);
        std::string response((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        isaacnx::WorkshopItem item;
        std::string error;
        if (!isaacnx::parse_workshop_response(response, argv[2], item, error)) {
            std::cerr << error << '\n'; return 1;
        }
        std::cout << item.id << "\n" << item.title << "\n" << item.download_url << '\n';
        return 0;
    }
    if (cmd == "root" && argc == 3) {
        std::cout << isaacnx::select_installed_mod_root(argv[2]) << '\n';
        return 0;
    }
    if (cmd == "install" && argc == 5) {
        std::string error;
        if (!isaacnx::install_zip(argv[2], argv[3], argv[4], error)) {
            std::cerr << error << '\n'; return 1;
        }
        std::cout << "INSTALLED\n";
        return 0;
    }
    return 2;
}
