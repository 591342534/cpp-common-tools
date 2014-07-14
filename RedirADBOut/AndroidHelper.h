#pragma once
#include "Singleton.h"
#include <SetupAPI.h>

extern const wchar_t* CMD_GET_BRAND;
extern const wchar_t* CMD_GET_MODEL;
extern const wchar_t* CMD_GET_VERSION;
extern const wchar_t* CMD_GET_IMEI;
extern const wchar_t* CMD_GET_MAC;

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
protected:

	/*
	 *	��ȡ�豸���ID��Ϣ
	 */
	std::wstring GetDeviceRegisterProperty(HDEVINFO hDevInfo,SP_DEVINFO_DATA deviceInfo,DWORD categoryId);

	/*
	 *	ʵ��ID�ǵ��Ը��豸�����ID���ڵ��Զ�I/O�豸�Ĺ���
	 */
	std::wstring GetDeviceInstanceId(HDEVINFO hDevInfo,SP_DEVINFO_DATA deviceInfo);

	/*
	 *	ͨ��adb�����ֻ�
	 *  _T("adb -d shell getprop ro.product.brand"); 		//��ȡ��������
	 *	_T("adb -d shell getprop ro.product.model")			//�豸�ͺ�
	 *	_T("adb -d shell getprop ro.build.version.release")	//android�汾
	 *	_T("adb -d shell dumpsys iphonesubinfo")			//IMEI��
	 *	_T("adb -d shell cat /sys/class/net/wlan0/address")	//MAC��ַ
	 */
	bool PostAdbCommand(const wchar_t* szCMD,std::string& strResult);
private:
	HDEVINFO			m_hDevInfo;
};
