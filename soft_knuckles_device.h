//////////////////////////////////////////////////////////////////////////////
// soft_knuckles_device.h
//
// Implements the ITrackedDeviceServerDriver to emulate a single knuckles
// controller.  The handedness of the controller is passed into the Init
// function.
//
// It uses it's own thread to continually send pose updates to the vrsystem.
// It uses soft_knuckles_config to define the input configuration.
//
#include <openvr_driver.h>
#include <thread>
#include <atomic>
#include "soft_knuckles_config.h"

using namespace vr;
using namespace std;

namespace soft_knuckles {

    class SoftKnucklesDebugHandler;

    class SoftKnucklesDevice : public ITrackedDeviceServerDriver
    {
        friend class SoftKnucklesDebugHandler;

        uint32_t m_id;
        bool m_activated;
        vr::IVRDriverContext *m_driver_context;
        PropertyContainerHandle_t m_tracked_device_container;
        ETrackedControllerRole m_role;
        const KnuckleComponentDefinition *m_component_definitions;
        uint32_t m_num_component_definitions;
        SoftKnucklesDebugHandler *m_debug_handler;

        vr::DriverPose_t m_pose;
        string m_serial_number;
        string m_model_number;
        string m_render_model_name;
        vector<VRInputComponentHandle_t> m_component_handles;
        std::atomic<bool> m_running;
        thread m_pose_thread;

    public:
        SoftKnucklesDevice();
        void Init(ETrackedControllerRole role,
            const KnuckleComponentDefinition *component_definitions,
            uint32_t num_component_definitions,
            SoftKnucklesDebugHandler *debug_handler);

        // implement required ITrackedDeviceServerDriver interfaces
        EVRInitError Activate(uint32_t unObjectId) override;
        void Deactivate() override;
        void EnterStandby() override;
        void Reactivate(); // TBD: how does a device become reactivated when it leave standby?
        void *GetComponent(const char *pchComponentNameAndVersion) override;
        void DebugRequest(const char *pchRequest, char *pchResponseBuffer, uint32_t unResponseBufferSize) override;
        DriverPose_t GetPose() override;

        string get_serial() const;

    private:
        VRInputComponentHandle_t CreateBooleanComponent(const char *full_path);
        VRInputComponentHandle_t CreateScalarComponent(const char *full_path, EVRScalarType scalar_type, EVRScalarUnits scalar_units);
        VRInputComponentHandle_t CreateTrackpadComponent(const char *name);
        VRInputComponentHandle_t CreateHapticComponent(const char *name);
        VRInputComponentHandle_t CreateSkeletonComponent(const char *name, const char *skeleton_path, const char *base_pose_path,
            const VRBoneTransform_t *pGripLimitTransforms, uint32_t unGripLimitTransformCount);
        void SetProperty(ETrackedDeviceProperty prop_key, const char *prop_value);
        void SetInt32Property(ETrackedDeviceProperty prop_key, int32_t value);
        void SetBoolProperty(ETrackedDeviceProperty prop_key, int32_t value);
        static void update_pose_thread(SoftKnucklesDevice *pthis);
    };
}
