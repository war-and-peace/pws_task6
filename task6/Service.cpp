#include "service.h"
#include <fstream>
#include <iostream>
#include <aclapi.h>

LPWSTR Service::name = (LPWSTR)L"AAA_Task6_Service";
HANDLE Service::ghSvcStopEvent = NULL;
SERVICE_STATUS_HANDLE Service::gSvcStatusHandle{};
SERVICE_STATUS Service::gSvcStatus{};
HANDLE Service::hThread = nullptr;
HANDLE Service::hThreadStopEvent = nullptr;

const DWORD waitTime{ 100 };
bool CreateChildProcess(HANDLE hPipe);
BOOL SetPrivilege(
    HANDLE hToken,          // access token handle
    LPCTSTR lpszPrivilege,  // name of privilege to enable/disable
    BOOL bEnablePrivilege   // to enable or disable privilege
);

void* pipeConnect() {
    std::ofstream logger("AAA_task6_logs_pipe_connect.txt", std::ios::out);

    const int BUFSIZE = 1024;
    BOOL   fConnected = FALSE;
    DWORD  dwThreadId = 0;
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    LPCTSTR lpszPipename = TEXT("\\\\.\\pipe\\mynamedpipe");
    

    hPipe = CreateNamedPipe(
        lpszPipename,             // pipe name 
        PIPE_ACCESS_DUPLEX | WRITE_OWNER | WRITE_DAC,       // read/write access 
        PIPE_TYPE_MESSAGE |       // message type pipe 
        PIPE_READMODE_MESSAGE |   // message-read mode 
        PIPE_WAIT,                // blocking mode 
        PIPE_UNLIMITED_INSTANCES, // max. instances  
        BUFSIZE,                  // output buffer size 
        BUFSIZE,                  // input buffer size 
        0,                        // client time-out 
        NULL);                    // default security attribute 

    if (hPipe == INVALID_HANDLE_VALUE)
    {
        logger << "could not create the pipe, error code  " << WSAGetLastError() << std::endl;
        logger.close();
        return nullptr;
    }

    logger << "successfully created the pipe" << std::endl;
    if (SetSecurityInfo(hPipe, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
        logger << "successfully changed the security inof" << std::endl;
    }
    else {
        logger << "failed to change the security info, error code: " << WSAGetLastError() << std::endl;
        logger.close();
        return nullptr;
    }

    while (true) {
        logger << "waiting for the client" << std::endl;
        fConnected = ConnectNamedPipe(hPipe, NULL) ?
            TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (fConnected) {
            logger << "client connected" << std::endl;
            logger.close();
            return hPipe;
        }
        else {
            logger << "connection failed, error code: " << WSAGetLastError() << std::endl;
            logger.close();
            CloseHandle(hPipe);
        }
    }
    

}

DWORD WINAPI worker(LPVOID lpParameter) {

    std::ofstream logger("AAA_TCB_HELPER_TASK_WORKER_LOGS.txt", std::ios::out);

    HANDLE hPipe = pipeConnect();

    if (!CreateChildProcess(hPipe)) {
        logger << "Could not create Child process" << std::endl;
        logger << "error code:  " << WSAGetLastError() << std::endl;
    }
    else {
        logger << "Successfully created child process" << std::endl;
    }

    auto hThreadStopEventHandle = (HANDLE)lpParameter;
    while (true) {
        DWORD res = WaitForSingleObject(hThreadStopEventHandle, waitTime);
        switch (res) {
        case WAIT_OBJECT_0:
            break;
        case WAIT_TIMEOUT:
            continue;
        default:
            logger << "Error occurred" << std::endl;
            logger << "error code" << WSAGetLastError() << std::endl;
            logger.close();
            return 1;
        }
    }
    logger << "Terminating..." << std::endl;
    logger.close();
    return 0;
}

void* Service::createMainThread() {
    std::cerr << "Creating main thread" << std::endl;
    DWORD tid;
    hThreadStopEvent = CreateEvent(nullptr, true, false, nullptr);
    if (!hThreadStopEvent) {
        return nullptr;
    }

    hThread = CreateThread(nullptr, 0, worker, hThreadStopEvent, 0, &tid);
    if (!hThread) {
        return nullptr;
    }


    return hThreadStopEvent;
}

int WINAPI Service::run()
{
    std::cerr << "running" << std::endl;
    // If command-line parameter is "install", install the service. 
    // Otherwise, the service is probably being started by the SCM.

    /*if (lstrcmpi(argv[1], TEXT("install")) == 0)
    {
        SvcInstall();
        return 0;
    }*/

    // TO_DO: Add any additional services for the process to this table.
    SERVICE_TABLE_ENTRY DispatchTable[] =
    {
        { name, (LPSERVICE_MAIN_FUNCTION)SvcMain },
        { NULL, NULL }
    };

    std::cerr << "something" << std::endl;

    // This call returns when the service has stopped. 
    // The process should simply terminate when the call returns.

    if (!StartServiceCtrlDispatcher(DispatchTable))
    {
        SvcReportEvent((LPTSTR)TEXT("StartServiceCtrlDispatcher"));
    }
    return 0;
}

VOID Service::SvcInstall()
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    TCHAR szPath[MAX_PATH];

    if (!GetModuleFileName(NULL, szPath, MAX_PATH))
    {
        printf("Cannot install service (%d)\n", GetLastError());
        return;
    }

    // Get a handle to the SCM database. 

    schSCManager = OpenSCManager(
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 

    if (NULL == schSCManager)
    {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return;
    }

    // Create the service

    schService = CreateService(
        schSCManager,              // SCM database 
        name,                   // name of service 
        name,                   // service name to display 
        SERVICE_ALL_ACCESS,        // desired access 
        SERVICE_WIN32_OWN_PROCESS, // service type 
        SERVICE_DEMAND_START,      // start type 
        SERVICE_ERROR_NORMAL,      // error control type 
        szPath,                    // path to service's binary 
        NULL,                      // no load ordering group 
        NULL,                      // no tag identifier 
        NULL,                      // no dependencies 
        NULL,                      // LocalSystem account 
        NULL);                     // no password 

    if (schService == NULL)
    {
        printf("CreateService failed (%d)\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return;
    }
    else printf("Service installed successfully\n");

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
}

VOID WINAPI Service::SvcMain(DWORD dwArgc, LPTSTR* lpszArgv)
{
    std::cerr << "main running" << std::endl;
    // Register the handler function for the service

    gSvcStatusHandle = RegisterServiceCtrlHandler(
        name,
        SvcCtrlHandler);
    std::cerr << "status handle" << gSvcStatusHandle << std::endl;

    if (!gSvcStatusHandle)
    {
        SvcReportEvent((LPTSTR)TEXT("RegisterServiceCtrlHandler"));
        return;
    }

    // These SERVICE_STATUS members remain as set here

    gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    gSvcStatus.dwServiceSpecificExitCode = 0;

    // Report initial status to the SCM

    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    // Perform service-specific initialization and work.

    std::cerr << "initializing" << std::endl;
    SvcInit(dwArgc, lpszArgv);
}

VOID Service::SvcInit(DWORD dwArgc, LPTSTR* lpszArgv)
{

    ghSvcStopEvent = CreateEvent(
        NULL,    // default security attributes
        TRUE,    // manual reset event
        FALSE,   // not signaled
        NULL);   // no name

    if (ghSvcStopEvent == NULL)
    {
        ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
        return;
    }

    // Report running status when initialization is complete.

    ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

    // TO_DO: Perform work until service stops.
    void* hThreadStopHandle = Service::createMainThread();
    if (!hThreadStopHandle) {
        ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
        return;
    }

    std::ofstream logger("AAA_SERVICE_STOP_SIGNAL.txt", std::ios::out);

    while (1)
    {
        // Check whether to stop the service.

        WaitForSingleObject(ghSvcStopEvent, INFINITE);

        logger << "STOP signal received" << std::endl;
        SetEvent(hThreadStopEvent);
        ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
        return;
    }
}

VOID Service::ReportSvcStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;

    // Fill in the SERVICE_STATUS structure.

    gSvcStatus.dwCurrentState = dwCurrentState;
    gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    gSvcStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
        gSvcStatus.dwControlsAccepted = 0;
    else gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if ((dwCurrentState == SERVICE_RUNNING) ||
        (dwCurrentState == SERVICE_STOPPED))
        gSvcStatus.dwCheckPoint = 0;
    else gSvcStatus.dwCheckPoint = dwCheckPoint++;

    // Report the status of the service to the SCM.
    SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}

VOID WINAPI Service::SvcCtrlHandler(DWORD dwCtrl)
{
    // Handle the requested control code. 

    switch (dwCtrl)
    {
    case SERVICE_CONTROL_STOP:
        ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

        // Signal the service to stop.

        SetEvent(ghSvcStopEvent);
        ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);

        return;

    case SERVICE_CONTROL_INTERROGATE:
        break;

    default:
        break;
    }

}

