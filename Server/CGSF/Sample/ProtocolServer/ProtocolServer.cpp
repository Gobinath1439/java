// ProtocolServer.cpp : �ܼ� ���� ���α׷��� ���� �������� �����մϴ�.
//

#include "stdafx.h"
#include "ProtocolLogicEntry.h"
#include "SFCGSFPacketProtocol.h"
#include "ProtocolCGSFHandler.h"
#include "ProtocolProtobufHandler.h"
#include "ProtocolServerProtocol.h"
#include "SFMsgPackProtocol.h"
#include "ProtocolMsgPackHandler.h"

#pragma comment(lib, "EngineLayer.lib")

int _tmain(int argc, _TCHAR* argv[])
{
	ProtocolLogicEntry<ProtocolCGSFHandler>* pLogicEntry = new ProtocolLogicEntry<ProtocolCGSFHandler>();
	
	SFBaseProtocol::SetPacketOption(0);
	//SFBaseProtocol::SetPacketOption(CGSF_PACKET_OPTION);
	SFPacketProtocol<SFCGSFPacketProtocol>* pProtocol = new SFPacketProtocol<SFCGSFPacketProtocol>(MAX_IO_SIZE, MAX_PACKET_SIZE);
	
	SFEngine::GetInstance()->Intialize(pLogicEntry, pProtocol);


	//ProtocolLogicEntry<ProtocolProtobufHandler>* pLogicEntry = new ProtocolLogicEntry<ProtocolProtobufHandler>();
	//SFEngine::GetInstance()->Intialize(pLogicEntry, new SFPacketProtocol<ProtocolServerProtocol>);

	//ProtocolLogicEntry<ProtocolMsgPackHandler>* pLogicEntry = new ProtocolLogicEntry<ProtocolMsgPackHandler>();
	//SFEngine::GetInstance()->Intialize(pLogicEntry, new SFPacketProtocol<SFMsgPackProtocol>(MAX_IO_SIZE, MAX_PACKET_DATA));

	SFEngine::GetInstance()->Start();

	google::FlushLogFiles(google::GLOG_INFO);

	getchar();

	SFEngine::GetInstance()->ShutDown();

	return 0;
}