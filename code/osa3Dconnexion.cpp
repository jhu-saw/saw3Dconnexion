/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */
/*

  Author(s): Simon Leonard
  Created on: Jan 11 2012

  (C) Copyright 2008 Johns Hopkins University (JHU), All Rights
  Reserved.

--- begin cisst license - do not edit ---

This software is provided "as is" under an open source license, with
no warranty.  The complete license can be found in license.txt and
http://www.cisst.org/cisst/license.txt.

--- end cisst license ---
*/

#include <saw3Dconnexion/osa3Dconnexion.h>

#include <cisstCommon/cmnAssert.h>
#include <cisstCommon/cmnLogger.h>

#if (CISST_OS == CISST_LINUX)
#include <string.h>           // for memset
#include <fcntl.h>            // for open/close read/write O_RDWR
#include <libudev.h>
#include <linux/joystick.h>   // for joystick event
#else
#endif

// OS dependent structure
struct osa3Dconnexion::Internals{

#if (CISST_OS == CISST_LINUX)
    std::string inputfn;     // input filename
    std::string eventfn;     // event filename
    int inputfd;             // file descriptor for input device
    int eventfd;             // file descriptor for event device (LED)
    long long data[6];       // state of the device (events are per axis)
#else
#endif

};


osa3Dconnexion::osa3Dconnexion() :
    internals( NULL ) { 
    
    // allocate the internal structure
    try{ internals = new osa3Dconnexion::Internals; }
    catch( std::bad_alloc& )
        { CMN_LOG_RUN_ERROR << "Failed to allocate internals" << std::endl; }

    // initialize the structure
#if (CISST_OS == CISST_LINUX)

    internals->inputfd = -1;
    internals->eventfd = -1;
    for( size_t i=0; i<6; i++ ){ internals->data[i] = 0; }

#else
#endif

}

osa3Dconnexion::~osa3Dconnexion(){

    if( internals != NULL ){
        
#if (CISST_OS == CISST_LINUX)

        Close();

#else
#endif

        delete internals;
        
    }

}

osa3Dconnexion::Errno osa3Dconnexion::Open( const std::string& filename ){

    if( internals != NULL ){

#if (CISST_OS == CISST_LINUX)

        // only open if device is closed
        if( internals->inputfd == -1 ){

            if( filename.empty() ){

                struct udev* udev;
                struct udev_enumerate* enumerate;
                struct udev_list_entry *devices, *dev_list_entry;;
                udev = udev_new();
                if( udev == NULL ){
                    CMN_LOG_RUN_ERROR << "Failed to create udev context"<<std::endl;
                    return osa3Dconnexion::EFAILURE;
                }
                
                // list the devices
                enumerate = udev_enumerate_new( udev );
                // only keep "input" devices (as in /dev/input")
                udev_enumerate_add_match_subsystem( enumerate, "input" );
                udev_enumerate_scan_devices( enumerate );
                devices = udev_enumerate_get_list_entry( enumerate );
                
                // iterate through all the input devices
                udev_list_entry_foreach( dev_list_entry, devices ){
                    
                    const char *path;
                    struct udev_device* dev;
                    
                    // sysfs path (i.e. /sys/devices/)
                    path = udev_list_entry_get_name( dev_list_entry );
                    dev = udev_device_new_from_syspath( udev, path );

                    // some path to a /dev
                    const char* devfilename = udev_device_get_devnode( dev );
                    if( devfilename != NULL ){
                        
                        // keep usb devices
                        struct udev_device* usbdev = 
                            udev_device_get_parent_with_subsystem_devtype( dev, "usb", "usb_device" );
                        
                        // get vendor/product ID
                        const char* vendor = NULL;
                        const char* product = NULL;
                        vendor=udev_device_get_sysattr_value(usbdev,"idVendor");
                        product=udev_device_get_sysattr_value(usbdev, "idProduct");
                        
                        if( vendor != NULL && product != NULL ){
                            
                            // 3Dconnexion vendorID and SpaceNavigator productID
                            if( strcmp( vendor, "046d" ) == 0 &&
                                strcmp( product, "c626" ) == 0 ){
                                
                                std::string tmp( devfilename );
                                if( tmp.find( "js" ) != std::string::npos )
                                    { internals->inputfn = tmp; }
                                if( tmp.find( "event" ) != std::string::npos )
                                    { internals->eventfn = tmp; }
                            }
                        }
                    }
                    
                    udev_device_unref(dev);
                    
                }

                if( !internals->inputfn.empty() && !internals->eventfn.empty() ){

                    // try to open the input
                    internals->inputfd = open(internals->inputfn.c_str(), O_RDONLY);
                    if( internals->inputfd == -1 ){
                        CMN_LOG_RUN_ERROR << "Failed to open "
                                          << internals->inputfn << std::endl;
                        return osa3Dconnexion::EFAILURE;
                    }
                    
                    // try to open the event (not critical)
                    internals->eventfd = open(internals->eventfn.c_str(), O_RDWR); \
                    if( internals->inputfd != -1 )
                        { LEDOn(); }
                    else{
                        CMN_LOG_RUN_ERROR << "Failed to open "
                                          << internals->eventfn << std::endl;
                    }
                    
                }
                
            }
            else{

                // try to open the input
                internals->inputfn = filename;
                internals->inputfd = open(internals->inputfn.c_str(), O_RDWR);
                if( internals->inputfd == -1 ){
                    CMN_LOG_RUN_ERROR << "Failed to open "
                                      << internals->inputfn << std::endl;
                    return osa3Dconnexion::EFAILURE;
                }
                
            }
        }
        else{
            CMN_LOG_RUN_ERROR << "Device is already open." << std::endl;
            return osa3Dconnexion::EFAILURE;
        }

#else
#endif

    }

    return osa3Dconnexion::ESUCCESS;
}

