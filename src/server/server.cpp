#include <algorithm>

#include "server.hpp"

ProcessHandler::ProcessHandler(Socket clientSocket) 
    : m_clientSocket(std::move(clientSocket)) {
    ZeroMemory(&m_processInfo, sizeof(m_processInfo));
    m_clientSocket.setBlocking(true);
}

ProcessHandler::~ProcessHandler() {
    stop();
    if (m_processInfo.hProcess) {
        TerminateProcess(m_processInfo.hProcess, 0);
        CloseHandle(m_processInfo.hProcess);
        CloseHandle(m_processInfo.hThread);
    }
}

bool ProcessHandler::createProcess() {
    std::cout << "Creating process for client..." << std::endl;
    
    if (!m_stdinPipe.create() || !m_stdoutPipe.create()) {
        std::cerr << "Failed to create pipes" << std::endl;
        return false;
    }
    
    STARTUPINFOA startupInfo;
    ZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = m_stdinPipe.getReadHandle();
    startupInfo.hStdOutput = m_stdoutPipe.getWriteHandle();
    startupInfo.hStdError = m_stdoutPipe.getWriteHandle();
    
    PROCESS_INFORMATION processInfo;
    ZeroMemory(&processInfo, sizeof(processInfo));

    char cmdLine[] = "cmd.exe";
    
    BOOL success = CreateProcessA(
        nullptr,
        cmdLine,
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startupInfo,
        &processInfo
    );
    
    if (!success) {
        std::cerr << "CreateProcess failed: " << GetLastError() << std::endl;
        return false;
    }
    
    std::cout << "Process created successfully with PID: " << processInfo.dwProcessId << std::endl;

    m_processInfo = processInfo;

    m_stdinPipe.closeRead();
    m_stdoutPipe.closeWrite();
    
    return true;
}

void ProcessHandler::run() {
    std::cout << "ProcessHandler started" << std::endl;
    
    if (!createProcess()) {
        std::cerr << "Failed to create process, closing connection" << std::endl;
        return;
    }

    if (!m_clientSocket.setBlocking(true)) {
        std::cerr << "Failed to set blocking mode, but continuing..." << std::endl;
    }
    
    std::cout << "Starting data transfer threads..." << std::endl;

    std::thread socketToPipeThread(&ProcessHandler::handleSocketToPipe, this);
    std::thread pipeToSocketThread(&ProcessHandler::handlePipeToSocket, this);

    while (isRunning()) {
        DWORD exitCode;
        if (GetExitCodeProcess(m_processInfo.hProcess, &exitCode)) {
            if (exitCode != STILL_ACTIVE) {
                std::cout << "Child process exited with code: " << exitCode << std::endl;
                break;
            }
        } else {
            std::cout << "GetExitCodeProcess failed, assuming process finished" << std::endl;
            break;
        }

        if (!m_clientSocket.isValid()) {
            std::cout << "Client socket is no longer valid" << std::endl;
            break;
        }
        
        Sleep(100);
    }
    
    std::cout << "Stopping ProcessHandler..." << std::endl;
    stop();

    if (m_processInfo.hProcess) {
        TerminateProcess(m_processInfo.hProcess, 0);
        WaitForSingleObject(m_processInfo.hProcess, 1000);
    }

    m_stdinPipe.close();
    m_stdoutPipe.close();
    m_clientSocket.close();
    
    if (socketToPipeThread.joinable()) {
        socketToPipeThread.join();
    }
    if (pipeToSocketThread.joinable()) {
        pipeToSocketThread.join();
    }

    if (m_processInfo.hProcess) {
        CloseHandle(m_processInfo.hProcess);
    }
    if (m_processInfo.hThread) {
        CloseHandle(m_processInfo.hThread);
    }
    
    std::cout << "ProcessHandler stopped" << std::endl;
}

void ProcessHandler::handleSocketToPipe() {
    std::cout << "Socket to pipe thread started" << std::endl;
    char buffer[4096];
    
    while (isRunning() && m_clientSocket.isValid()) {
        int bytesRead = m_clientSocket.recv(buffer, sizeof(buffer));
        if (bytesRead > 0) {
            std::cout << "Received " << bytesRead << " bytes from client" << std::endl;

            DWORD bytesWritten = m_stdinPipe.write(buffer, bytesRead);
            if (bytesWritten != bytesRead) {
                std::cerr << "Failed to write all data to stdin pipe. Written: " 
                         << bytesWritten << ", expected: " << bytesRead << std::endl;
                break;
            } else {
                std::cout << "Written " << bytesWritten << " bytes to process stdin" << std::endl;
            }

        } else if (bytesRead == 0) {
            std::cout << "Client disconnected (graceful shutdown)" << std::endl;
            break;
        } else {
            int error = WSAGetLastError();
            std::cerr << "Socket receive error in blocking mode: " << error << std::endl;
            break;
        }
    }
    std::cout << "Socket to pipe thread finished" << std::endl;
}

