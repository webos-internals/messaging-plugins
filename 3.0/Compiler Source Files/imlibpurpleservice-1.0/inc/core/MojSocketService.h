/* ============================================================
 * Date  : July 8, 2009
 * Copyright 2009 Palm, Inc. All rights reserved.
 * ============================================================ */

#ifndef MOJSOCKETSERVICE_H_
#define MOJSOCKETSERVICE_H_

#include "core/MojCoreDefs.h"
#include "core/MojService.h"
#include "core/MojDataSerialization.h"
#include "core/MojSet.h"
#include "core/MojSock.h"
#include "core/MojSocketEncoding.h"
#include "core/MojReactor.h"

class MojSocketService : public MojService
{
public:
	MojSocketService(MojReactor& reactor);
	virtual ~MojSocketService();

	virtual MojErr configure(MojObject& conf);
	virtual MojErr open(const MojChar* serviceName);
	virtual MojErr close();

	virtual MojErr createRequest(MojRefCountedPtr<MojServiceMessage>& msgOut);
	virtual MojErr dispatch(); // for testing only

	MojReactor& reactor() { return m_reactor; }

private:
	class Message : public MojServiceMessage
	{
	public:
	};

	class SockHandler : public MojSignalHandler
	{
	public:
		virtual MojErr close();

	protected:
		SockHandler(MojSocketService& service);
		MojReactor& reactor() { return m_service.reactor(); }

		MojSock m_sock;
		MojSocketService& m_service;
	};

	class Connection : public MojSignalHandler
	{
	public:
		Connection(MojSocketService& service);
		~Connection();

		MojErr open(MojSockT sock);
		MojErr open(const MojChar* path);
		MojErr open(const MojChar* host, MojInt32 port);

		MojErr writeRequest(Message* msg);
		MojErr writeResponse(Message* msg);
		MojErr writeCancel(Message* msg);

	private:
		static const MojSize IoVecSize = 16;

		enum MsgType {
			TypeOpen = 0,
			TypeClose,
			TypeKeepAlive,
			TypeKeepAliveResponse,
			TypeRequest,
			TypeResponse,
			TypeFinalResponse,
			TypeCancel
		};

		enum State {
			StateHeaderSize,
			StateHeader,
			StatePayload,
			StateInit = StateHeaderSize
		};

		MojErr start();
		MojErr writeOpen();
		MojErr writeClose();
		MojErr readMsg();
		MojErr writeMsg();
		MojErr flush(MojSockT sock);
		MojErr read(MojSockT sock);
		MojErr handleRead();
		MojErr readHeaderSize();
		MojErr readHeader();
		MojErr readPayload();

		State m_state;
		MojBuffer m_readBuf;
		MojBuffer m_writeBuf;
		MojDataWriter m_writer;
		MojReactor::SockSignal::Slot<Connection> m_readSlot;
		MojReactor::SockSignal::Slot<Connection> m_flushSlot;
		MojReactor::SockSignal::Slot<Connection> m_connectSlot;
	};

	class Server : public MojSignalHandler
	{
	public:
		Server(MojSocketService& service);
		~Server();

		MojErr listen(const MojChar* path);
		MojErr listen(const MojChar* host, MojInt32 port);
		MojErr listen(MojSockAddrT* addr, MojSize addrSize);

	private:
		static const MojInt32 ListenBacklog = 1024;

		MojErr accept(MojSockT sock);

		MojReactor::SockSignal::Slot<Server> m_acceptSlot;
	};

	typedef MojRefCountedPtr<Connection> ConnectionPtr;
	typedef MojHashMap<MojServiceMessage::Token, ConnectionPtr> RequestMap;
	typedef MojHashMap<MojString, ConnectionPtr> ConMap;

	MojErr parseUri(const MojChar* uri, MojString& socknameOut, MojString& categoryOut, MojString& methodOut);
	MojErr getCon(const MojString& sockName, MojSockT& sockOut);
	MojErr writeToSocket(const MojSocketMessage& msg, MojSockT sock);
	MojErr dispatchFromSocket(MojSockT sock);
	MojErr handleInput(MojSockT sock);
	MojErr handleOutput(MojSockT sock);
	MojErr handleConnectionClose(MojSockT sock);
	MojErr acceptConnection();
	MojErr flush(MojSockT sock);

	ConMap m_conMap;
	RequestMap m_activeRequests;
	MojInt64 m_nextToken;
	MojSockT m_listenSocket;
	MojReactor& m_reactor;
};

#endif /* MOJSOCKETSERVICE_H_ */

