#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <csignal>

#include "server/server.hpp"
#include "client/client.hpp"
#include "service/service.hpp"
#include "define.hpp"

std::atomic<bool> g_running(true);

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
}

void waitForExit() {
    std::cout << "Press Ctrl+C or Enter to stop server..." << std::endl;
    
    std::signal(SIGINT, signalHandler);

    std::string input;
    std::getline(std::cin, input);
    
    if (g_running) {
        g_running = false;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage:" << std::endl;
        std::cout << "  RemoteConsole -s                 Run as server" << std::endl;
        std::cout << "  RemoteConsole -c                 Run as client" << std::endl;
        std::cout << "  RemoteConsole -install           Install service" << std::endl;
        std::cout << "  RemoteConsole -uninstall         Uninstall service" << std::endl;
        std::cout << "  RemoteConsole -run               Run as service" << std::endl;
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "-s") {
        Server server(PORT);
        if (!server.initialize()) {
            std::cerr << "Server initialization failed!" << std::endl;
            return 1;
        }
        
        server.start();

        std::thread exitThread(waitForExit);

        while (g_running) {
            Sleep(100);
        }
        
        std::cout << "Stopping server..." << std::endl;
        server.stop();

        if (server.isRunning()) {
            std::cout << "Waiting for server thread to finish..." << std::endl;
            server.stop();
        }

        if (exitThread.joinable()) {
            exitThread.join();
        }
        
        std::cout << "Server stopped successfully" << std::endl;
    } 
    else if (mode == "-c") {
        Client client(HOST, PORT);
        client.start();
        client.stop();
    }
    else if (mode == "-install") {
        Service service("RemoteConsoleService", "Remote Console Service");
        if (service.install()) {
            std::cout << "Service installed successfully." << std::endl;
            std::cout << "You can start it with: net start RemoteConsoleService" << std::endl;
        } else {
            std::cerr << "Failed to install service." << std::endl;
        }
    }
    else if (mode == "-uninstall") {
        Service service("RemoteConsoleService", "Remote Console Service");
        if (service.uninstall()) {
            std::cout << "Service uninstalled successfully." << std::endl;
        } else {
            std::cerr << "Failed to uninstall service." << std::endl;
        }
    }
    else if (mode == "-run") {
        Service service("RemoteConsoleService", "Remote Console Service");
        service.run();
    }
    else {
        std::cerr << "Invalid mode." << std::endl;
        return 1;
    }

    return 0;
}