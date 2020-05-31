#include <iostream>
#include "Service.h"
#include "Client.h"

int main(int argc, const char* argv[])
{
    if (argc == 1) {
        Service service;
        service.run();
    }
    else if(argc == 2) {
        if (std::string(argv[1]) == "--client") {
            Client client;
            if (!client.connect()) {
                std::cerr << "error while connecting to the server" << std::endl;
                return 1;
            }
            client.halt();
        }
        else {
            std::cerr << "Invalid argument given" << std::endl;
            return 1;
        }
    }
    else {
        std::cerr << "Invalid arguments" << std::endl;
        return 1;
    }
    return 0;
}
