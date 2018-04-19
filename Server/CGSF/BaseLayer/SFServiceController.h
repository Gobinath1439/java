#pragma once

class SFServiceController
{
public:
	SFServiceController(void);
	virtual ~SFServiceController(void);

	BOOL InstallService(TCHAR* szServiceName, TCHAR* szServiceDescription, TCHAR* szServicePath);
	BOOL DeleteService( TCHAR* szServiceName);
	BOOL StartService(TCHAR* szServiceName);
	BOOL StopService(TCHAR* szServiceName);

	BOOL ServiceEntry(TCHAR* szServiceName, LPTHREAD_START_ROUTINE ServiceStartEntry);

	static void ServiceMain(DWORD argc, LPTSTR *argv); 
	static void ServiceCtrlHandler(DWORD nControlCode);
	static BOOL UpdateServiceStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode,
		DWORD dwServiceSpecificExitCode, DWORD dwCheckPoint,
		DWORD dwWaitHint);
	static BOOL StartServiceThread();
	static DWORD ServiceExecutionThread(LPDWORD param);
	static LPTHREAD_START_ROUTINE funcServiceMainEntry;
	
	static void KillService();

	static BOOL nServiceRunning;
	static HANDLE killServiceEvent;

protected:

private:
	static SERVICE_STATUS_HANDLE nServiceStatusHandle; 
	static DWORD nServiceCurrentStatus;
	static HANDLE hServiceThread;
};
