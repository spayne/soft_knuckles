#!/bin/bash
export OPENVR_DIR=~/projects/openvr

export INCLUDES=-I$OPENVR_DIR/headers

echo $INCLUDES

export COMPILE_PFX="g++ $INCLUDES -std=c++11"


$COMPILE_PFX -c dprintf.cpp 
$COMPILE_PFX -c socket_notifier.cpp 
$COMPILE_PFX -c soft_knuckles_config.cpp 
$COMPILE_PFX -c soft_knuckles_debug_handler.cpp 
$COMPILE_PFX -c soft_knuckles_device.cpp 
$COMPILE_PFX -c soft_knuckles_provider.cpp 
