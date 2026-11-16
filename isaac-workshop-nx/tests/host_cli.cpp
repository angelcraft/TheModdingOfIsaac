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
    if (cmd == "listing" && argc == 3) {
        std::ifstream file(argv[2]);
        std::string html((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        std::vector<std::string> ids;
        std::string error;
        if (!isaacnx::parse_workshop_listing(html, ids, error)) {
            std::cerr << error << '\n'; return 1;
        }
        for (const auto& id : ids) std::cout << id << '\n';
        return 0;
    }
    if (cmd == "batch" && argc == 4) {
        std::ifstream file(argv[3]);
        std::string response((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        std::vector<std::string> ids;
        std::string args = argv[2];
        size_t begin = 0;
        while (begin < args.size()) {
            size_t end = args.find(',', begin);
            if (end == std::string::npos) end = args.size();
            ids.push_back(args.substr(begin, end - begin));
            begin = end + 1;
        }
        std::vector<isaacnx::WorkshopItem> items;
        std::string error;
        if (!isaacnx::parse_workshop_batch_response(response, ids, items, error)) {
            std::cerr << error << '\n'; return 1;
        }
        for (const auto& item : items) {
            std::cout << item.id << " | " << isaacnx::safe_console_text(item.title, 100)
                      << " | " << item.preview_url << "\n";
        }
        return 0;
    }
    if (cmd == "clean-text" && argc == 3) {
        std::cout << isaacnx::safe_console_text(argv[2], 100) << '\n';
        return 0;
    }
    if (cmd == "bridge" && argc == 3) {
        std::string endpoint, error;
        if (!isaacnx::parse_bridge_endpoint(argv[2], endpoint, error)) {
            std::cerr << error << '\n'; return 1;
        }
        std::cout << endpoint << '\n';
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
