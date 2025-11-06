#pragma once
#ifndef SERVER_HPP
#define SERVER_HPP

#include <map>

#include "../utils.hpp"
#include "../define.hpp"

class ProcessHandler : public Thread {
private:
    Socket m_clientSocket;
    Pipe m_stdinPipe;
    Pipe m_stdoutPipe;
    PROCESS_INFORMATION m_processInfo;
    
public:
    ProcessHandler(Socket clientSocket);
    ~ProcessHandler();
    
    bool createProcess();
    
protected:
    void run() override;
    
private:
    void handlePipeToSocket();
    void handleSocketToPipe();

public:
    void stop();
};

class Server : public Thread {
private:
    Socket m_serverSocket;
    std::atomic<bool> m_running;
    std::vector<std::unique_ptr<ProcessHandler>> m_handlers;
    
public:
    Server(unsigned short port = PORT);
    ~Server();
    
    bool initialize();
    
protected:
    void run() override;
    
public:
    void stop();
};

#endif // SERVER_HPP