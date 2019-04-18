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
        dprintf("SoftKnucklesDevice::SoftKnucklesDevice\n");
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
    dprintf("SoftKnucklesDevice::Init for role: %d num_definitions %d\n", role, num_component_definitions);

    m_component_definitions = component_definitions;
    m_num_component_definitions = num_component_definitions;
    m_debug_handler = debug_handler;
    m_role = role;

    // look up config from soft_knuckles/resources/settings/default.vrsettings.  
    char buf[1024];
    vr::VRSettings()->GetString(kSettingsSection, "serialNumber", buf, sizeof(buf));
    m_serial_number = buf;
    vr::VRSettings()->GetString(kSettingsSection, "modelNumber", buf, sizeof(buf));
    m_model_number = buf;

    if (m_role == TrackedControllerRole_LeftHand)
    {
        m_serial_number += "L";
        m_render_model_name = "{soft_knuckles}/rendermodels/soft_knuckles_placeholder_left";
    }
    else if (m_role == TrackedControllerRole_RightHand)
    {
        m_serial_number += "R";
        m_render_model_name = "{soft_knuckles}/rendermodels/soft_knuckles_placeholder_right";
		m_pose.vecPosition[0] += 0.2f; // offset the right a little
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
    }
}

VRInputComponentHandle_t SoftKnucklesDevice::CreateBooleanComponent(const char *full_path)
{
	dprintf("SoftKnucklesDevice::CreateBooleanComponent for %s on %d\n", full_path, m_tracked_device_container);
    VRInputComponentHandle_t input_handle = k_ulInvalidInputComponentHandle;
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
	dprintf("SoftKnucklesDevice::CreateScalarComponent for %s on %d\n", full_path, m_tracked_device_container);
    VRInputComponentHandle_t input_handle = k_ulInvalidInputComponentHandle;
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
	dprintf("SoftKnucklesDevice::CreateHapticComponent for %s\n", name);
    VRInputComponentHandle_t input_handle = k_ulInvalidInputComponentHandle;
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
	dprintf("SoftKnucklesDevice::CreateSkeletonComponent for %s\n", name);
    VRInputComponentHandle_t input_handle = k_ulInvalidInputComponentHandle;
    EVRInputError input_error = vr::VRDriverInput()->CreateSkeletonComponent(m_tracked_device_container, name, skeleton_path, base_pose_path,
		VRSkeletalTracking_Partial, pGripLimitTransforms, unGripLimitTransformCount, &input_handle);

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

static const int NUM_BONES = 31;

static VRBoneTransform_t left_open_hand_pose[NUM_BONES] = {
{ { 0.000000f,  0.000000f,  0.000000f,  1.000000f}, { 1.000000f, -0.000000f, -0.000000f,  0.000000f} },
{ {-0.034038f,  0.036503f,  0.164722f,  1.000000f}, {-0.055147f, -0.078608f, -0.920279f,  0.379296f} },
{ {-0.012083f,  0.028070f,  0.025050f,  1.000000f}, { 0.464112f,  0.567418f,  0.272106f,  0.623374f} },
{ { 0.040406f,  0.000000f, -0.000000f,  1.000000f}, { 0.994838f,  0.082939f,  0.019454f,  0.055130f} },
{ { 0.032517f,  0.000000f,  0.000000f,  1.000000f}, { 0.974793f, -0.003213f,  0.021867f, -0.222015f} },
{ { 0.030464f, -0.000000f, -0.000000f,  1.000000f}, { 1.000000f, -0.000000f, -0.000000f,  0.000000f} },
{ { 0.000632f,  0.026866f,  0.015002f,  1.000000f}, { 0.644251f,  0.421979f, -0.478202f,  0.422133f} },
{ { 0.074204f, -0.005002f,  0.000234f,  1.000000f}, { 0.995332f,  0.007007f, -0.039124f,  0.087949f} },
{ { 0.043930f, -0.000000f, -0.000000f,  1.000000f}, { 0.997891f,  0.045808f,  0.002142f, -0.045943f} },
{ { 0.028695f,  0.000000f,  0.000000f,  1.000000f}, { 0.999649f,  0.001850f, -0.022782f, -0.013409f} },
{ { 0.022821f,  0.000000f, -0.000000f,  1.000000f}, { 1.000000f, -0.000000f,  0.000000f, -0.000000f} },
{ { 0.002177f,  0.007120f,  0.016319f,  1.000000f}, { 0.546723f,  0.541276f, -0.442520f,  0.460749f} },
{ { 0.070953f,  0.000779f,  0.000997f,  1.000000f}, { 0.980294f, -0.167261f, -0.078959f,  0.069368f} },
{ { 0.043108f,  0.000000f,  0.000000f,  1.000000f}, { 0.997947f,  0.018493f,  0.013192f,  0.059886f} },
{ { 0.033266f,  0.000000f,  0.000000f,  1.000000f}, { 0.997394f, -0.003328f, -0.028225f, -0.066315f} },
{ { 0.025892f, -0.000000f,  0.000000f,  1.000000f}, { 0.999195f, -0.000000f,  0.000000f,  0.040126f} },
{ { 0.000513f, -0.006545f,  0.016348f,  1.000000f}, { 0.516692f,  0.550143f, -0.495548f,  0.429888f} },
{ { 0.065876f,  0.001786f,  0.000693f,  1.000000f}, { 0.990420f, -0.058696f, -0.101820f,  0.072495f} },
{ { 0.040697f,  0.000000f,  0.000000f,  1.000000f}, { 0.999545f, -0.002240f,  0.000004f,  0.030081f} },
{ { 0.028747f, -0.000000f, -0.000000f,  1.000000f}, { 0.999102f, -0.000721f, -0.012693f,  0.040420f} },
{ { 0.022430f, -0.000000f,  0.000000f,  1.000000f}, { 1.000000f,  0.000000f,  0.000000f,  0.000000f} },
{ {-0.002478f, -0.018981f,  0.015214f,  1.000000f}, { 0.526918f,  0.523940f, -0.584025f,  0.326740f} },
{ { 0.062878f,  0.002844f,  0.000332f,  1.000000f}, { 0.986609f, -0.059615f, -0.135163f,  0.069132f} },
{ { 0.030220f,  0.000000f,  0.000000f,  1.000000f}, { 0.994317f,  0.001896f, -0.000132f,  0.106446f} },
{ { 0.018187f,  0.000000f,  0.000000f,  1.000000f}, { 0.995931f, -0.002010f, -0.052079f, -0.073526f} },
{ { 0.018018f,  0.000000f, -0.000000f,  1.000000f}, { 1.000000f,  0.000000f,  0.000000f,  0.000000f} },
{ {-0.006059f,  0.056285f,  0.060064f,  1.000000f}, { 0.737238f,  0.202745f,  0.594267f,  0.249441f} },
{ {-0.040416f, -0.043018f,  0.019345f,  1.000000f}, {-0.290331f,  0.623527f, -0.663809f, -0.293734f} },
{ {-0.039354f, -0.075674f,  0.047048f,  1.000000f}, {-0.187047f,  0.678062f, -0.659285f, -0.265683f} },
{ {-0.038340f, -0.090987f,  0.082579f,  1.000000f}, {-0.183037f,  0.736793f, -0.634757f, -0.143936f} },
{ {-0.031806f, -0.087214f,  0.121015f,  1.000000f}, {-0.003659f,  0.758407f, -0.639342f, -0.126678f} },
};


static VRBoneTransform_t left_fist_pose[NUM_BONES] = 
{
{ { 0.000000f,  0.000000f,  0.000000f,  1.000000f}, { 1.000000f, -0.000000f, -0.000000f,  0.000000f} },
{ {-0.034038f,  0.036503f,  0.164722f,  1.000000f}, {-0.055147f, -0.078608f, -0.920279f,  0.379296f} },
{ {-0.016305f,  0.027529f,  0.017800f,  1.000000f}, { 0.225703f,  0.483332f,  0.126413f,  0.836342f} },
{ { 0.040406f,  0.000000f, -0.000000f,  1.000000f}, { 0.894335f, -0.013302f, -0.082902f,  0.439448f} },
{ { 0.032517f,  0.000000f,  0.000000f,  1.000000f}, { 0.842428f,  0.000655f,  0.001244f,  0.538807f} },
{ { 0.030464f, -0.000000f, -0.000000f,  1.000000f}, { 1.000000f, -0.000000f, -0.000000f,  0.000000f} },
{ { 0.003802f,  0.021514f,  0.012803f,  1.000000f}, { 0.617314f,  0.395175f, -0.510874f,  0.449185f} },
{ { 0.074204f, -0.005002f,  0.000234f,  1.000000f}, { 0.737291f, -0.032006f, -0.115013f,  0.664944f} },
{ { 0.043287f, -0.000000f, -0.000000f,  1.000000f}, { 0.611381f,  0.003287f,  0.003823f,  0.791321f} },
{ { 0.028275f,  0.000000f,  0.000000f,  1.000000f}, { 0.745388f, -0.000684f, -0.000945f,  0.666629f} },
{ { 0.022821f,  0.000000f, -0.000000f,  1.000000f}, { 1.000000f, -0.000000f,  0.000000f, -0.000000f} },
{ { 0.005787f,  0.006806f,  0.016534f,  1.000000f}, { 0.514203f,  0.522315f, -0.478348f,  0.483700f} },
{ { 0.070953f,  0.000779f,  0.000997f,  1.000000f}, { 0.723653f, -0.097901f,  0.048546f,  0.681458f} },
{ { 0.043108f,  0.000000f,  0.000000f,  1.000000f}, { 0.637464f, -0.002366f, -0.002831f,  0.770472f} },
{ { 0.033266f,  0.000000f,  0.000000f,  1.000000f}, { 0.658008f,  0.002610f,  0.003196f,  0.753000f} },
{ { 0.025892f, -0.000000f,  0.000000f,  1.000000f}, { 0.999195f, -0.000000f,  0.000000f,  0.040126f} },
{ { 0.004123f, -0.006858f,  0.016563f,  1.000000f}, { 0.489609f,  0.523374f, -0.520644f,  0.463997f} },
{ { 0.065876f,  0.001786f,  0.000693f,  1.000000f}, { 0.759970f, -0.055609f,  0.011571f,  0.647471f} },
{ { 0.040331f,  0.000000f,  0.000000f,  1.000000f}, { 0.664315f,  0.001595f,  0.001967f,  0.747449f} },
{ { 0.028489f, -0.000000f, -0.000000f,  1.000000f}, { 0.626957f, -0.002784f, -0.003234f,  0.779042f} },
{ { 0.022430f, -0.000000f,  0.000000f,  1.000000f}, { 1.000000f,  0.000000f,  0.000000f,  0.000000f} },
{ { 0.001131f, -0.019295f,  0.015429f,  1.000000f}, { 0.479766f,  0.477833f, -0.630198f,  0.379934f} },
{ { 0.062878f,  0.002844f,  0.000332f,  1.000000f}, { 0.827001f,  0.034282f,  0.003440f,  0.561144f} },
{ { 0.029874f,  0.000000f,  0.000000f,  1.000000f}, { 0.702185f, -0.006716f, -0.009289f,  0.711903f} },
{ { 0.017979f,  0.000000f,  0.000000f,  1.000000f}, { 0.676853f,  0.007956f,  0.009917f,  0.736009f} },
{ { 0.018018f,  0.000000f, -0.000000f,  1.000000f}, { 1.000000f,  0.000000f,  0.000000f,  0.000000f} },
{ { 0.019716f,  0.002802f,  0.093937f,  1.000000f}, { 0.377286f, -0.540831f,  0.150446f, -0.736562f} },
{ { 0.000171f,  0.016473f,  0.096515f,  1.000000f}, {-0.006456f,  0.022747f, -0.932927f, -0.359287f} },
{ { 0.000448f,  0.001536f,  0.116543f,  1.000000f}, {-0.039357f,  0.105143f, -0.928833f, -0.353079f} },
{ { 0.003949f, -0.014869f,  0.130608f,  1.000000f}, {-0.055071f,  0.068695f, -0.944016f, -0.317933f} },
{ { 0.003263f, -0.034685f,  0.139926f,  1.000000f}, { 0.019690f, -0.100741f, -0.957331f, -0.270149f} },
};

void SoftKnucklesDevice::update_pose_thread(SoftKnucklesDevice *pthis)
{
#ifdef _WIN32
    HRESULT hr = SetThreadDescription(GetCurrentThread(), L"update_pose_thread");
#endif
	bool m_show_open_hand_pose = true;
    while (pthis->m_running)
    {
        vr::VRServerDriverHost()->TrackedDevicePoseUpdated(pthis->m_id, pthis->GetPose(), sizeof(DriverPose_t));

		// demo code to show alternate fist and open_hand poses on the left hand skeleton
		VRBoneTransform_t *left_pose;
		if (m_show_open_hand_pose)
		{
			left_pose = left_open_hand_pose;
		}
		else
		{
			left_pose = left_fist_pose;
		}
		m_show_open_hand_pose = !m_show_open_hand_pose;

		// update right skeletons with right poses
		for (int i = 0; i < pthis->m_component_handles.size(); i++)
		{
			if (pthis->m_component_definitions[i].component_type == CT_SKELETON)
			{
				if (strcmp(pthis->m_component_definitions[i].skeleton_path, "/skeleton/hand/left") == 0)
				{
					vr::VRDriverInput()->UpdateSkeletonComponent(
						pthis->m_component_handles[i],
						vr::VRSkeletalMotionRange_WithoutController,
						left_pose,
						NUM_BONES);
					vr::VRDriverInput()->UpdateSkeletonComponent(
						pthis->m_component_handles[i],
						vr::VRSkeletalMotionRange_WithController,
						left_pose,
						NUM_BONES);

				}
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

EVRInitError SoftKnucklesDevice::Activate(uint32_t unObjectId) 
{
	dprintf("SoftKnucklesDevice::Activate.  object ID: %d\n", unObjectId);
    if (m_activated)
    {
        dprintf("warning: Activate called twice\n");
        return VRInitError_Driver_Failed;
    }
    m_activated = true;
    m_id = unObjectId;
    m_tracked_device_container = vr::VRProperties()->TrackedDeviceToPropertyContainer(m_id);

    SetProperty(Prop_SerialNumber_String, m_serial_number.c_str());
    SetProperty(Prop_ModelNumber_String, m_model_number.c_str());
    SetProperty(Prop_RenderModelName_String, m_render_model_name.c_str());
    SetProperty(Prop_ManufacturerName_String, "sean");
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
	m_pose_thread.detach();

    return VRInitError_None;
}

void SoftKnucklesDevice::Deactivate() 
{
    dprintf("SoftKnucklesDevice::Deactivate.  object ID: %d\n", m_id);
    if (m_running)
    {
		m_running = false; // signal to pose thread to shut down
    }
}

void SoftKnucklesDevice::Reactivate()
{
    dprintf("SoftKnucklesDevice::Reactivate() object ID: %d\n", m_id);
    if (!m_running)
    {
        m_running = true;
        m_pose_thread = thread(update_pose_thread, this);
    }
}

void *SoftKnucklesDevice::GetComponent(const char *pchComponentNameAndVersion)
{
    // GetComponent will get called for the IVRControllerComponent_001
    dprintf("SoftKnucklesDevice::GetComponent: %s\n");
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