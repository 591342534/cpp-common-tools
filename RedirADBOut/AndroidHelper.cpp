/********************************************************************
	created:	2014/07/18
	created:	18:7:2014   13:46
	filename: 	AndroidHelper.cpp
	file path:	
	file base:	AndroidHelper
	file ext:	cpp
	author:		findeway
	
	purpose:	
*********************************************************************/
#include "StdAfx.h"
#include "AndroidHelper.h"
#include <cfgmgr32.h>
#include <Dbt.h>
#include "Util.h"
#include <algorithm>
#include <boost/bind.hpp>
#include "Util.h"

#pragma comment(lib,"setupapi")

const wchar_t* CMD_GET_BRAND = L"adb.exe -d shell getprop ro.product.brand";
const wchar_t* CMD_GET_MODEL = L"adb.exe -d shell getprop ro.product.model";
const wchar_t* CMD_GET_VERSION = L"adb.exe -d shell getprop ro.build.version.release";
const wchar_t* CMD_GET_IMEI = L"adb.exe -d shell dumpsys iphonesubinfo";
const wchar_t* CMD_GET_MAC = L"adb.exe -d shell cat /sys/class/net/wlan0/address";
const wchar_t* CMD_PUSH_FILE = L"adb.exe push %s %s";
const wchar_t* CMD_CHECK_EXIST = L"adb.exe shell ls %s";
const wchar_t* CMD_CREATE_DIR = L"adb.exe shell mkdir %s";

const wchar_t* ERROR_INFO_GETIMEI_FAILED = L"Failed to get IMEI code";
const wchar_t* ERROR_INFO_GETMODEL_FAILED = L"Failed to get Model";
const wchar_t* ERROR_INFO_GETVERSION_FAILED = L"Failed to get Version";
const wchar_t* ERROR_INFO_GETMAC_FAILED = L"Failed to get MAC";
const wchar_t* ERROR_INFO_GETBRAND_FAILED = L"Failed to get Brand";
const wchar_t* ERROR_INFO_CHECKEXIST_FAILED = L"No such file or directory";

#define ADB_BUFFER_SIZE		2048
#define DURATION_WAIT_ADB	(5*1000)

CAndroidHelper::CAndroidHelper(void)
{
    m_hDevInfo = NULL;
	m_bDeviceConnected = false;
	m_strStorageDir = L"/sdcard/333";
	UpdateConnectionState();
}

CAndroidHelper::~CAndroidHelper(void)
{
	m_mapConnectCallback.clear();
	m_mapProgressCallback.clear();
}

