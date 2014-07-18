/********************************************************************
	created:	2014/07/18
	created:	18:7:2014   13:46
	filename: 	AndroidHelper.h
	file path:	
	file base:	AndroidHelper
	file ext:	h
	author:		findeway
	
	purpose:	
*********************************************************************/
#pragma once
#include "Singleton.h"
#include <SetupAPI.h>
#include <boost/function.hpp>
#include <map>

extern const wchar_t* CMD_GET_BRAND;
extern const wchar_t* CMD_GET_MODEL;
extern const wchar_t* CMD_GET_VERSION;
extern const wchar_t* CMD_GET_IMEI;
extern const wchar_t* CMD_GET_MAC;

extern const wchar_t* ERROR_INFO_GETIMEI_FAILED;
extern const wchar_t* ERROR_INFO_GETMODEL_FAILED;
extern const wchar_t* ERROR_INFO_GETVERSION_FAILED;
extern const wchar_t* ERROR_INFO_GETMAC_FAILED;
extern const wchar_t* ERROR_INFO_GETBRAND_FAILED;

typedef boost::function<void(int)> ProgressCallback;
typedef boost::function<void(bool)> ConnectCallback;

class CAndroidHelper:
	public SohuTool::SingletonImpl<CAndroidHelper>
{
public:
	CAndroidHelper(void);
	~CAndroidHelper(void);

	/*
	 *	�յ��豸���֪ͨ
	 */
	void NotifyDeviceChanged(DWORD wParam,DWORD lParam);

	/*
	 *	�����ֻ��豸 
	 */
	bool SearchPhone(SP_DEVINFO_DATA* pDeviceInfo);

	/*
	 *	����豸�����Ƿ����� 
	 */
	bool CheckDeviceDriver(const wchar_t* szInstanceId);

	/*
	 *	��ȡ�ֻ������Ϣ 
	 */
	void GetPhoneInfo();
	
	/*
	 *	���ֻ������ļ�
	 */
	bool PushFile(const wchar_t* szSourcefile, const wchar_t* szDest, ProgressCallback callback);

	/*
	 *	ͨ��adb�����ֻ�
	 *  _T("adb -d shell getprop ro.product.brand"); 		//��ȡ��������
	 *	_T("adb -d shell getprop ro.product.model")			//�豸�ͺ�
	 *	_T("adb -d shell getprop ro.build.version.release")	//android�汾
	 *	_T("adb -d shell dumpsys iphonesubinfo")			//IMEI��
	 *	_T("adb -d shell cat /sys/class/net/wlan0/address")	//MAC��ַ
	 */
	bool PostAdbCommand(const wchar_t* szCMD,std::string& strResult,bool bWaitResult = true);

	/*
	 *	��ȡ�ֻ��洢Ŀ¼
	 */
	std::wstring GetStorageDir();
	
	/*
	 *	ע���ֻ���½״̬�仯�Ļص�����
	 */
	int RegisterConnectCallback(ConnectCallback callback);

	/*
	 *	��ע���ֻ���½״̬�仯�Ļص�����
	 */
	bool UnRegisterConnectCallback(int id);

protected:

	/*
	 *	�����ֻ�����״̬
	 */
	void UpdateConnectionState();
	/*
	 *	��ȡ�豸���ID��Ϣ
	 */
	std::wstring GetDeviceRegisterProperty(HDEVINFO hDevInfo,SP_DEVINFO_DATA deviceInfo,DWORD categoryId);

	/*
	 *	ʵ��ID�ǵ��Ը��豸�����ID���ڵ��Զ�I/O�豸�Ĺ���
	 */
	std::wstring GetDeviceInstanceId(HDEVINFO hDevInfo,SP_DEVINFO_DATA deviceInfo);

	/*
	 *	����adb��������
	 */
	std::string FilterResult(const wchar_t* szCMD, std::string strResultMsg);

	/*
	 * ���˵��������е�������Ϣ�������������	
	 */
	std::string FilterUselessMsg(std::string strResultMsg);

	/*
	 *	��adb�ܵ���ȡ���
	 */
	std::string ReadResponseFromPipe(HANDLE hStdOutRead,const wchar_t* szCMD);

	/*
	 *	�ж��Ƿ��Ǻ�ʱ����
	 */
	bool NeedWaitProcess(const wchar_t* szCMD);

	/*
	 *	֪ͨ��ʱ����ִ�еİٷֱ�
	 */
	void NotifyProgress(float fPercent, const wchar_t* szCMD);

	/*
	 *	֪ͨ�ֻ�����״̬�仯
	 */
	void NotifyConnect(bool bConnect);

	//************************************
	// FullName:  CAndroidHelper::UpdateProgress
	// Access:    protected 
	// Returns:   bool
	// Parameter: const std::string & strMsg
	// Parameter: const wchar_t * szCMD
	// Desc:	  ��ȡ��ʱ�������� called from ReadResponseFromPipe
	// Return:	  true-������� false-δ�������
	//************************************
	bool UpdateProgress(const std::string& strMsg, const wchar_t* szCMD);

	std::map<std::wstring,ProgressCallback>  m_mapProgressCallback;
	std::map<int,ConnectCallback>			 m_mapConnectCallback;
private:
	HDEVINFO			m_hDevInfo;
	bool				m_bDeviceConnected;
	std::wstring		m_strStorageDir;
};
