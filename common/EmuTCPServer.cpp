



#include "debug.h"
#include "EmuTCPServer.h"
#include "EmuTCPConnection.h"

EmuTCPServer::EmuTCPServer(int16 iPort, bool iOldFormat)
: TCPServer<EmuTCPConnection>(iPort),
  pOldFormat(iOldFormat)
{
}

EmuTCPServer::~EmuTCPServer() {
	MInQueue.lock();
	while(!m_InQueue.empty()) {
		delete m_InQueue.front();
		m_InQueue.pop();
	}
	MInQueue.unlock();
}

void EmuTCPServer::Process() {
	CheckInQueue();
	TCPServer<EmuTCPConnection>::Process();
}

void EmuTCPServer::CreateNewConnection(int32 ID, SOCKET in_socket, int32 irIP, int16 irPort)
{
	EmuTCPConnection *conn = new EmuTCPConnection(ID, this, in_socket, irIP, irPort, pOldFormat);
	AddConnection(conn);
}


void EmuTCPServer::SendPacket(ServerPacket* pack) {
	EmuTCPNetPacket_Struct* tnps = EmuTCPConnection::MakePacket(pack);
	SendPacket(&tnps);
}

void EmuTCPServer::SendPacket(EmuTCPNetPacket_Struct** tnps) {
	MInQueue.lock();
	m_InQueue.push(*tnps);
	MInQueue.unlock();
	tnps = NULL;
}

void EmuTCPServer::CheckInQueue() {
	EmuTCPNetPacket_Struct* tnps = 0;

	while (( tnps = InQueuePop() )) {
		vitr cur, end;
		cur = m_list.begin();
		end = m_list.end();
		for(; cur != end; cur++) {
			if ((*cur)->GetMode() != EmuTCPConnection::modeConsole && (*cur)->GetRemoteID() == 0)
				(*cur)->SendPacket(tnps);
		}
		safe_delete(tnps);
	}
}

EmuTCPNetPacket_Struct* EmuTCPServer::InQueuePop() {
	EmuTCPNetPacket_Struct* ret = NULL;
	MInQueue.lock();
	if(!m_InQueue.empty()) {
		ret = m_InQueue.front();
		m_InQueue.pop();
	}
	MInQueue.unlock();
	return ret;
}


EmuTCPConnection *EmuTCPServer::FindConnection(int32 iID) {
	vitr cur, end;
	cur = m_list.begin();
	end = m_list.end();
	for(; cur != end; cur++) {
		if ((*cur)->GetID() == iID)
			return *cur;
	}
	return(NULL);
}