void ProcessHandler::handlePipeToSocket() {
    std::cout << "Pipe to socket thread started" << std::endl;
    char buffer[4096];
    
    while (isRunning() && m_clientSocket.isValid()) {
        DWORD bytesRead = m_stdoutPipe.read(buffer, sizeof(buffer));
        if (bytesRead > 0) {
            std::cout << "Read " << bytesRead << " bytes from process stdout" << std::endl;

            int totalSent = 0;
            while (totalSent < bytesRead && m_clientSocket.isValid()) {
                int bytesSent = m_clientSocket.send(buffer + totalSent, bytesRead - totalSent);
                if (bytesSent > 0) {
                    totalSent += bytesSent;
                    std::cout << "Sent " << bytesSent << " bytes to client (total: " << totalSent << "/" << bytesRead << ")" << std::endl;
                } else if (bytesSent == 0) {
                    std::cout << "Client disconnected during send" << std::endl;
                    break;
                } else {
                    int error = WSAGetLastError();
                    std::cerr << "Failed to send data to client, error: " << error << std::endl;
                    break;
                }
            }
            
            if (totalSent != bytesRead) {
                std::cerr << "Failed to send all data to client. Sent: " << totalSent << ", expected: " << bytesRead << std::endl;
                break;
            }
        } else {
            Sleep(10);
        }
    }
    std::cout << "Pipe to socket thread finished" << std::endl;
}

void ProcessHandler::stop() {
    std::cout << "Stopping ProcessHandler..." << std::endl;

    m_clientSocket.close();

    if (m_processInfo.hProcess) {
        TerminateProcess(m_processInfo.hProcess, 0);
    }
    
    Thread::stop();
    std::cout << "ProcessHandler stopped" << std::endl;
}

Server::Server(unsigned short port) : m_running(false) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return;
    }
    
    if (!m_serverSocket.create()) {
        std::cerr << "Failed to create server socket" << std::endl;
        return;
    }

    int reuse = 1;
    if (setsockopt(m_serverSocket.getHandle(), SOL_SOCKET, SO_REUSEADDR, 
                   (char*)&reuse, sizeof(reuse)) < 0) {
        std::cerr << "setsockopt failed" << std::endl;
    }
    
    if (!m_serverSocket.bind("0.0.0.0", port)) {
        std::cerr << "Failed to bind server socket to port " << port << std::endl;
        return;
    }
    
    if (!m_serverSocket.listen()) {
        std::cerr << "Failed to listen on server socket" << std::endl;
        return;
    }
    
    std::cout << "Server initialized successfully on port " << port << std::endl;
}

Server::~Server() {
    std::cout << "Server destructor called" << std::endl;
    stop();

    m_serverSocket.close();

    for (auto& handler : m_handlers) {
        handler->stop();
    }
    m_handlers.clear();

    Sleep(100);
    
    WSACleanup();
    std::cout << "Server cleanup completed" << std::endl;
}

void Server::run() {
    m_running = true;
    std::cout << "Server started and waiting for connections..." << std::endl;

    m_serverSocket.setBlocking(true);
    
    while (m_running) {
        Socket clientSocket = m_serverSocket.accept();
        
        if (!m_running) {
            if (clientSocket.isValid()) {
                clientSocket.close();
            }
            break;
        }
        
        if (clientSocket.isValid()) {
            std::cout << "New client connected" << std::endl;

            clientSocket.setBlocking(true);
            
            auto handler = std::make_unique<ProcessHandler>(std::move(clientSocket));
            handler->start();
            m_handlers.push_back(std::move(handler));
        } else {
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK && m_running) {
                std::cerr << "Accept failed: " << error << std::endl;
            }
            if (!m_running) {
                break;
            }
        }

        m_handlers.erase(
            std::remove_if(m_handlers.begin(), m_handlers.end(),
                [](const std::unique_ptr<ProcessHandler>& handler) {
                    return !handler->isRunning();
                }),
            m_handlers.end()
        );
        
        Sleep(100);
    }
    
    std::cout << "Server run loop ended" << std::endl;
}

void Server::stop() {
    std::cout << "Server stop initiated..." << std::endl;
    m_running = false;

    m_serverSocket.close();

    for (auto& handler : m_handlers) {
        handler->stop();
    }
    m_handlers.clear();

    std::cout << "Server stop completed" << std::endl;
}

bool Server::initialize() {
    if (!m_serverSocket.isValid()) {
        std::cerr << "Server socket is not valid" << std::endl;
        return false;
    }
    
    std::cout << "Server initialized successfully on port " << PORT << std::endl;
    return true;
}