VOID Service::SvcReportEvent(LPTSTR szFunction)
{
    HANDLE hEventSource;
    LPCTSTR lpszStrings[2];
    TCHAR Buffer[80];

    hEventSource = RegisterEventSource(NULL, name);

    if (NULL != hEventSource)
    {
        StringCchPrintf(Buffer, 80, TEXT("%s failed with %d"), szFunction, GetLastError());

        lpszStrings[0] = name;
        lpszStrings[1] = Buffer;

        ReportEvent(hEventSource,        // event log handle
            EVENTLOG_ERROR_TYPE, // event type
            0,                   // event category
            SVC_ERROR,           // event identifier
            NULL,                // no security identifier
            2,                   // size of lpszStrings array
            0,                   // no binary data
            lpszStrings,         // array of strings
            NULL);               // no binary data

        DeregisterEventSource(hEventSource);
    }
}

bool CreateChildProcess(HANDLE hPipe) {
    std::ofstream logger("AAA_TASK6_LOGS.txt", std::ios::out);

    const int BUFSIZE = 1024;
    char buffer[BUFSIZE];
    DWORD dwRead{};

    bool fSuccess = ReadFile(
        hPipe,        // handle to pipe 
        buffer,    // buffer to receive data 
        BUFSIZE * sizeof(TCHAR), // size of buffer 
        &dwRead, // number of bytes read 
        NULL);        // not overlapped I/O 

    if (!fSuccess || dwRead == 0)
    {
        logger << "error while reading, error code: " << WSAGetLastError() << std::endl;
        logger.close();
        return false;
    }

    

    HANDLE hToken, hNewToken;

    if (!ImpersonateNamedPipeClient(hPipe)) {
        logger << "impersonation failed, error code: " << WSAGetLastError() << std::endl;
        return FALSE;
    }

    if (!OpenThreadToken(GetCurrentThread(), TOKEN_ALL_ACCESS, TRUE, &hNewToken)){
        logger << "open thread token failed, error code: " << WSAGetLastError() << std::endl;
        return false;
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    wchar_t cmdline[] = L"cmd.exe";

    if (!CreateProcessAsUserW(hNewToken, NULL, cmdline, NULL, NULL, FALSE,
        CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
        logger << "create process failed, error code: " << WSAGetLastError() << std::endl;
        return FALSE;
    }
    logger.close();
}

BOOL SetPrivilege(
    HANDLE hToken,          // access token handle
    LPCTSTR lpszPrivilege,  // name of privilege to enable/disable
    BOOL bEnablePrivilege   // to enable or disable privilege
)
{
    TOKEN_PRIVILEGES tp;
    LUID luid;

    if (!LookupPrivilegeValue(
        NULL,            // lookup privilege on local system
        lpszPrivilege,   // privilege to lookup 
        &luid))        // receives LUID of privilege
    {
        printf("LookupPrivilegeValue error: %u\n", GetLastError());
        return FALSE;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    if (bEnablePrivilege)
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    else
        tp.Privileges[0].Attributes = 0;

    // Enable the privilege or disable all privileges.

    if (!AdjustTokenPrivileges(
        hToken,
        FALSE,
        &tp,
        sizeof(TOKEN_PRIVILEGES),
        (PTOKEN_PRIVILEGES)NULL,
        (PDWORD)NULL))
    {
        printf("AdjustTokenPrivileges error: %u\n", GetLastError());
        return FALSE;
    }

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)

    {
        printf("The token does not have the specified privilege. \n");
        return FALSE;
    }

    return TRUE;
}