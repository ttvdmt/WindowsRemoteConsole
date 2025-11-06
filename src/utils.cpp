#include "utils.hpp"

bool Socket::create(int af, int type, int protocol) {
    close();
    m_socket = socket(af, type, protocol);
    return m_socket != INVALID_SOCKET;
}

bool Socket::bind(const std::string& address, unsigned short port) {
    if (m_socket == INVALID_SOCKET) {
        return false;
    }

    int reuse = 1;
    if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) < 0) {
        std::cerr << "setsockopt SO_REUSEADDR failed: " << WSAGetLastError() << std::endl;
    }
    
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (address.empty() || address == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
            return false;
        }
    }
    
    if (::bind(m_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed for port " << port << ", error: " << WSAGetLastError() << std::endl;
        return false;
    }
    
    return true;
}

bool Socket::listen(int backlog) {
    return ::listen(m_socket, backlog) != SOCKET_ERROR;
}

Socket Socket::accept() {
    if (m_socket == INVALID_SOCKET) {
        return Socket();
    }
    
    SOCKET client_socket = ::accept(m_socket, nullptr, nullptr);
    if (client_socket == INVALID_SOCKET) {
        return Socket();
    }
    
    return Socket(client_socket);
}

bool Socket::connect(const std::string& address, unsigned short port) {
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, address.c_str(), &addr.sin_addr);
    
    return ::connect(m_socket, (sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR;
}

void Socket::close() {
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
}

int Socket::send(const void* buffer, size_t length, int flags) {
    return ::send(m_socket, (const char*)buffer, length, flags);
}

int Socket::recv(void* buffer, size_t length, int flags) {
    return ::recv(m_socket, (char*)buffer, length, flags);
}

bool Socket::setBlocking(bool blocking) {
    if (m_socket == INVALID_SOCKET) {
        std::cerr << "Cannot set blocking mode: socket is invalid" << std::endl;
        return false;
    }
    
    u_long mode = blocking ? 0 : 1;
    if (ioctlsocket(m_socket, FIONBIO, &mode) != 0) {
        int error = WSAGetLastError();
        std::cerr << "ioctlsocket failed with error: " << error << std::endl;
        return false;
    }
    m_blocking = blocking;
    std::cout << "Socket set to " << (blocking ? "blocking" : "non-blocking") << " mode" << std::endl;
    return true;
}

void Thread::start() {
    if (!m_running) {
        m_running = true;
        m_thread = std::make_unique<std::thread>([this]() { run(); });
    }
}

void Thread::stop() {
    if (m_running) {
        m_running = false;
        if (m_thread && m_thread->joinable()) {
            m_thread->join();
        }
    }
}

bool Pipe::create() {
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;
    
    if (!CreatePipe(&m_readHandle, &m_writeHandle, &sa, 0)) {
        return false;
    }

    return true;
}

void Pipe::close() {
    closeRead();
    closeWrite();
}

DWORD Pipe::read(void* buffer, DWORD size) {
    DWORD bytesRead = 0;
    if (!ReadFile(m_readHandle, buffer, size, &bytesRead, nullptr)) {
        return 0;
    }
    return bytesRead;
}

DWORD Pipe::write(const void* buffer, DWORD size) {
    DWORD bytesWritten = 0;
    if (!WriteFile(m_writeHandle, buffer, size, &bytesWritten, nullptr)) {
        return 0;
    }
    return bytesWritten;
}