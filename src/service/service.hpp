#pragma once
#ifndef SERVICE_HPP
#define SERVICE_HPP

#include "../utils.hpp"
#include "../server/server.hpp"

#include <windows.h>
#include <string>


class Service {
private:
    static Service* s_instance;
    
    std::string m_serviceName;
    std::string m_displayName;
    SERVICE_STATUS m_serviceStatus;
    SERVICE_STATUS_HANDLE m_serviceStatusHandle;
    HANDLE m_stopEvent;
    Server* m_server;
    
public:
    Service(const std::string& serviceName, const std::string& displayName);
    ~Service();
    
    static Service* getInstance() { return s_instance; }
    
    bool install();
    bool uninstall();
    void run();
    void stop();

    static void WINAPI serviceMain(DWORD argc, LPSTR* argv);
    static void WINAPI serviceCtrlHandler(DWORD ctrlCode);
    
private:
    void setServiceStatus(DWORD currentState, DWORD win32ExitCode = NO_ERROR, DWORD waitHint = 0);
    void serviceWorker();
};

#endif // SERVICE_HPP