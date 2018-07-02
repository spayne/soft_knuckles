//////////////////////////////////////////////////////////////////////////////
// soft_knuckles_debug_client.cpp
//
// Reads commands from standard input and sends the commands to either 
// the left or right soft_knuckles_controller. 
// 
// It knows which controllers are right and left and sends to those device 
// indexes.  It is does not actually know the input_paths supported by the
// device and relies on the server side to check that.
//
// The example syntax in the printfs below should be sufficient to understand
// the syntax.  
//
#include <stdio.h>
#include <ctype.h>
#include <openvr.h>
#include <thread>
#include <chrono>
#include <cstring>

#ifdef _WIN32
#pragma warning (disable: 4996)
#endif

using namespace vr;

static void send_request(COpenVRContext &ctx, vr::TrackedDeviceIndex_t index, const char *request, char *response_buffer, uint32_t response_buffer_size)
{
    //printf("sending request: %s\n", request);
    response_buffer[0] = '\0';
    ctx.VRSystem()->DriverDebugRequest(index, request, response_buffer, response_buffer_size);
    printf("%s\n", response_buffer);
}

int main()
{
    EVRInitError err;
    VR_Init(&err, VRApplication_Utility);

    if (err == VRInitError_None)
    {
        COpenVRContext ctx;
        // look for the two soft_knuckles controllers
        TrackedDeviceIndex_t left_index = ctx.VRSystem()->GetTrackedDeviceIndexForControllerRole(TrackedControllerRole_LeftHand);
        TrackedDeviceIndex_t right_index = ctx.VRSystem()->GetTrackedDeviceIndexForControllerRole(TrackedControllerRole_RightHand);

        printf("Soft Knuckles Debug Client Terminal\n");
        printf("-----------------------------------\n");
        printf("  Example commands:\n");
        printf("   l /input/system/click 0     # toggle system button on left controller\n");
        printf("   l /input/system/click 1\n\n");
        printf("   r pos 0 0 0                 # move right controller to 0,0,0\n");
        printf("   r /input/joystick/x -1      # set right joystick position to -1\n");
        printf("   r /input/trigger/value 0.25 # set right trigger position to .25\n");
        printf("   sleep 50                    # sleep for 50ms\n");
        printf("   quit\n");
        printf("\n");
        printf("ok\n");

        if (left_index != k_unTrackedDeviceIndexInvalid && right_index != k_unTrackedDeviceIndexInvalid)
        {
            bool quit = false;
            while (!quit)
            {
                char szbuf[256];
                fgets(szbuf, sizeof(szbuf), stdin);
                char *cmd = szbuf;
                while (*cmd && isspace(*cmd))
                    cmd++;
                char *terminator = cmd;
                while (*terminator)
                {
                    if (*terminator == '#')
                    {
                        *terminator = '\0';
                        break;
                    }
                    terminator++;
                }

                int sleep_ms;
                if (*cmd == '#' || *cmd == '\0')
                {
                    // ignore comment or empty lines
                }
                else if (sscanf(cmd, "sleep %d", &sleep_ms) == 1)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
                    printf("ok\n");
                }
                else if (strncmp(cmd, "quit", 4) == 0)
                {
                    quit = true;
                    printf("bye\n");
                }
                else
                {
                    TrackedDeviceIndex_t target = k_unTrackedDeviceIndexInvalid;
                    if (cmd[0] == 'l')
                    {
                        target = left_index;
                    }
                    else if (cmd[0] == 'r')
                    {
                        target = right_index;
                    }
                    
                    if (target != k_unTrackedDeviceIndexInvalid)
                    {
                        // send either /input or pos commands to driver to process
                        char response[256];
                        send_request(ctx, target, cmd+1, response, sizeof(response));
                    }
                    else
                    {
                        printf("unrecognized target (choose l or r)\n");
                    }
                }
            }
        }
    }

    return 0;
}