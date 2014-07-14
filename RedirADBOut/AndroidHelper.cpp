#include "StdAfx.h"
#include "AndroidHelper.h"
#include <cfgmgr32.h>
#include <Dbt.h>
#include "Util.h"
#include <algorithm>

#pragma comment(lib,"setupapi")

const wchar_t* CMD_GET_BRAND = L"adb.exe -d shell getprop ro.product.brand"; 			//��ȡ��������
const wchar_t* CMD_GET_MODEL = L"adb.exe -d shell getprop ro.product.model";				//�豸�ͺ�
const wchar_t* CMD_GET_VERSION = L"adb.exe -d shell getprop ro.build.version.release";	//android�汾
const wchar_t* CMD_GET_IMEI = L"adb.exe -d shell dumpsys iphonesubinfo";					//IMEI��
const wchar_t* CMD_GET_MAC = L"adb.exe -d shell cat /sys/class/net/wlan0/address";

#define ADB_BUFFER_SIZE		2048
#define DURATION_WAIT_ADB	(5*1000)

CAndroidHelper::CAndroidHelper(void)
{
    m_hDevInfo = NULL;
}

CAndroidHelper::~CAndroidHelper(void)
{
}

void CAndroidHelper::NotifyDeviceChanged(DWORD wParam, DWORD lParam)
{
    if (wParam == DBT_DEVNODES_CHANGED || wParam == DBT_DEVICEARRIVAL)
    {
        //�����豸���룬������Щ�豸���ᴥ��DBT_DEVICEARRIVAL,����Ҫʹ��DBT_DEVNODES_CHANGED
        SP_DEVINFO_DATA deviceInfo = {0};
        deviceInfo.cbSize = sizeof(SP_DEVINFO_DATA);
        if (SearchPhone(&deviceInfo))
        {
            if (CheckDeviceDriver(GetDeviceInstanceId(m_hDevInfo, deviceInfo).c_str()))
            {
                std::string strResult;
                PostAdbCommand(CMD_GET_IMEI, strResult);
            }
        }
    }
}

