#include <iostream>

#include "service.hpp"
#include "../server/server.hpp"
#include "../define.hpp"

Service* Service::s_instance = nullptr;

Service::Service(const std::string& serviceName, const std::string& displayName)
    : m_serviceName(serviceName), m_displayName(displayName), 
      m_server(nullptr), m_stopEvent(INVALID_HANDLE_VALUE) {
    
    s_instance = this;

    m_serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    m_serviceStatus.dwCurrentState = SERVICE_STOPPED;
    m_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    m_serviceStatus.dwWin32ExitCode = NO_ERROR;
    m_serviceStatus.dwServiceSpecificExitCode = 0;
    m_serviceStatus.dwCheckPoint = 0;
    m_serviceStatus.dwWaitHint = 0;
}

Service::~Service() {
    if (m_stopEvent != INVALID_HANDLE_VALUE) {
        CloseHandle(m_stopEvent);
    }
    delete m_server;
}

bool Service::install() {
    SC_HANDLE scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!scm) {
        std::cerr << "OpenSCManager failed: " << GetLastError() << std::endl;
        return false;
    }

    char modulePath[MAX_PATH];
    GetModuleFileNameA(nullptr, modulePath, MAX_PATH);

    SC_HANDLE service = CreateServiceA(
        scm,
        m_serviceName.c_str(),
        m_displayName.c_str(),
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        modulePath,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );

    if (!service) {
        DWORD error = GetLastError();
        if (error == ERROR_SERVICE_EXISTS) {
            std::cout << "Service already exists." << std::endl;
        } else {
            std::cerr << "CreateService failed: " << error << std::endl;
        }
        CloseServiceHandle(scm);
        return false;
    }

    SERVICE_DESCRIPTIONA description;
    description.lpDescription = (LPSTR)"Remote Console Service - provides SSH-like remote command execution";
    ChangeServiceConfig2A(service, SERVICE_CONFIG_DESCRIPTION, &description);

    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    
    std::cout << "Service installed successfully." << std::endl;
    return true;
}

bool Service::uninstall() {
    SC_HANDLE scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!scm) {
        std::cerr << "OpenSCManager failed: " << GetLastError() << std::endl;
        return false;
    }

    SC_HANDLE service = OpenServiceA(scm, m_serviceName.c_str(), DELETE);
    if (!service) {
        std::cerr << "OpenService failed: " << GetLastError() << std::endl;
        CloseServiceHandle(scm);
        return false;
    }

    bool success = DeleteService(service) != 0;
    if (!success) {
        std::cerr << "DeleteService failed: " << GetLastError() << std::endl;
    } else {
        std::cout << "Service uninstalled successfully." << std::endl;
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return success;
}

void Service::run() {
    SERVICE_TABLE_ENTRYA serviceTable[] = {
        { (LPSTR)m_serviceName.c_str(), serviceMain },
        { nullptr, nullptr }
    };

    if (!StartServiceCtrlDispatcherA(serviceTable)) {
        std::cerr << "StartServiceCtrlDispatcher failed: " << GetLastError() << std::endl;
    }
}

void WINAPI Service::serviceMain(DWORD argc, LPSTR* argv) {
    Service* service = getInstance();
    service->m_serviceStatusHandle = RegisterServiceCtrlHandlerA(
        service->m_serviceName.c_str(), serviceCtrlHandler);
    
    if (!service->m_serviceStatusHandle) {
        return;
    }

    service->setServiceStatus(SERVICE_START_PENDING);
    service->m_stopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    
    if (service->m_stopEvent == INVALID_HANDLE_VALUE) {
        service->setServiceStatus(SERVICE_STOPPED);
        return;
    }

    service->setServiceStatus(SERVICE_RUNNING);
    service->serviceWorker();
    service->setServiceStatus(SERVICE_STOPPED);
}

void WINAPI Service::serviceCtrlHandler(DWORD ctrlCode) {
    Service* service = getInstance();
    
    switch (ctrlCode) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        service->setServiceStatus(SERVICE_STOP_PENDING);
        service->stop();
        break;
    default:
        break;
    }
}

void Service::setServiceStatus(DWORD currentState, DWORD win32ExitCode, DWORD waitHint) {
    m_serviceStatus.dwCurrentState = currentState;
    m_serviceStatus.dwWin32ExitCode = win32ExitCode;
    m_serviceStatus.dwWaitHint = waitHint;

    if (currentState == SERVICE_START_PENDING) {
        m_serviceStatus.dwControlsAccepted = 0;
    } else {
        m_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    }

    if (currentState == SERVICE_RUNNING || currentState == SERVICE_STOPPED) {
        m_serviceStatus.dwCheckPoint = 0;
    } else {
        m_serviceStatus.dwCheckPoint++;
    }

    SetServiceStatus(m_serviceStatusHandle, &m_serviceStatus);
}

void Service::serviceWorker() {
    m_server = new Server(PORT);
    m_server->start();

    HANDLE waitHandles[2];
    waitHandles[0] = m_stopEvent;
    
    while (true) {
        DWORD waitResult = WaitForMultipleObjects(1, waitHandles, FALSE, 1000);
        
        if (waitResult == WAIT_OBJECT_0) {
            break;
        }
        else if (waitResult == WAIT_TIMEOUT) {
            continue;
        }
    }
    
    m_server->stop();
}

void Service::stop() {
    SetEvent(m_stopEvent);
}