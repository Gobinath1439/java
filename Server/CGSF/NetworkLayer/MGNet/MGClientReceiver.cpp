#include "MGClientReceiver.h"
#include <EngineInterface/INetworkEngine.h>
#include <EngineInterface/IEngine.h>
#include "MGEngine.h"


MGClientReceiver::MGClientReceiver(INetworkEngine* pOwner)
	: m_pOwner(pOwner)
{
}


MGClientReceiver::~MGClientReceiver(void)
{
}

void MGClientReceiver::notifyRegisterSocket(ASSOCKDESCEX& sockdesc, SOCKADDR_IN& ip)
{
	/*printf(" Connected %d\n", sockdesc.assockUid);

	m_pOwner->GetCallback()->OnConnect(sockdesc.assockUid);

	Synchronized es(&m_SessionLock);
	m_SessionMap.insert(std::make_pair(sockdesc.assockUid, sockdesc));*/
}

void MGClientReceiver::notifyReleaseSocket(ASSOCKDESCEX& sockdesc)
{
	printf("Disconnected %d\n", sockdesc.assockUid);

	ISession::OnDisconnect(sockdesc.assockUid);

	Synchronized es(&m_SessionLock);
           
	m_SessionMap.erase(sockdesc.assockUid);

	sockdesc.psender->releaseSocketUniqueId(sockdesc.assockUid);
}

void MGClientReceiver::notifyMessage(ASSOCKDESCEX& sockdesc, size_t length, char* data)
{
	if(false == ISession::OnReceive(data, length))
	{
		Synchronized es(&m_SessionLock);
           
		m_SessionMap.erase(sockdesc.assockUid);

		sockdesc.psender->releaseSocketUniqueId(sockdesc.assockUid);
	}
}

void MGClientReceiver::notifyConnectingResult(INT32 requestID, ASSOCKDESCEX& sockdesc, DWORD error)
{
	if(error == 0)
	{
		printf(" Connected %d\n", sockdesc.assockUid);

		ISession::OnConnect(sockdesc.assockUid);

		Synchronized es(&m_SessionLock);
		m_SessionMap.insert(std::make_pair(sockdesc.assockUid, sockdesc));
	}
	
}

void MGClientReceiver::SendInternal(char* pBuffer, int BufferSize, int ownerSerial)
{
	Synchronized es(&m_SessionLock);

	SessionMap::iterator iter = m_SessionMap.find(ownerSerial);

	if(iter == m_SessionMap.end())
	{
		return;
	}

	iter->second.psender->postingSend(iter->second, BufferSize, pBuffer);
}

////////////////////////////////////////////////////////
//����� �����ϴ��� Ȯ���� �� �ʿ䰡 �ִ�...
////////////////////////////////////////////////////////
bool MGClientReceiver::Disconnect(int Serial)
{
	Synchronized es(&m_SessionLock);

	SessionMap::iterator iter = m_SessionMap.find(Serial);

	if(iter == m_SessionMap.end())
	{
		return FALSE;
	}

	iter->second.psender->releaseSocketUniqueId(Serial);
           
	m_SessionMap.erase(Serial);

	return true;

}