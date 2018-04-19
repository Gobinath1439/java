#pragma once
// ============================================================================
// SFMsgPackPacket Ŭ����
// author : pdpdds
// desc : MessagePack ���������� ���� ��Ŷ Ŭ����
// ============================================================================

#include <msgpack.hpp>
#include "BasePacket.h"

class SFMsgPackPacket : public BasePacket
{
	friend class SFMsgPackProtocol;

public:
	SFMsgPackPacket(USHORT usPacketId);
	virtual ~SFMsgPackPacket();

	SFPacketHeader* GetHeader() { return &m_packetHeader; }
	msgpack::unpacker&	GetData(){ return m_unpacker; }

protected:

private:

	SFMsgPackPacket();

	// ��Ŷ ���: SFPacketHeader(12����Ʈ)
	// ��Ŷ ���̵�(2����Ʈ) + ��Ŷ �ɼ�(4����Ʈ) + CRC(4����Ʈ) + ������ ������(2����Ʈ)	
	SFPacketHeader m_packetHeader;

	// ��Ŷ ������: msgpack::unpacker
	msgpack::unpacker m_unpacker;
};