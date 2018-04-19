#include "stdafx.h"
#include "SFCGSFPacketProtocol.h"
#include "SFCompressor.h"
#include <EngineInterface/ISession.h>
#include "SFEngine.h"

SFCGSFPacketProtocol::SFCGSFPacketProtocol(void)
{
	
}

SFCGSFPacketProtocol::~SFCGSFPacketProtocol(void)
{
	if(m_pPacketIOBuffer)
		delete m_pPacketIOBuffer;

	m_pPacketIOBuffer = NULL;
}

bool SFCGSFPacketProtocol::Initialize(int ioBufferSize, USHORT packetSize)
{
	m_pPacketIOBuffer = new SFPacketIOBuffer();
	m_pPacketIOBuffer->AllocIOBuf(ioBufferSize);

	SFPacket::SetMaxPacketSize(packetSize);

	return true;
}

BasePacket* SFCGSFPacketProtocol::GetPacket(int& errorCode)
{
	SFPacket* pPacket = PacketPoolSingleton::instance()->Alloc();
	pPacket->Initialize();

	if (FALSE == m_pPacketIOBuffer->GetPacket(*pPacket->GetHeader(), (char*)pPacket->GetData(), errorCode))
	{
		PacketPoolSingleton::instance()->Release(pPacket);
		return NULL;
	}

	if (FALSE == pPacket->Decode(errorCode))
	{
		PacketPoolSingleton::instance()->Release(pPacket);
		return NULL;
	}

	return pPacket;
}

bool SFCGSFPacketProtocol::AddTransferredData(char* pBuffer, DWORD dwTransferred)
{
	m_pPacketIOBuffer->AppendData(pBuffer, dwTransferred);

	return true;
}

bool SFCGSFPacketProtocol::Reset()
{
	m_pPacketIOBuffer->InitIOBuf();

	return true;
}

bool SFCGSFPacketProtocol::SendRequest(BasePacket* pPacket)
{
	SFPacket* pSFPacket = (SFPacket*)pPacket;
	pSFPacket->Encode();

	SFEngine::GetInstance()->SendInternal(pSFPacket->GetOwnerSerial(), (char*)pSFPacket->GetHeader(), pSFPacket->GetPacketSize());
	
	return TRUE;
}

bool SFCGSFPacketProtocol::DisposePacket(BasePacket* pPacket)
{
	SFPacket* pSFPacket = static_cast<SFPacket*>(pPacket);

	SFASSERT(pSFPacket != NULL);
	return PacketPoolSingleton::instance()->Release(pSFPacket);
}

bool SFCGSFPacketProtocol::GetPacketData(BasePacket* pPacket, char* buffer, const int bufferSize, unsigned int& writtenSize)
{
	writtenSize = 0;

	SFPacket* pSFPacket = (SFPacket*)pPacket;

	if (pSFPacket->GetPacketSize() == 0)
	{
		return true;
	}

	if (pSFPacket->GetPacketSize() > bufferSize)
	{
		SFASSERT(0);
		return false;
	}

	pSFPacket->Encode();

	memcpy(buffer, pSFPacket->GetHeader(), pSFPacket->GetPacketSize());
	writtenSize = pSFPacket->GetPacketSize();

	return true;
}