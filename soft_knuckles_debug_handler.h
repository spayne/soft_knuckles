//////////////////////////////////////////////////////////////////////////////
// soft_knuckles_debug_handler.h
//
// The "soft" in soft_knuckles is to be able to use software
// to simulate a knuckles controller.   To simulate a knuckles controller, the
// debug_handler is is registered on each soft_knuckles device and receives
// requests to change input component states.
//
// This module uses the DebugRequest mechanism provided by 
// ITrackedDeviceServerDriver and uses the input configuration provided by
// soft_knuckles_config.h
//
// See soft_knuckles_debug_client.cpp for an example client.
//
#include <openvr_driver.h>
#include <unordered_map>

class SoftKnucklesDevice;

namespace soft_knuckles
{
    class SoftKnucklesDebugHandler
    {
        SoftKnucklesDevice *m_device;
        std::unordered_map<std::string, uint32_t> m_inputstring2index;

    public:
        SoftKnucklesDebugHandler();
        void Init(SoftKnucklesDevice *);

        void DebugRequest(const char *pchRequest, char *pchResponseBuffer, uint32_t unResponseBufferSize);

    private:
        void InitializeLookupTable();
        void SetPosition(double x, double y, double z);

    };
};
