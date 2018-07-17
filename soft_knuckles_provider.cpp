//////////////////////////////////////////////////////////////////////////////
// soft_knuckles_provider.cpp
//
// This is the "main" of the entire driver.  It defines HmdDriverFactory
// below which is the entry point from the vrserver into this shared library.
//
// It also defines SoftKnucklesProvider that waits on a listen socket thread
// thread trigger instantiating the the soft_knuckles_devices.   When it
// does receive a new connection, it creates left and right handed
// soft_knuckles_devices.
//
// While it is possible to instantiate the controllers at startup, 
// sometimes you may want to start other controllers like the vive controller
// earlier so that you can use a tested and real controller and then add the 
// simulated ones in later.
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
#include <openvr_driver.h>
#include "soft_knuckles_device.h"
#include "soft_knuckles_debug_handler.h"
#include "dprintf.h"

using namespace vr;

namespace soft_knuckles
{

static const int NUM_DEVICES = 2;
static const char *listen_address = "127.0.0.1";
static const u_short listen_port = 27015;

class SoftKnucklesProvider : public IServerTrackedDeviceProvider
{
    SoftKnucklesDevice m_knuckles[NUM_DEVICES];
    SoftKnucklesDebugHandler m_debug_handler[NUM_DEVICES];
    thread m_thread;    // a thread to wait to know when to add the new devices;
	SOCKET m_listen_socket;

    static void listen_thread(SoftKnucklesProvider *pthis)
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
            service.sin_addr.s_addr = inet_addr(listen_address);
            service.sin_port = htons(listen_port);

            if (::bind(pthis->m_listen_socket, (sockaddr*)&service, sizeof(service)) == SOCKET_ERROR) {
                dprintf("bind failed with error: %ld\n", LAST_ERROR());
                CLOSE_SOCKET(pthis->m_listen_socket);
				pthis->m_listen_socket = -1;
                return;
            }

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
                    dprintf("new connection on port port: %d\n", listen_port);
                    // add devices
                    for (int i = 0; i < NUM_DEVICES; i++)
                    {
                        vr::VRServerDriverHost()->TrackedDeviceAdded(
                            pthis->m_knuckles[i].get_serial().c_str(),
                            TrackedDeviceClass_Controller,
                            &pthis->m_knuckles[i]);
                    }
                }
                CLOSE_SOCKET(pthis->m_listen_socket);
				pthis->m_listen_socket = -1;
            }
        }
    }

public:
    SoftKnucklesProvider()
		: m_listen_socket(-1)
    {
        dprintf("SoftKnucklesProvider: constructor called\n");
    }
    
    virtual EVRInitError Init(vr::IVRDriverContext *pDriverContext) override
    {
        // NOTE 1: use the driver context.  Sets up a big set of globals
        VR_INIT_SERVER_DRIVER_CONTEXT(pDriverContext);
        dprintf("SoftKnucklesProvider: Init called\n");

        m_knuckles[0].Init(TrackedControllerRole_LeftHand, component_definitions_left, NUM_INPUT_COMPONENT_DEFINITIONS, 
                            &m_debug_handler[0]);

        m_knuckles[1].Init(TrackedControllerRole_RightHand, component_definitions_right, NUM_INPUT_COMPONENT_DEFINITIONS, 
                            &m_debug_handler[1]);

        // start the listening thread after both devices have been initialized
        m_thread = thread(listen_thread, this);

        return VRInitError_None;
    }

    virtual void Cleanup() override
    {
        dprintf("SoftKnucklesProvider: Cleanup\n");
		// close my listener socket
		if (m_listen_socket != -1)
		{
			shutdown(m_listen_socket, SHUTDOWN_BOTH);
			m_listen_socket = -1;
		}
        EnterStandby(); // shuts down threads in devices
        m_thread.join();
    }
    virtual const char * const *GetInterfaceVersions() override
    {
        return vr::k_InterfaceVersions;
    }
    virtual void RunFrame() override
    {
        static int i;
        if (i++ % 10000 == 0)
        {
            dprintf("SoftKnucklesProvider: Run Frame %d\n", i);
        }
    }
    virtual bool ShouldBlockStandbyMode() override
    {
        dprintf("SoftKnucklesProvider: ShouldBlockStandbyMode\n");
        return false;
    }
    virtual void EnterStandby() override
    {
        dprintf("SoftKnucklesProvider: EnterStandby\n");
        m_knuckles[0].Deactivate();
        m_knuckles[1].Deactivate();
    }
    virtual void LeaveStandby() override
    {
        dprintf("SoftKnucklesProvider: LeaveStandby\n");
        m_knuckles[0].Reactivate(); 
        m_knuckles[1].Reactivate();
    }
};
} // end of namespace 

