// stdafx.h : ���� ��������� ���� ��������� �ʴ�
// ǥ�� �ý��� ���� ���� �� ������Ʈ ���� ���� ������
// ��� �ִ� ���� �����Դϴ�.
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>


#include "ACEHeader.h"
#include <windows.h>
#include "CommonHeader.h"
#include "DBStruct.h"

#include "SFDatabaseProxy.h"
#include "SFDatabaseProxyLocal.h"
#include "SFDatabaseProxyImpl.h"

#ifdef _DEBUG
#pragma comment(lib, "aced.lib")
#else
#pragma comment(lib, "ace.lib")
#endif

#pragma comment(lib, "zlib.lib")
#pragma comment(lib, "liblzf.lib")
#pragma comment(lib, "BaseLayer.lib")
#pragma comment(lib, "DatabaseLayer.lib")
#pragma comment(lib, "fastdb.lib")
#pragma comment(lib, "EngineLayer.lib")