bool CAndroidHelper::SearchPhone(SP_DEVINFO_DATA* pDeviceInfo)
{
    bool bResult = false;
    m_hDevInfo = SetupDiGetClassDevs(NULL, L"USB", NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (NULL != m_hDevInfo)
    {
        for (int index = 0; SetupDiEnumDeviceInfo(m_hDevInfo, index, pDeviceInfo); index++)
        {
            std::wstring strCompatibleId = GetDeviceRegisterProperty(m_hDevInfo, *pDeviceInfo, SPDRP_COMPATIBLEIDS);
            //�������ID����"usb\\class_ff&subclass_42"����豸���ֻ�(�����ִ�Сд)
            std::transform(strCompatibleId.begin(), strCompatibleId.end(), strCompatibleId.begin(), ::tolower);
            if (_tcsstr(strCompatibleId.c_str(), _T("usb\\class_ff&subclass_42")) != NULL)
            {
                bResult = true;
                break;
            }

            //��Щ�ֻ��ļ���ID����"usb\\class_ff&subclass_42",�ǾͱȽ��鷳��,��Ҫƥ�� Ӳ��ID
            std::wstring strHardwareID = GetDeviceRegisterProperty(m_hDevInfo, *pDeviceInfo, SPDRP_HARDWAREID);	//VID_1234&PID_4321
            std::vector<std::wstring> vecPVIDs = SpliterString(strHardwareID, L"&");
            if (vecPVIDs.size() >= 2)
            {
                std::vector<std::wstring> pairVid = SpliterString(vecPVIDs[0], L"_");
                std::vector<std::wstring> pairPid = SpliterString(vecPVIDs[1], L"_");
                //��ʼƥ��PID VID,��ʱδʵ��
            }
        }
    }
    return bResult;
}

std::wstring CAndroidHelper::GetDeviceRegisterProperty(HDEVINFO hDevInfo, SP_DEVINFO_DATA deviceInfo, DWORD categoryId)
{
    DWORD dataType = 0;
    DWORD buffSize = 0;
    SetupDiGetDeviceRegistryProperty(hDevInfo, &deviceInfo, categoryId, &dataType, NULL, buffSize, &buffSize);
    DWORD dwErr = GetLastError();
    if (dwErr != ERROR_INSUFFICIENT_BUFFER)
    {
        return L"";
    }
    LPTSTR szParamID = (LPTSTR)LocalAlloc(LPTR, buffSize + 1);
    SetupDiGetDeviceRegistryProperty(hDevInfo, &deviceInfo, categoryId, &dataType, (PBYTE)szParamID, buffSize, &buffSize);
    std::wstring strParamId = szParamID;
    ::LocalFree(szParamID);
    return strParamId;
}

std::wstring CAndroidHelper::GetDeviceInstanceId(HDEVINFO hDevInfo, SP_DEVINFO_DATA deviceInfo)
{
    DWORD dataType = 0;
    DWORD buffSize = 0;
    wchar_t szInstanceId[1024] = {0};
    SetupDiGetDeviceInstanceId(hDevInfo, &deviceInfo, NULL, buffSize, &buffSize);
    DWORD dwErr = GetLastError();
    if (dwErr != ERROR_INSUFFICIENT_BUFFER)
    {
        return L"";
    }
    SetupDiGetDeviceInstanceId(hDevInfo, &deviceInfo, szInstanceId, buffSize, &buffSize);
    return szInstanceId;
}

bool CAndroidHelper::CheckDeviceDriver(const wchar_t* szInstanceId)
{
    DEVINST deviceInstance = 0;
    if (CM_Locate_DevNode(&deviceInstance, (DEVINSTID_W)szInstanceId, CM_LOCATE_DEVNODE_NORMAL) == CR_SUCCESS)
    {
        DWORD status = 0;
        DWORD problemNumber = 0;
        if (CM_Get_DevNode_Status(&status, &problemNumber, deviceInstance, 0) == CR_SUCCESS)
        {
            if (!(status & DN_HAS_PROBLEM))
            {
                //����������װ
                return true;
            }
            else
            {
                if (problemNumber == CM_PROB_DRIVER_FAILED_LOAD || problemNumber == CM_PROB_DRIVER_FAILED_PRIOR_UNLOAD)
                {
                    //��������ʧ��
                    return false;
                }
                else
                {
                    //����δ��װ
                    return false;
                }
            }
        }
    }
    return false;
}

bool CAndroidHelper::PostAdbCommand(const wchar_t* szCMD, std::string& strResult)
{
	bool bResult = false;
    SECURITY_ATTRIBUTES securityAttributes = {0};
    securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
	securityAttributes.lpSecurityDescriptor = NULL;
	securityAttributes.bInheritHandle = TRUE;

    char recvBuffer[ADB_BUFFER_SIZE] = {0};
    DWORD recvLen = 0;
    DWORD occupyLen = 0;

    HANDLE hStdoutReadTmp;				// parent stdout read handle
    HANDLE hStdoutWrite, hStderrWrite;	// child stdout write handle
    HANDLE hStdinWriteTmp;				// parent stdin write handle
    HANDLE hStdinRead;					// child stdin read handle
    HANDLE hStdinWrite, hStdoutRead;
    if (!::CreatePipe(&hStdoutReadTmp, &hStdoutWrite, &securityAttributes, 0))
    {
        return false;
    }
    // Create a duplicate of the stdout write handle for the std
    // error write handle. This is necessary in case the child
    // application closes one of its std output handles.
    if (!::DuplicateHandle(::GetCurrentProcess(),hStdoutWrite,::GetCurrentProcess(),&hStderrWrite,0,TRUE,DUPLICATE_SAME_ACCESS))
    {
        return false;
    }

    // Create a child stdin pipe.
    if (!::CreatePipe(&hStdinRead, &hStdinWriteTmp, &securityAttributes, 0))
    {
        return false;
    }

    // Create new stdout read handle and the stdin write handle.
    // Set the inheritance properties to FALSE. Otherwise, the child
    // inherits the these handles; resulting in non-closeable
    // handles to the pipes being created.
    if (!::DuplicateHandle(::GetCurrentProcess(),hStdoutReadTmp,::GetCurrentProcess(),&hStdoutRead,0,FALSE,DUPLICATE_SAME_ACCESS))
    {
        return false;
    }

    if (!::DuplicateHandle(::GetCurrentProcess(),hStdinWriteTmp,::GetCurrentProcess(),&hStdinWrite,0, FALSE,DUPLICATE_SAME_ACCESS))
    {
        return false;
    }

    // Close inheritable copies of the handles we do not want to
    // be inherited.
    CloseHandle(hStdoutReadTmp);
    CloseHandle(hStdinWriteTmp);

    PROCESS_INFORMATION processInfo = {0};
    STARTUPINFO startInfo;
    startInfo.cb = sizeof(STARTUPINFO);
    GetStartupInfo(&startInfo);
    startInfo.wShowWindow = SW_HIDE;
    startInfo.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    startInfo.hStdError = hStderrWrite;			//���ӽ��̵ı�׼��������ض��򵽹ܵ�����
    startInfo.hStdOutput = hStdoutWrite;		//���ӽ��̵ı�׼����ض��򵽹ܵ�����
    startInfo.hStdInput = hStdinRead;			//���ӽ��̵ı�׼�����ض��򵽹ܵ����

    TCHAR szCommand[MAX_PATH] = {0};
    wcscpy_s(szCommand, MAX_PATH, szCMD);
    if (!CreateProcess(NULL, szCommand, NULL, NULL, TRUE, 0, NULL, NULL, &startInfo, &processInfo))
    {
        CloseHandle(hStderrWrite);
        CloseHandle(hStdoutWrite);
        CloseHandle(hStdinRead);
        return false;
    }
    if(WAIT_TIMEOUT == WaitForSingleObject(processInfo.hProcess,DURATION_WAIT_ADB))
    {
    	TerminateProcess(processInfo.hProcess,0);
    	CloseHandle(processInfo.hProcess);
    	return false;
    }
	for (;;)
	{
		recvLen = 0;
		if (!::PeekNamedPipe(hStdoutRead, NULL, 0, NULL,&recvLen, NULL))			// error
		{
			break;
		}
		if (recvLen > 0)					// not data available
		{
			if (!ReadFile(hStdoutRead, recvBuffer, MAX_PATH, &recvLen, NULL))
			{
				break;
			}
			bResult = true;
			break;
		}
	}
	if(bResult)
	{
		//��ȡIMEI
		CStringA strIMEI = recvBuffer;
		if (strIMEI.Find("error:") == -1)
		{
			strIMEI = strIMEI.Mid(strIMEI.FindOneOf("=") + 2);
			strcpy_s(recvBuffer, ADB_BUFFER_SIZE, strIMEI.GetBuffer());
			occupyLen = strIMEI.GetLength();
			strIMEI.ReleaseBuffer();
			strResult = recvBuffer;
		}
	}

    CloseHandle(hStderrWrite);
    CloseHandle(hStdoutWrite);
    CloseHandle(hStdinRead);

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    TerminateProcess(processInfo.hProcess, 0);
    return bResult;
}
