#pragma once
//���� ǥ���� Ȯ��
//VS2008 SP1���� ����

class SFRegexChecker
{
public:
	SFRegexChecker(void);
	virtual ~SFRegexChecker(void);

	BOOL IsValidCharName(TCHAR* szStr);
	BOOL IsValidResidentRegistrationNumber(TCHAR* szStr);
	BOOL IsValidURL(TCHAR* szStr);
	BOOL IsValidMacAddress(TCHAR* szStr);
	BOOL IsValidEMail(TCHAR* szStr);
	BOOL IsValidIPAddress( TCHAR* szStr );
protected:

private:
};
