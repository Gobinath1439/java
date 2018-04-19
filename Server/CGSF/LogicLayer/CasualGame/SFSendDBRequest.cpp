#include "stdafx.h"
#include "SFSendDBRequest.h"
#include "SFPlayer.h"
#include "DBMsg.h"

///////////////////////////////////////////////////////////////////////////////////////
//Internal Function
///////////////////////////////////////////////////////////////////////////////////////


BOOL SFSendDBRequest::SendDBRequest(SFMessage* pMessage)
{
	return SFLogicEntry::GetLogicEntry()->GetDataBaseProxy()->SendDBRequest(pMessage);
}

void SFSendDBRequest::SendToLogic(BasePacket* pMessage)
{
	pMessage->SetPacketType(SFPACKET_DB);
	LogicGatewaySingleton::instance()->PushPacket(pMessage);
}


//////////////////////////////////////////////////////////////////////////////////////////////
//���������� ������Ʈ�� �ۼ��ؾ� �� ��
//////////////////////////////////////////////////////////////////////////////////////////////
BOOL SFSendDBRequest::RequestLogin(SFPlayer* pPlayer)
{
	SFMessage* pMessage = SFDatabase::GetInitMessage(DBMSG_LOGIN, pPlayer->GetSerial());
	*pMessage << (char*)pPlayer->m_username.c_str();
	*pMessage << (char*)pPlayer->m_password.c_str();

	return SendDBRequest(pMessage);
}