bool g_bExiting = false;
class CWatchdogDriver_Sample : public IVRWatchdogProvider
{
public:
    CWatchdogDriver_Sample()
    {
        m_pWatchdogThread = nullptr;
    }

    virtual EVRInitError Init(vr::IVRDriverContext *pDriverContext);
    virtual void Cleanup();

private:
    thread * m_pWatchdogThread;
};

void WatchdogThreadFunction()
{
    while (!g_bExiting)
    {
#if defined( _WINDOWS )
        // on windows send the event when the Y key is pressed.
        if ((0x01 & GetAsyncKeyState('Y')) != 0)
        {
            // Y key was pressed. 
            vr::VRWatchdogHost()->WatchdogWakeUp();
        }
        this_thread::sleep_for(chrono::microseconds(500));
#else
        // for the other platforms, just send one every five seconds
        this_thread::sleep_for(chrono::seconds(5));
        vr::VRWatchdogHost()->WatchdogWakeUp();
#endif
    }
}

EVRInitError CWatchdogDriver_Sample::Init(vr::IVRDriverContext *pDriverContext)
{
    VR_INIT_WATCHDOG_DRIVER_CONTEXT(pDriverContext);
    dprintf("SoftKnuckles starting watchdog\n");

    // Watchdog mode on Windows starts a thread that listens for the 'Y' key on the keyboard to 
    // be pressed. A real driver should wait for a system button event or something else from the 
    // the hardware that signals that the VR system should start up.
    g_bExiting = false;
    m_pWatchdogThread = new thread(WatchdogThreadFunction);
    if (!m_pWatchdogThread)
    {
        dprintf("Unable to create watchdog thread\n");
        return VRInitError_Driver_Failed;
    }

    return VRInitError_None;
}

void CWatchdogDriver_Sample::Cleanup()
{
    g_bExiting = true;
    if (m_pWatchdogThread)
    {
        m_pWatchdogThread->join();
        delete m_pWatchdogThread;
        m_pWatchdogThread = nullptr;
    }
}

#if defined(_WIN32)
#define HMD_DLL_EXPORT extern "C" __declspec( dllexport )
#define HMD_DLL_IMPORT extern "C" __declspec( dllimport )
#elif defined(__GNUC__) || defined(COMPILER_GCC) || defined(__APPLE__)
#define HMD_DLL_EXPORT extern "C" __attribute__((visibility("default")))
#define HMD_DLL_IMPORT extern "C" 
#else
#error "Unsupported Platform."
#endif

HMD_DLL_EXPORT void *HmdDriverFactory(const char *pInterfaceName, int *pReturnCode)
{
    dprintf("HmdDriverFactory %s\n", pInterfaceName);

    static soft_knuckles::SoftKnucklesProvider s_knuckles_provider; // single instance of the provider
    static CWatchdogDriver_Sample s_watchdogDriverNull; // this is from sample code.

    if (0 == strcmp(IServerTrackedDeviceProvider_Version, pInterfaceName))
    {
        return &s_knuckles_provider;
    }
    if (0 == strcmp(IVRWatchdogProvider_Version, pInterfaceName))
    {
        return &s_watchdogDriverNull;
    }

    return nullptr;
}

