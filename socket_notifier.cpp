//////////////////////////////////////////////////////////////////////////////
// socket_notifier
//	* hide ipc and cross platform stuff 
//
#if defined(_WIN32)
#include <io.h>
#include <tchar.h>
#include <windows.h>
#define LAST_ERROR() WSAGetLastError() 
#define CLOSE_SOCKET(x) closesocket(x)
#define SHUTDOWN_BOTH 2
#else
#define SOCKET int
#define INVALID_SOCKET (-1) 
#define SOCKET_ERROR (-1) 
#define CLOSE_SOCKET(x) close(x)
#define LAST_ERROR() errno 
#define SHUTDOWN_BOTH SHUT_RDWR
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unistd.h>
#endif

#include <thread>
#include <mutex>
#include <string.h>
#include <vector>
#include <string>
#include "dprintf.h"

#include "socket_notifier.h"

using namespace std;

class SocketNotifierImpl
{
private:
	SocketNotifier *m_who_to_notify;

	SOCKET m_listen_socket;
	string m_listen_address;
	u_short m_listen_port;
	thread m_listen_thread;    // a thread to wait to know when to add the new devices;
	
	static void listen_thread(SocketNotifierImpl *pthis);
public:
	SocketNotifierImpl(SocketNotifier *who_to_notify);
	~SocketNotifierImpl();
	void StartListening(const char *listen_address, u_short listen_port);
	void StopListening();
};

SocketNotifierImpl::SocketNotifierImpl(SocketNotifier *who_to_notify)
	:	m_who_to_notify(who_to_notify),
		m_listen_socket(-1)
{
}

SocketNotifierImpl::~SocketNotifierImpl()
{
	StopListening();
}

void SocketNotifierImpl::StartListening(const char *listen_address, u_short listen_port)
{
	m_listen_address = listen_address;
	m_listen_port = listen_port;
	m_listen_thread = thread(listen_thread, this);
}


void SocketNotifierImpl::listen_thread(SocketNotifierImpl *pthis)
{
#ifdef _WIN32
	HRESULT hr = SetThreadDescription(GetCurrentThread(), L"soft knuckles listen to activate thread");
#endif
	dprintf("listen thread started\n");
	pthis->m_listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (pthis->m_listen_socket == INVALID_SOCKET) {
		dprintf("socket failed with error: %ld\n", LAST_ERROR());
		return;
	}
	else
	{
		sockaddr_in service;
		memset(&service, 0, sizeof(service));
		service.sin_family = AF_INET;
		service.sin_addr.s_addr = inet_addr(pthis->m_listen_address.c_str());
		service.sin_port = htons(pthis->m_listen_port);

		if (::bind(pthis->m_listen_socket, (sockaddr*)&service, sizeof(service)) == SOCKET_ERROR) {
			dprintf("bind failed with error: %ld\n", LAST_ERROR());
			CLOSE_SOCKET(pthis->m_listen_socket);
			pthis->m_listen_socket = -1;
			return;
		}
		dprintf("about to listen\n");
		if (listen(pthis->m_listen_socket, 1) == SOCKET_ERROR) {
			dprintf("listen failed with error: %ld\n", LAST_ERROR());
			CLOSE_SOCKET(pthis->m_listen_socket);
			pthis->m_listen_socket = -1;
		}
		else
		{
			SOCKET incoming = accept(pthis->m_listen_socket, nullptr, nullptr);
			if (0 > incoming)
			{
				dprintf("accept failed with error: %ld\n", LAST_ERROR());
			}
			else
			{
				CLOSE_SOCKET(incoming);
				if (CLOSE_SOCKET(pthis->m_listen_socket) < 0)
				{
					dprintf("close failed - must be shutting down\n");
				}
				else
				{
					dprintf("new connection from port: %d\n", pthis->m_listen_port);
					if (pthis->m_who_to_notify)
					{
						pthis->m_who_to_notify->Notify();
					}
				}
			}
		}
	}
}

void SocketNotifierImpl::StopListening()
{
	// close my listener socket
	if (m_listen_socket != -1)
	{
		//shutdown(m_listen_socket, SHUTDOWN_BOTH);
		CLOSE_SOCKET(m_listen_socket);
		m_listen_socket = -1;
		if (m_listen_thread.joinable())
		{
			m_listen_thread.join();
		}
	}
}

SocketNotifier::SocketNotifier()
	: m_impl(new SocketNotifierImpl(this))
{}

SocketNotifier::~SocketNotifier()
{
	delete m_impl;
}

void SocketNotifier::StartListening(const char *listen_address, u_short listen_port)
{
	m_impl->StartListening(listen_address, listen_port);
}

void SocketNotifier::StopListening()
{
	m_impl->StopListening();
}

