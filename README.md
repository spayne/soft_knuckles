# Soft Knuckles OpenVR Device Driver

An OpenVR input device driver.  To ensure the coverage is complete it's meant to align with how Valve's Knuckles EV2 driver exposes inputs.

It's intended for developers to use as a starting point for further driver development.


## Getting Started

### Prerequisites

1. Enable/Install the SteamVR beta from within Steam.
2. If you haven't already clone or update your openvr installation [https://github.com/ValveSoftware/openvr]


### Installing and Running the Driver

1. Download repo
2. Using Visual Studio 2017, open soft_knuckles.sln.  Update the header directories and linker directories to point to your installed location of openvr.
3. Build the 64 bit debug version of the dll.  This should compile fine.
4. Edit install_softknuckles_debug.bat.  There are two things you need to change to match your installation:  4a. Change the path to vrpathreg to your vrpathreg.exe. 4b. Change the driver path to the [current_directory]/soft_knuckles.  Notice this soft_knuckles directory is important because this is where the config files reside and where the driver dll will reside.
5. Run install_softknuckles_debug.bat from a command prompt.  This should successfully copy the dll and install the current directory into vrpathreg so that steamvr knows about your new driver.
6. Turn off your controllers, turn on just your HMD and lighthouses.   We don't want any controllers on right now because we will soon add the software ones dynamically.
7. Start SteamVR.  I usually start steamvr by running windbg.exe -g and then opening the vrmonitor.exe executable and clicking on "debug child processes".  This gives you a nice amount of debugging so you can see the monitor, server, compositor and drivers starting up. See [https://github.com/spayne/soft_knuckles/blob/master/doc/start_with_no_controllers.png]
8. At this point SteamVR should be running and only the headset and lighthouses should be present.
9. Use telnet to localhost 27015. This will trigger the soft_knuckles device driver to add 2 new active devices.  The left and right knuckles. See [https://github.com/spayne/soft_knuckles/blob/master/doc/use_telnet_to_trigger_adding_controllers.png].
10. At this point the knuckles should be showing green.  If you put on your headset you should see the knuckles somewhere in your room floating in the air. See [https://github.com/spayne/soft_knuckles/blob/master/doc/knuckles_floating_in_air.jpg] 
11. Start Steam 
12. From a web browser, open the controller bindings gui at [].  You should see the soft knuckles controller available.  See [https://github.com/spayne/soft_knuckles/blob/master/doc/edit_soft_knuckles_bindings.png] Edit the soft_knuckles_controller_configuration. Choose the Input Debugger option at the bottom.  You should see the soft knuckles config along the right side.  
13. From Visual Studio, start the soft_knuckles_debug_client.  Try executing a command to move the right controller to 0, 0, 0 by typing <b>r pos 0 0 0</b> Put the headset on and observe that the right controller has moved to one of the lighthouses.  Try executing a command to set the left joystick position <b>r /input/joystick/x -1</b>. [See [https://github.com/spayne/soft_knuckles/blob/master/doc/controllers_moved_using_debug_client.png] 
14. Try modifying other states. See [https://github.com/spayne/soft_knuckles/blob/master/doc/use_soft_knuckles_client_to_set_input_states.png]

### Looking at the code
1. open <b>soft_knuckles_provider.cpp</b>.  Look at <b>HmdDriverFactory</b>.  This is the main for the driver: this is how openvr finds out about what kinds of devices your driver provides.  Conceptually openvr considers your driver to be a device provider and you provide a  IServerTrackedDeviceProvider that openvr will give an <b> IVRDriverContext</b> to.
2. in the same file, look at <b>SoftKnucklesProvider::Init()</b>.  Here two devices/instances of the same soft_knuckles type are created 'locally' in the driver, but are not published.  They * could * be published here, but instead we wait on a socket for a signal from you/the developer to add the devices.  Instead of a socket, a real driver would be listening on hid, usb or hdmi busses to find and publish devices.
3. in the same file look at listen_thread.  Here you will see a couple calls to <b>TrackedDeviceAdded</b> Here is when the devices are actually published to openvr and greyed out icons for these controllers will start to appear in the vrmonitor.exe.
4. Open <b>soft_knuckles_device.cpp</b>.   Look at <b>SoftKnucklesDevice::Activate</b> Here is where the devices get published and for each button or input or output on the device a component (<b>VRInputComponentHandle_t>is created.  
5. Open <b>soft_knuckles_config.cpp</b>.   Observe that for each instance we are going to register 20 or so different component handles.  Open [https://github.com/ValveSoftware/openvr/wiki/Input-Profiles] and look at the section called <b>"Input source path"</b>.  Observe that there is a one to one mapping between the types of input sources and component type between what the web page describes and what we are mapping here.  A device doesn't necessarily need to register everything, but I'm registering as much as possible so that what is in the application profiles have something to bind to.

### Status
The driver framework is there and is usable to test actions and bindings.   

### TODO - Improve placeholder controllers.
Originally this project had used links to the actual knuckles art in Valve's driver.  This has now been replaced by placeholder art.  New art and buttons need to be added as well as input binding positions and, I think, separate component configurations.  This will probably go hand in hand with adding a placeholder skeleton/animation system, below. 

### TODO - Animation System
The animation system is not integrated yet.  With the new input system, it's up to the driver to provide bones and animation transformation updates to the vr system.  That is just the bones - no skinning or models or textures of the hands - presumably that's done in the unity or unreal layers.
* To do this, an animation state machine will need to be added to the device.  Refer to [https://github.com/ValveSoftware/openvr/wiki/Creating-a-Skeletal-Input-Driver] to understand what needs to be done.
* <b>open3mod</b> can be used to inspect the fbx files.  See [/https://github.com/spayne/soft_knuckles/blob/master/doc/fbx_viewer.png]
* To test the animation system, the steamvr plugin in unity can be used - look for the interaction scene
* It would be interesting to see how the <b>Moondust</b> demo would interact with the soft_knuckles.   It may be looking for some state or config that hasn't been handled/simulated.

## Contact
sean.d.payne@gmail.com
