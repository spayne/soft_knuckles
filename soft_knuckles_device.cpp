//////////////////////////////////////////////////////////////////////////////
// soft_knuckles_device.cpp
// 
// See header for description
//
#if defined(_WIN32)
#include <io.h>
#include <tchar.h>
#include <windows.h>
#endif

#include <openvr_driver.h>
#include <thread>
#include <mutex>
#include <string.h>
#include <vector>
#include <string>
#include "dprintf.h"

#include "soft_knuckles_device.h"
#include "soft_knuckles_config.h"
#include "soft_knuckles_debug_handler.h"


using namespace vr;
using namespace std;

namespace soft_knuckles
{
SoftKnucklesDevice::SoftKnucklesDevice()
        :   m_id(vr::k_unTrackedDeviceIndexInvalid),
            m_activated(false),
            m_driver_context(nullptr),
            m_tracked_device_container(k_unTrackedDeviceIndexInvalid),
            m_role(TrackedControllerRole_Invalid),
            m_running(false)
    {
        dprintf("soft_knuckles constructor\n");
        m_pose = { 0 };
        m_pose.poseIsValid = true;
        m_pose.result = vr::TrackingResult_Running_OK;
        m_pose.deviceIsConnected = true;
        m_pose.qWorldFromDriverRotation.w = 1;
        m_pose.qWorldFromDriverRotation.x = 0;
        m_pose.qWorldFromDriverRotation.y = 0;
        m_pose.qWorldFromDriverRotation.z = 0;
        m_pose.qDriverFromHeadRotation.w = 1;
        m_pose.qDriverFromHeadRotation.x = 0;
        m_pose.qDriverFromHeadRotation.y = 0;
        m_pose.qDriverFromHeadRotation.z = 0;

        m_pose.vecPosition[0] = 0;
        m_pose.vecPosition[1] = -.5;
        m_pose.vecPosition[2] = -1.5;
    }

void SoftKnucklesDevice::Init(
    ETrackedControllerRole role,
    const KnuckleComponentDefinition *component_definitions,
    uint32_t num_component_definitions,
    SoftKnucklesDebugHandler *debug_handler)
{
    dprintf("soft_knuckles Init for role: %d num_definitions %d\n", role, num_component_definitions);

    m_component_definitions = component_definitions;
    m_num_component_definitions = num_component_definitions;
    m_debug_handler = debug_handler;
    m_role = role;

    // look up config from soft_knuckles/resources/default.vrsettings.  
    char buf[1024];
    vr::VRSettings()->GetString(kSettingsSection, "serialNumber", buf, sizeof(buf));
    m_serial_number = buf;
    vr::VRSettings()->GetString(kSettingsSection, "modelNumber", buf, sizeof(buf));
    m_model_number = buf;

    if (m_role == TrackedControllerRole_LeftHand)
    {
        m_serial_number += "L";
        m_render_model_name = "{knuckles}/rendermodels/valve_controller_knu_ev2_0_left";    // TODO, use own models
    }
    else if (m_role == TrackedControllerRole_RightHand)
    {
        m_serial_number += "R";
        m_render_model_name = "{knuckles}/rendermodels/valve_controller_knu_ev2_0_right";
    }
        
    dprintf("soft_knuckles serial: %s\n", m_serial_number.c_str());
    dprintf("soft_knuckles model_number: %s\n", m_model_number.c_str());

    if (m_debug_handler)
    {
        m_debug_handler->Init(this);
    }
}

void SoftKnucklesDevice::EnterStandby()
{
    dprintf("SoftKnucklesDevice::EnterStandby()\n");
    if (m_running)
    {
        m_running = false;
        if (m_pose_thread.joinable())
        {
            m_pose_thread.join();
        }
    }
}

VRInputComponentHandle_t SoftKnucklesDevice::CreateBooleanComponent(const char *full_path)
{
    VRInputComponentHandle_t input_handle = k_ulInvalidInputComponentHandle;
    dprintf("soft_knuckles creating bool handle for %s on %d\n", full_path, m_tracked_device_container);
    EVRInputError input_error = vr::VRDriverInput()->CreateBooleanComponent(m_tracked_device_container, full_path, &input_handle); // note it goes into the container specific to this instance of the device
    if (input_error != VRInputError_None)
    {
        dprintf("error %d\n", input_error);
    }
    else
    {
        dprintf("ok\n");
    }
    return input_handle;
}

VRInputComponentHandle_t SoftKnucklesDevice::CreateScalarComponent(const char *full_path, EVRScalarType scalar_type, EVRScalarUnits scalar_units)
{
    VRInputComponentHandle_t input_handle = k_ulInvalidInputComponentHandle;
    dprintf("soft_knuckles creating scalar input handle for %s on %d\n", full_path, m_tracked_device_container);
    EVRInputError input_error = vr::VRDriverInput()->CreateScalarComponent(m_tracked_device_container, full_path, &input_handle,
                    scalar_type, scalar_units);
    
    if (input_error != VRInputError_None)
    {
        dprintf("error %d\n", input_error);
    }
    else
    {
        dprintf("ok\n");
    }
    return input_handle;
}

VRInputComponentHandle_t SoftKnucklesDevice::CreateHapticComponent(const char *name)
{
    VRInputComponentHandle_t input_handle = k_ulInvalidInputComponentHandle;
    dprintf("soft_knuckles creating haptic handle for %s\n", name);
    EVRInputError input_error = vr::VRDriverInput()->CreateHapticComponent(m_tracked_device_container, name, &input_handle); // note it goes into the container specific to this instance of the device
    if (input_error != VRInputError_None)
    {
        dprintf("error %d\n", input_error);
    }
    else
    {
        dprintf("ok\n");
    }
    return input_handle;
}

VRInputComponentHandle_t SoftKnucklesDevice::CreateSkeletonComponent(const char *name, const char *skeleton_path, const char *base_pose_path,
                                const VRBoneTransform_t *pGripLimitTransforms, uint32_t unGripLimitTransformCount)
{
    VRInputComponentHandle_t input_handle = k_ulInvalidInputComponentHandle;
    dprintf("soft_knuckles creating skeleton handle for %s\n", name);
    EVRInputError input_error = vr::VRDriverInput()->CreateSkeletonComponent(m_tracked_device_container, name, skeleton_path, base_pose_path,
                                                                                pGripLimitTransforms, unGripLimitTransformCount, &input_handle);
    if (input_error != VRInputError_None)
    {
        dprintf("error %d\n", input_error);
    }
    else
    {
        dprintf("ok\n");
    }
    return input_handle;
}

void SoftKnucklesDevice::SetProperty(ETrackedDeviceProperty prop_key, const char *prop_value)
{
    vr::VRProperties()->SetStringProperty(m_tracked_device_container, prop_key, prop_value);
}

void SoftKnucklesDevice::SetInt32Property(ETrackedDeviceProperty prop_key, int32_t value)
{
    vr::VRProperties()->SetInt32Property(m_tracked_device_container, prop_key, value);
}

void SoftKnucklesDevice::SetBoolProperty(ETrackedDeviceProperty prop_key, int32_t value)
{
    vr::VRProperties()->SetBoolProperty(m_tracked_device_container, prop_key, value);
}

void SoftKnucklesDevice::update_pose_thread(SoftKnucklesDevice *pthis)
{
#ifdef _WIN32
    HRESULT hr = SetThreadDescription(GetCurrentThread(), L"update_pose_thread");
#endif
    while (pthis->m_running)
    {
        vr::VRServerDriverHost()->TrackedDevicePoseUpdated(pthis->m_id, pthis->GetPose(), sizeof(DriverPose_t));
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

EVRInitError SoftKnucklesDevice::Activate(uint32_t unObjectId) 
{
    if (m_activated)
    {
        dprintf("warning: Activate called twice\n");
        return VRInitError_Driver_Failed;
    }
    m_activated = true;
    m_id = unObjectId;
    m_tracked_device_container = vr::VRProperties()->TrackedDeviceToPropertyContainer(m_id);
    dprintf("soft_knuckles Activated.  object ID: %d\n", m_id);

    SetProperty(Prop_SerialNumber_String, m_serial_number.c_str());
    SetProperty(Prop_ModelNumber_String, m_model_number.c_str());
    SetProperty(Prop_RenderModelName_String, m_render_model_name.c_str());
    SetProperty(Prop_ManufacturerName_String, "HTC");
    SetInt32Property(Prop_ControllerRoleHint_Int32, m_role);
    SetInt32Property(Prop_DeviceClass_Int32, (int32_t)TrackedDeviceClass_Controller);
    SetProperty(Prop_InputProfilePath_String, "{soft_knuckles}/input/soft_knuckles_profile.json");
    SetProperty(Prop_ControllerType_String, "soft_knuckles");
    SetProperty(Prop_LegacyInputProfile_String, "soft_knuckles");

    m_component_handles.resize(m_num_component_definitions);
    for (uint32_t i = 0; i < m_num_component_definitions; i++)
    {
        const KnuckleComponentDefinition *definition = &m_component_definitions[i];
        switch (definition->component_type)
        {
            case CT_BOOLEAN:
                m_component_handles[i] = CreateBooleanComponent(definition->full_path);
                break;
            case CT_SCALAR:
                m_component_handles[i] = CreateScalarComponent(definition->full_path, definition->scalar_type, definition->scalar_units );
                break;
            case CT_SKELETON:
                m_component_handles[i] = CreateSkeletonComponent(definition->full_path, definition->skeleton_path, definition->base_pose_path, nullptr, 0);
                break;
            case CT_HAPTIC:
                m_component_handles[i] = CreateHapticComponent(definition->full_path);
                break;
        }
    }

    m_running = true;
    m_pose_thread = thread(update_pose_thread, this);

    return VRInitError_None;
}

void SoftKnucklesDevice::Deactivate() 
{
    dprintf("soft_knuckles Deactivating.  object ID: %d\n", m_id);
    if (m_running)
    {
        m_running = false;
        if (m_pose_thread.joinable())
            m_pose_thread.join();
    
    }
}

void SoftKnucklesDevice::Reactivate()
{
    dprintf("SoftKnucklesDevice::Reactivate()\n");
    if (!m_running)
    {
        m_running = true;
        m_pose_thread = thread(update_pose_thread, this);
    }
}

void *SoftKnucklesDevice::GetComponent(const char *pchComponentNameAndVersion)
{
    // GetComponent will get called for the IVRControllerComponent_001
    dprintf("soft_knuckles GetComponent: %s\n");
    return nullptr;
}

void SoftKnucklesDevice::DebugRequest(const char *pchRequest, char *pchResponseBuffer, uint32_t unResponseBufferSize)
{
    if (m_debug_handler)
    {
        m_debug_handler->DebugRequest(pchRequest, pchResponseBuffer, unResponseBufferSize);
    }
}

DriverPose_t SoftKnucklesDevice::GetPose()
{
    return m_pose;
}

string SoftKnucklesDevice::get_serial() const
{
    return m_serial_number;
}

} // end of namespace