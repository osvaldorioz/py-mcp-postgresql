#include <nlohmann/json.hpp>
#include <string>
#include <iostream>
#include <fstream>

using json = nlohmann::json;

int main() {
    std::ifstream file("config.json");
    if (!file.is_open()) {
        std::cerr << "Fallo al abrir config.json" << std::endl;
        return 1;
    }
    json config;
    try {
        file >> config;
        std::cout << config.dump(2) << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
