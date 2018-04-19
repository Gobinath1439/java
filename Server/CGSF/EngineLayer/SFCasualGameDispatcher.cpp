#include "stdafx.h"
#include "SFCasualGameDispatcher.h"
#include "SFEngine.h"
#include "SFDatabase.h"

//���� ������ ���� �޼ҵ� ���� �� ������������ ���� ����
SFCasualGameDispatcher::SFCasualGameDispatcher(void)
{
	m_nLogicThreadCnt = 1;
	m_funcBusnessThread = (void*)SFCasualGameDispatcher::BusinessThread;
}


SFCasualGameDispatcher::~SFCasualGameDispatcher(void)
{
}

//��������Ʈ���� ť�� ��Ŷ�� ť���Ѵ�.
void SFCasualGameDispatcher::Dispatch(BasePacket* pPacket)
{
	LogicGatewaySingleton::instance()->PushPacket(pPacket);
}

void SFCasualGameDispatcher::BusinessThread(void* Args)
{
	SFEngine* pEngine = (SFEngine*)Args;
	while (SFEngine::GetInstance()->ServerTerminated() == FALSE)
	{
//��������Ʈ���� ť���� ��Ŷ�� ������.
		BasePacket* pPacket = LogicGatewaySingleton::instance()->PopPacket();
//������Ʈ�� ��ü�� ProcessPacket �޼ҵ带 ȣ���ؼ� ��Ŷ ó���� �����Ѵ�.
		LogicEntrySingleton::instance()->ProcessPacket(pPacket);

//����� ��Ŷ�� �����Ѵ�. ��Ŷ�� Ÿ�Կ� ���� ������ ���°� �ٸ�
		switch (pPacket->GetPacketType())
		{
		case SFPACKET_DATA:
			pEngine->ReleasePacket(pPacket);
			break;
		case SFPACKET_CONNECT:
		case SFPACKET_DISCONNECT:
		case SFPACKET_TIMER:
		case SFPACKET_SHOUTER:
			delete pPacket;
			break;

		case SFPACKET_DB:
			SFDatabase::RecallDBMsg((SFMessage*)pPacket);
			break;

		case SFPACKET_SERVERSHUTDOWN:
			return;

		default:
			SFASSERT(0);
		}
	}
}


