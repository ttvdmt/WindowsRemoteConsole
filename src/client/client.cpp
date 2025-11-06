#include "client.hpp"

Client::Client(const std::string& serverAddress, unsigned short port) 
    : m_running(false) {
    
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    
    if (!m_socket.create()) {
        std::cerr << "Failed to create client socket" << std::endl;
        return;
    }
    
    if (!m_socket.connect(serverAddress, port)) {
        std::cerr << "Failed to connect to server" << std::endl;
        return;
    }
}

Client::~Client() {
    stop();
    WSACleanup();
}

void Client::run() {
    if (!m_socket.isValid()) {
        std::cerr << "Cannot start client: invalid socket" << std::endl;
        return;
    }

    if (!m_socket.setBlocking(true)) {
        std::cerr << "Warning: failed to set blocking mode" << std::endl;
    }
    
    m_running = true;
    std::cout << "Connected to server. Type commands below:" << std::endl;
    
    std::thread inputThread(&Client::handleUserInput, this);
    handleServerOutput();
    
    if (inputThread.joinable()) {
        inputThread.join();
    }
}

void Client::handleServerOutput() {
    char buffer[4096];
    
    while (m_running) {
        int bytesRead = m_socket.recv(buffer, sizeof(buffer));
        if (bytesRead > 0) {
            std::cout.write(buffer, bytesRead);
            std::cout.flush();
        } else if (bytesRead == 0) {
            std::cout << "\nConnection closed by server" << std::endl;
            break;
        } else {
            std::cerr << "\nReceive error: " << WSAGetLastError() << std::endl;
            break;
        }
    }
    
    m_running = false;
}

void Client::handleUserInput() {
    std::string input;
    
    while (m_running) {
        std::getline(std::cin, input);
        if (!m_running) break;
        
        input += "\r\n";
        
        if (m_socket.send(input.c_str(), input.size()) <= 0) {
            break;
        }
    }
}