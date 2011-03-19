/* ============================================================
 * Date  : Oct 12, 2009
 * Copyright 2009 Palm, Inc. All rights reserved.
 * ============================================================ */

#ifndef MOJSOCK_H_
#define MOJSOCK_H_

#include "core/MojCoreDefs.h"

class MojSockAddr
{
public:
	MojSockAddr();
	MojSockAddr(MojSockAddrT* addr, MojSockLenT size) { fromAddr(addr, size); }

	void fromAddr(MojSockAddrT* addr, MojSockLenT size);
	MojErr fromPath(const MojChar* path);
	MojErr fromHostPort(const MojChar* host, MojUInt32 port);

	MojSockLenT size() const { return m_size; }
	const MojSockAddrT* impl() const { return &m_u.m_sa; }
	MojSockAddrT* impl() { return &m_u.m_sa; }

private:
	union {
		MojSockAddrT m_sa;
		MojSockAddrUnT m_un;
		MojSockAddrInT m_in;
	} m_u;
	MojSockLenT m_size;
};

class MojSock
{
public:
	MojSock(MojSockT sock = MojInvalidSock) : m_sock(sock) {}
	~MojSock() { (void) close(); }

	void open(MojSockT sock) { MojAssert(m_sock == MojInvalidSock); m_sock = sock; }
	MojErr open(int domain, int type, int protocol = 0) { MojAssert(m_sock == MojInvalidSock); return MojSockOpen(m_sock, domain, type, protocol); }
	MojErr close();

	MojErr setNonblocking(bool val) { return MojSockSetNonblocking(m_sock, val); }
	MojErr accept(MojSock& sockOut, MojSockAddr* addrOut = NULL) { return MojSockAccept(m_sock, sockOut, addrOut ? addrOut->impl() : NULL); }
	MojErr bind(const MojSockAddr& addr) { return MojSockBind(m_sock, addr.impl(), addr.size()); }
	MojErr connect(const MojSockAddr& addr) { return MojSockConnect(m_sock, addr.impl(), addr.size()); }
	MojErr listen(MojSockT sock, int backlog) { return MojSockListen(m_sock, backlog); }
	MojErr read(void* buf, MojSize bufSize, MojSize& sizeOut, int flags = 0) { return MojSockRecv(m_sock, buf, bufSize, sizeOut, flags); }
	MojErr write(const void* data, MojSize size, MojSize& sizeOut, int flags = 0) { return MojSockSend(m_sock, data, size, sizeOut, flags); }

	MojSockT impl() const { return m_sock; }
	MojSockT& impl() { return m_sock; }

	operator MojSockT() const { return impl(); }
	operator MojSockT&() { return impl(); }

private:
	MojSockT m_sock;
};

#endif /* MOJSOCK_H_ */
