#pragma once
// ============================================================================
// SFPacketProtocol Ŭ����
// author : pdpdds
// desc : ������ ������ ��Ʈ��ũ �����͸� �����ϰ� ��Ŷ�� ���� �� ó��, �׸��� �������� ��Ŷ�� �����ϴ� ������ ����ϴ� ���ø� Ŭ�����Դϴ�.
// ���� �������� ó���� ���ø����� ������ Ŭ������ ����մϴ�.
// ============================================================================

#include <EngineInterface/IPacketProtocol.h>
#include "SFConstant.h"
#include "SFChecksum.h"
#include "SFPacketIOBuffer.h"
#include "SFEncryption.h"
#include "SFEncryptionXOR.h"
#include "SFBaseProtocol.h"

#include "Macro.h"

class BasePacket;

void SendLogicLayer(BasePacket* pPacket);

template <typename T>
class SFPacketProtocol : public IPacketProtocol, public SFBaseProtocol 
{
public:
	SFPacketProtocol();
	SFPacketProtocol(int bufferIOSize, USHORT packetDataSize);
	virtual ~SFPacketProtocol(void){}

	// ----------------------------------------------------------------
	//  Name:           OnReceive
	//  Description:    ������ ������ �����͸� ó���Ѵ�. 	
	// ----------------------------------------------------------------
	bool OnReceive(int serial, char* pBuffer, unsigned int dwTransferred) override;
	
	// ----------------------------------------------------------------
	//  Name:           SendRequest
	//  Description:    Ÿ�ٿ��� ��Ŷ�� �����Ѵ�.
	// ----------------------------------------------------------------
	virtual bool SendRequest(BasePacket* pPacket) override;	

	// ----------------------------------------------------------------
	//  Name:           DisposePacket
	//  Description:    ���� ������ ��Ŷ�� �����Ѵ�.
	// ----------------------------------------------------------------
	virtual bool DisposePacket(BasePacket* pPacket) override;

	// ----------------------------------------------------------------
	//  Name:           Clone
	//  Description:    ��Ŷ�������� Ŭ���� ��ü�� �����Ѵ�.
	// ----------------------------------------------------------------
	virtual IPacketProtocol* Clone(){ return new SFPacketProtocol<T>(); }
	
	// ----------------------------------------------------------------
	//  Name:           GetPacketData
	//  Description:    ���������尡 ��Ŷ ���ڵ��� ���� �ʵ��� ��Ŷ�� �����͸� �̾Ƴ��� �޼ҵ��̴�.
	//					���� �������� �޼ҵ���
	// ----------------------------------------------------------------
	virtual bool GetPacketData(BasePacket* pPacket, char* buffer, const int BufferSize, unsigned int& writtenSize) override;

	//virtual BasePacket* CreatePacket() override;

private:

	// ----------------------------------------------------------------
	//  Name:           ���ø� Ŭ����
	//  Description:    ���� ��Ŷ ���������� ó���ϴ� Ŭ����
	//					AddTransferredData, DisposePacket, SendRequest GetPacket �޼ҵ带 �ݵ�� �����ؾ� �ȴ�.
	//					GetPacketData �޼ҵ�� �� �޼ҵ�� ����� �ε��� �Ѵ�.
	// ----------------------------------------------------------------
	T m_Analyzer;	
};

template <typename T>
SFPacketProtocol<T>::SFPacketProtocol()
{
	m_Analyzer.Initialize(m_ioSize, m_packetSize);
}

template <typename T>
SFPacketProtocol<T>::SFPacketProtocol(int bufferIOSize, USHORT packetDataSize)
{
	if (bufferIOSize > MAX_IO_SIZE)
		bufferIOSize = MAX_IO_SIZE;

	if (packetDataSize > MAX_PACKET_SIZE)
		packetDataSize = MAX_PACKET_SIZE;

	if (packetDataSize > bufferIOSize)
		packetDataSize = bufferIOSize;

	m_ioSize = bufferIOSize;
	m_packetSize = packetDataSize;

	m_Analyzer.Initialize(m_ioSize, m_packetSize);
}

template <typename T>
bool SFPacketProtocol<T>::DisposePacket(BasePacket* pPacket)
{
	return m_Analyzer.DisposePacket(pPacket);
}

template <typename T>
bool SFPacketProtocol<T>::GetPacketData(BasePacket* pPacket, char* buffer, const int BufferSize, unsigned int& writtenSize)
{
	return m_Analyzer.GetPacketData(pPacket, buffer, BufferSize, writtenSize);
}

template <typename T>
bool SFPacketProtocol<T>::SendRequest(BasePacket* pPacket)
{
	return m_Analyzer.SendRequest(pPacket);
}

template <typename T>
bool SFPacketProtocol<T>::OnReceive(int Serial, char* pBuffer, unsigned int dwTransferred)
{
	bool bRet = m_Analyzer.AddTransferredData(pBuffer, dwTransferred);

	if(false == bRet)
	{
		SFASSERT(0);
		return false;
	}

	int iErrorCode = PACKETIO_ERROR_NONE;
	
	do
	{
		BasePacket* pPacket = m_Analyzer.GetPacket(iErrorCode);
		
		if(NULL == pPacket)
			break;

		pPacket->SetPacketType(SFPACKET_DATA);
		pPacket->SetOwnerSerial(Serial);
		
		SendLogicLayer(pPacket);
	}
	while(true);
	
	if(iErrorCode != PACKETIO_ERROR_NONE)
	{
		return false;
	}

	return true;
}

/*
template <typename T>
BasePacket* SFPacketProtocol<T>::CreatePacket()
{
return m_Analyzer.CreatePacket();
}*/