osa3Dconnexion::Errno osa3Dconnexion::Close(){

    if( internals != NULL ){
        
#if (CISST_OS == CISST_LINUX)
        
        // close the device if not already closed
        if( internals->inputfd != -1 ){
            if( close( internals->inputfd ) == -1 )
                { CMN_LOG_RUN_ERROR << "Failed to close input." << std::endl; }
        }
    
        // close the device if not already closed
        if( internals->eventfd != -1 ){
            LEDOff();
            if( close( internals->eventfd ) == -1 )
                { CMN_LOG_RUN_ERROR << "Failed to close event." << std::endl; }
        }
        
#else
#endif

    }

    return osa3Dconnexion::ESUCCESS;

}

osa3Dconnexion::Errno osa3Dconnexion::LEDOn(){

    if( internals != NULL ){

#if (CISST_OS == CISST_LINUX)

        // event device must be opened
        if( internals->eventfd != -1) {
            struct input_event ev;

            memset(&ev, 0, sizeof ev);
            ev.type = EV_LED;
            ev.code = LED_MISC;
            ev.value = 1;
        
            if( write( internals->eventfd, &ev, sizeof(ev) ) == -1){
                CMN_LOG_RUN_ERROR << "Failed to write event" << std::endl;
                return osa3Dconnexion::EFAILURE;
            }
            
        }
        else{
            CMN_LOG_RUN_ERROR << "Event device not opened" << std::endl;
            return osa3Dconnexion::EFAILURE;
        }
    
#else
#endif

    }

    return osa3Dconnexion::ESUCCESS;

}

osa3Dconnexion::Errno osa3Dconnexion::LEDOff(){

    if( internals != NULL ){

#if (CISST_OS == CISST_LINUX)

        // event device must be opened
        if( internals->eventfd != -1) {
            struct input_event ev;

            memset(&ev, 0, sizeof ev);
            ev.type = EV_LED;
            ev.code = LED_MISC;
            ev.value = 0;
        
            if( write( internals->eventfd, &ev, sizeof(ev) ) == -1){
                CMN_LOG_RUN_ERROR << "Failed to write event" << std::endl;
                return osa3Dconnexion::EFAILURE;
            }
            
        }
        else{
            CMN_LOG_RUN_ERROR << "Event device not opened" << std::endl;
            return osa3Dconnexion::EFAILURE;
        }
    
#else
#endif

    }

    return osa3Dconnexion::ESUCCESS;

}

osa3Dconnexion::Event osa3Dconnexion::WaitForEvent(){

    osa3Dconnexion::Event event;
    event.type = osa3Dconnexion::Event::UNKNOWN;

    if( internals != NULL ){

#if (CISST_OS == CISST_LINUX)
        
        // check the file descriptor
        if( internals->inputfd != -1 ){
            
            // read the event
            struct js_event e; 
            if( read( internals->inputfd, &e, sizeof(struct js_event) ) != -1 ){
                
                // copty the timestamp
                event.timestamp = e.time;
                
                // Button event
                if( e.type == JS_EVENT_BUTTON ){
                    
                    // Find which button event
                    if( e.value == 0 )
                        { event.type = osa3Dconnexion::Event::BUTTON_RELEASED; }
                    if( e.value == 1 )
                        { event.type = osa3Dconnexion::Event::BUTTON_PRESSED; }
                    
                    // Find which button
                    if( e.number == 0 )
                        { event.button = osa3Dconnexion::Event::BUTTON1; }
                    if( e.number == 1 )
                        { event.button = osa3Dconnexion::Event::BUTTON2; }
                    
                }
                
                // Axis event
                if( e.type == JS_EVENT_AXIS ){
                    
                    event.type = osa3Dconnexion::Event::MOTION;
                    // accumulate the axis value to the internals
                    internals->data[ e.number ] += e.value; 
                    // copy all the axis to the event
                    for( size_t i=0; i<6; i++ )
                        { event.data[i] = internals->data[ i ]; }

                }
                
            }
            else { CMN_LOG_RUN_ERROR << "Failed to read device" << std::endl; }
        }
        else { CMN_LOG_RUN_ERROR << "Invalid device" << std::endl; }
#else
#endif

    }

    return event;
}
