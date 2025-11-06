#pragma once
#ifndef UTILS_HPP
#define UTILS_HPP

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

class Socket {
private:
    SOCKET m_socket;
    bool m_blocking;

public:
    Socket() : m_socket(INVALID_SOCKET), m_blocking(true) {}
    Socket(SOCKET s) : m_socket(s), m_blocking(true) {}

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept : m_socket(other.m_socket), m_blocking(other.m_blocking) {
        other.m_socket = INVALID_SOCKET;
    }
    
    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            close();
            m_socket = other.m_socket;
            m_blocking = other.m_blocking;
            other.m_socket = INVALID_SOCKET;
        }
        return *this;
    }
    
    ~Socket() { close(); }

    bool create(int af = AF_INET, int type = SOCK_STREAM, int protocol = IPPROTO_TCP);
    bool bind(const std::string& address, unsigned short port);
    bool listen(int backlog = SOMAXCONN);
    Socket accept();
    bool connect(const std::string& address, unsigned short port);
    void close();
    
    int send(const void* buffer, size_t length, int flags = 0);
    int recv(void* buffer, size_t length, int flags = 0);
    
    bool setBlocking(bool blocking);
    bool isValid() const { return m_socket != INVALID_SOCKET; }
    SOCKET getHandle() const { return m_socket; }
};

class Thread {
private:
    std::unique_ptr<std::thread> m_thread;
    std::atomic<bool> m_running;

public:
    Thread() : m_running(false) {}
    virtual ~Thread() { stop(); }
    
    void start();
    void stop();
    bool isRunning() const { return m_running; }
    
protected:
    virtual void run() = 0;
};

class Pipe {
private:
    HANDLE m_readHandle;
    HANDLE m_writeHandle;

public:
    Pipe() : m_readHandle(INVALID_HANDLE_VALUE), m_writeHandle(INVALID_HANDLE_VALUE) {}
    ~Pipe() { close(); }
    
    bool create();
    void close();

    void closeRead() {
        if (m_readHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_readHandle);
            m_readHandle = INVALID_HANDLE_VALUE;
        }
    }
    
    void closeWrite() {
        if (m_writeHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_writeHandle);
            m_writeHandle = INVALID_HANDLE_VALUE;
        }
    }
    
    HANDLE getReadHandle() const { return m_readHandle; }
    HANDLE getWriteHandle() const { return m_writeHandle; }
    
    DWORD read(void* buffer, DWORD size);
    DWORD write(const void* buffer, DWORD size);
    
    bool isValid() const { 
        return m_readHandle != INVALID_HANDLE_VALUE && 
               m_writeHandle != INVALID_HANDLE_VALUE; 
    }
};

#endif // UTILS_HPP