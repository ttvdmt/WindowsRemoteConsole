#pragma once
#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "../utils.hpp"
#include "../define.hpp"

class Client : public Thread {
private:
    Socket m_socket;
    std::atomic<bool> m_running;
    
public:
    Client(const std::string& serverAddress = HOST, unsigned short port = PORT);
    ~Client();
    
    bool connect();
    
protected:
    void run() override;
    
private:
    void handleUserInput();
    void handleServerOutput();
};

#endif // CLIENT_HPP