void CAndroidHelper::NotifyDeviceChanged(DWORD wParam, DWORD lParam)
{
    if (wParam == DBT_DEVNODES_CHANGED || wParam == DBT_DEVICEARRIVAL)
    {
        //�����豸���룬������Щ�豸���ᴥ��DBT_DEVICEARRIVAL,����Ҫʹ��DBT_DEVNODES_CHANGED
		UpdateConnectionState();
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

bool CAndroidHelper::PostAdbCommand(const wchar_t* szCMD, std::string& strResult,bool bWaitResult)
{
	if(!m_bDeviceConnected)
	{
		return false;
	}
	bool bResult = false;
    SECURITY_ATTRIBUTES securityAttributes = {0};
    securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
	securityAttributes.lpSecurityDescriptor = NULL;
	securityAttributes.bInheritHandle = TRUE;

   
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
	//��ʱ�������ȴ����̽���
    if(!NeedWaitProcess(szCMD))
	{
		if(WAIT_TIMEOUT == WaitForSingleObject(processInfo.hProcess,DURATION_WAIT_ADB))
		{
			TerminateProcess(processInfo.hProcess,0);
			CloseHandle(processInfo.hProcess);
			return false;
		}
	}
	if(bWaitResult)
	{
		std::string pipeResult = ReadResponseFromPipe(hStdoutRead,szCMD);
		strResult = FilterResult(szCMD,pipeResult);
	}

    CloseHandle(hStderrWrite);
    CloseHandle(hStdoutWrite);
    CloseHandle(hStdinRead);

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    TerminateProcess(processInfo.hProcess, 0);
    return bResult;
}

bool CAndroidHelper::PushFile( const wchar_t* szSourcefile, const wchar_t* szDest, ProgressCallback callback )
{
	if(!m_bDeviceConnected)
	{
		return false;
	}
	std::string strResult;
	CString strCMD;
	if(_taccess(szSourcefile,0) == -1)
	{
		return false;
	}
	strCMD.Format(CMD_PUSH_FILE,szSourcefile,szDest);
	if(callback != NULL)
	{
		std::wstring cmd = LPCTSTR(strCMD);
		m_mapProgressCallback.insert(std::make_pair(cmd,callback));
	}
	PostAdbCommand(LPCTSTR(strCMD), strResult);
	if(!strResult.empty())
	{
		return true;
	}
	return false;
}

std::string CAndroidHelper::FilterResult( const wchar_t* szCMD, std::string strResultMsg )
{
	std::string strResult;
	CStringA strBuffer = strResultMsg.c_str();
	if (strBuffer.Find("error:") == -1)
	{
		if(_tcsicmp(szCMD,CMD_GET_IMEI) == 0)
		{
			if(strBuffer.Find("Device ID =") >= 0)
			{
				//��ȡIMEI
				CStringA strIMEI = strBuffer.Mid(strBuffer.Find("Device ID = ") + strlen("Device ID = "));
				strIMEI.ReleaseBuffer();
				strResult = (LPCSTR)strIMEI;
			}
			else
			{
				strResult = W2Utf8(ERROR_INFO_GETIMEI_FAILED);
			}
		}
		else if(_tcsicmp(szCMD,CMD_GET_MODEL) == 0)
		{
			strResult = LPCSTR(strBuffer);
		}
		else if(_tcsicmp(szCMD,CMD_GET_VERSION) == 0)
		{
			strResult = LPCSTR(strBuffer);
		}
		else if(_tcsicmp(szCMD,CMD_GET_BRAND) == 0)
		{
			strResult = LPCSTR(strBuffer);
		}
		else if(_tcsicmp(szCMD,CMD_GET_MAC) == 0)
		{
			strResult = LPCSTR(strBuffer);
		}
		else
		{
			strResult = LPCSTR(strBuffer);
		}
	}
	else
	{
		strResult = LPCSTR(strBuffer);
	}
	return strResult;
}

std::string CAndroidHelper::ReadResponseFromPipe( HANDLE hStdOutRead, const wchar_t* szCMD )
{
	std::string strResult;
	char recvBuffer[ADB_BUFFER_SIZE] = {0};
	DWORD recvLen = 0;
	DWORD occupyLen = 0;

	for (;;)
	{
		recvLen = 0;
		if (!::PeekNamedPipe(hStdOutRead, NULL, 0, NULL,&recvLen, NULL))			// error
		{
			break;
		}
		if (recvLen > 0)					// not data available
		{
			//���buffer
			memset(recvBuffer,0,ADB_BUFFER_SIZE);
			if (ReadFile(hStdOutRead, recvBuffer, MAX_PATH, &recvLen, NULL))
			{
				//���˵�һЩ������Ϣ
				strResult = FilterUselessMsg(recvBuffer);
				//�ж��Ƿ��Ǻ�ʱ�������Ƿ�ȴ����̽���
				if(!NeedWaitProcess(szCMD))
				{
					break;
				}
				else
				{
					if(UpdateProgress(strResult,szCMD))
					{
						break;
					}
				}
			}
			else
			{
				break;
			}
		}
	}
	return strResult;
}

bool CAndroidHelper::NeedWaitProcess( const wchar_t* szCMD )
{
	if(_tcsstr(szCMD,L"push") != NULL)
	{
		return true;
	}
	return false;
}

void CAndroidHelper::NotifyProgress( float fPercent, const wchar_t* szCMD )
{
	if(!m_mapProgressCallback.empty())
	{
		std::wstring strCMD = szCMD;
		if(m_mapProgressCallback.find(strCMD) != m_mapProgressCallback.end())
		{
			ProgressCallback callback = m_mapProgressCallback.find(strCMD)->second;
			if(callback)
			{
				callback(int(fPercent));
			}
		}
	}
}

std::wstring CAndroidHelper::GetStorageDir()
{
	//���Ŀ��·���Ƿ����
	std::string strResult;
	CString strCMD;
	strCMD.Format(CMD_CHECK_EXIST,L"/sdcard/");

	PostAdbCommand(LPCTSTR(strCMD),strResult);
	std::vector<std::wstring> vecSubPaths = SpliterString(m_strStorageDir,L"/");
	std::vector<std::string> vecFiles = SpliterString(strResult,"\n");
	bool bFound = false;
	for(int i = 0; i < vecFiles.size(); i++)
	{
		if(vecFiles[i] == W2Utf8(vecSubPaths[vecSubPaths.size() - 1].c_str()))
		{
			bFound = true;
			break;
		}
	}
	if(!bFound)
	{
		//Ŀ¼�����ڵĻ�������Ŀ¼
		strCMD.Format(CMD_CREATE_DIR,m_strStorageDir.c_str());
		PostAdbCommand(LPCTSTR(strCMD),strResult,false);
	}
	return m_strStorageDir;
}

void CAndroidHelper::UpdateConnectionState()
{
	SP_DEVINFO_DATA deviceInfo = {0};
	deviceInfo.cbSize = sizeof(SP_DEVINFO_DATA);
	if (SearchPhone(&deviceInfo))
	{
		if (CheckDeviceDriver(GetDeviceInstanceId(m_hDevInfo, deviceInfo).c_str()))
		{
			NotifyConnect(true);
			return;
		}
	}
	NotifyConnect(false);
}

std::string CAndroidHelper::FilterUselessMsg( std::string strResultMsg )
{
	std::string tags = "adb server is out of date.  killing...\n* daemon started successfully *\n";
	int npos = strResultMsg.find(tags.c_str());
	if(npos != std::string::npos)
	{
		npos += tags.size(); 
	}
	if(npos > 0)
	{
		strResultMsg = strResultMsg.substr(npos,strResultMsg.size()-npos);
	}
	return strResultMsg;
}

int CAndroidHelper::RegisterConnectCallback( ConnectCallback callback )
{
	int id = m_mapConnectCallback.size();
	if(callback != NULL)
	{
		m_mapConnectCallback.insert(std::make_pair(id,callback));
	}
	//��ֹandroidhelper֮ǰ��ʼ������֪ͨ����״̬��ʱ������ע��ص������ղ���֪ͨ
	UpdateConnectionState();
	return id;
}

void CAndroidHelper::NotifyConnect( bool bConnect )
{
	m_bDeviceConnected = bConnect;
	for(std::map<int,ConnectCallback>::iterator iter = m_mapConnectCallback.begin();iter != m_mapConnectCallback.end(); iter++)
	{
		if(iter->second)
		{
			iter->second(bConnect);
		}
	}
}

bool CAndroidHelper::UnRegisterConnectCallback( int id )
{
	if(m_mapConnectCallback.find(id) != m_mapConnectCallback.end())
	{
		m_mapConnectCallback.erase(m_mapConnectCallback.find(id));
		return true;
	}
	return false;
}

bool CAndroidHelper::UpdateProgress( const std::string& strMsg, const wchar_t* szCMD )
{
	//��ȡ��ǰ�ٷֱ�
	CStringA strPartialResult = strMsg.c_str();
	int startPos = -1;
	int lastPos = -1;
	int recordEndPos = -1;
	//��ʱ100%�������һ��buffer�е��¶�������,��Ҫ���⴦��
	if(strPartialResult.Find("100%") >= 0)
	{
		NotifyProgress(100.0f,szCMD);
		return true;
	}
	//find last progress record for example: "push copying: 12% has been transfered-"
	do 
	{
		if(lastPos + 1 < strPartialResult.GetLength())
		{
			startPos = strPartialResult.Find("push copying: ",lastPos + 1);
			recordEndPos = strPartialResult.Find("has been transfered -",startPos + 1);
			//must be a complete record
			if(startPos >= 0 && recordEndPos < 0)
			{
				break;
			}
			else if(startPos >= 0)
			{
				lastPos = startPos;
			}
		}
		else
		{
			break;
		}
	} while (startPos >= 0);
	//extract progress number
	if(lastPos >= 0)
	{
		startPos = lastPos;
		startPos += strlen("push copying: ");
		int endPos = strPartialResult.Find("%",startPos);
		strPartialResult = strPartialResult.Mid(startPos,endPos-startPos);
		if(!strPartialResult.IsEmpty())
		{
			float progress = atof(LPCSTR(strPartialResult));
			NotifyProgress(progress,szCMD);
			if(progress >= 100.0)
			{
				return true;
			}
		}
	}
	return false;
}
