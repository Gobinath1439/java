// EchoClient.cpp : �ܼ� ���� ���α׷��� ���� �������� �����մϴ�.
//

#include "stdafx.h"
#include "EchoCallback.h"
#include "SFNetworkEntry.h"
#include "SFPacketProtocol.h"
#include "SFJsonProtocol.h"
#include "SFJsonPacket.h"
#include <string>
#include <iostream>

#pragma comment(lib, "EngineLayer.lib")

#define ECHO_PACKET_ID 1000

void EchoInputThread(void* Args)
{
	std::string input;

	while (SFNetworkEntry::GetInstance()->IsConnected())
	{
		std::cin >> input;

		if(input.compare("exit") == 0)
			break;

		SFJsonPacket packet(ECHO_PACKET_ID);
		packet.GetData().Add("ECHO", input.c_str());

		SFNetworkEntry::GetInstance()->TCPSend(&packet);
	}
}

void ProcessInput()
{
	int inputThreadID = ACE_Thread_Manager::instance()->spawn_n(1, (ACE_THR_FUNC)EchoInputThread, NULL, THR_NEW_LWP, ACE_DEFAULT_THREAD_PRIORITY);

	SFASSERT(inputThreadID != -1);

	while (SFNetworkEntry::GetInstance()->IsConnected())
	{
		SFNetworkEntry::GetInstance()->Update();

		Sleep(1);
	}

	ACE_Thread_Manager::instance()->wait_grp(inputThreadID);
}

int _tmain(int argc, _TCHAR* argv[])
{
	EchoCallback* pCallback = new EchoCallback();

	SFNetworkEntry::GetInstance()->Initialize(pCallback, new SFPacketProtocol<SFJsonProtocol>);
	SFNetworkEntry::GetInstance()->Run();

	ProcessInput();

	SFNetworkEntry::GetInstance()->ShutDown();

	return 0;
}