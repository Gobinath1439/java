#pragma once
// ============================================================================
// SFMsgPackProtocol Ŭ����
// author : pdpdds
// desc : MessagePack ���������� ó���ϴ� Ŭ����
// SFMsgPackPacket ��Ŷ ��ü�� ������ ����ϸ� ��ŷ�� MessagePack �����͸� ��Ʈ��ũ�� �����Ѵ�.
// �������� Ŭ������ ����� ���Ͽ� 1:1 ������ �Ѵ�.
// ============================================================================

#include <msgpack.hpp>

class SFMsgPackPacket;

class SFMsgPackProtocol
{
public:
	SFMsgPackProtocol();
	virtual ~SFMsgPackProtocol();

	bool Initialize(int ioBufferSize, USHORT packetDataSize);

	// ----------------------------------------------------------------
	//  Name:           Reset
	//  Description:    �������� ��ü�� �� Ȱ���ϱ� ���� ��Ŷ IO ���۸� �ʱ�ȭ�Ѵ�.	
	// ----------------------------------------------------------------
	bool Reset();

	// ----------------------------------------------------------------
	//  Name:           AddTransferredData
	//  Description:    ������ ������ �����͸� ��Ŷ IO ���ۿ� �����Ѵ�.	
	// ----------------------------------------------------------------
	bool AddTransferredData(char* pBuffer, DWORD dwTransferred);

	// ----------------------------------------------------------------
	//  Name:           GetPacket
	//  Description:    ������ ������ �����͸� �̿��ؼ� SFMsgPackPacket ��Ŷ ��ü�� �����Ѵ�.
	//					�ش� ��Ŷ ��ü�� ���� ������ ���۵ȴ�.
	// ----------------------------------------------------------------
	BasePacket* GetPacket(int& errorCode);

	// ----------------------------------------------------------------
	//  Name:           SendRequest
	//  Description:    MessagePack �������ݷ� ��ŷ�� SFMsgPackPacket ��ü�� ��Ʈ��ũ�� �����Ѵ�.	
	// ----------------------------------------------------------------
	bool SendRequest(BasePacket* pPacket);

	// ----------------------------------------------------------------
	//  Name:           DisposePacket
	//  Description:    ��Ŷ�� �����Ѵ�.
	// ----------------------------------------------------------------
	bool DisposePacket(BasePacket* pPacket);

	// ----------------------------------------------------------------
	//  Name:           CreatePacket
	//  Description:    ������ SFMsgPackPacket ��ü�� ���´�.
	// ----------------------------------------------------------------
	BasePacket* CreatePacket();

	// ----------------------------------------------------------------
	//  Name:           GetPacketData
	//  Description:    �������� �޼ҵ�. �ϴ� ������ true�� ��ȯ�ϵ��� ������ ���´�.
	// ----------------------------------------------------------------
	bool GetPacketData(BasePacket* pPacket, char* buffer, const int BufferSize, unsigned int& writtenSize);

protected:

private:
	SFPacketIOBuffer* m_pPacketIOBuffer;
};

