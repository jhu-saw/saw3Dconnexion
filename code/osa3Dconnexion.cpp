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
#include <fcntl.h>
#include <linux/joystick.h>
#else
#endif

// OS dependent structure
struct osa3Dconnexion::Internals{

#if (CISST_OS == CISST_LINUX)
    std::string filename;    // device filename
    int fd;                  // file descriptor
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

    internals->fd = -1;
    for( size_t i=0; i<6; i++ ){ internals->data[i] = 0; }

#else
#endif

}

osa3Dconnexion::~osa3Dconnexion(){

    if( internals != NULL ){
        
#if (CISST_OS == CISST_LINUX)

        // close the device if not already closed
        if( !IsOpened() ){
            if( close( internals->fd ) == -1 )
                { CMN_LOG_RUN_ERROR << "Failed to close." << std::endl; }
        }

#else
#endif

        delete internals;
        
    }

}

bool osa3Dconnexion::IsOpened() const {

    bool isopened = false;

    if( internals != NULL ){

#if (CISST_OS == CISST_LINUX)

        isopened = (internals->fd != -1 );

#else
#endif

    }

    return isopened;

}

osa3Dconnexion::Errno osa3Dconnexion::Open( const std::string& filename ){

    if( internals != NULL ){

#if (CISST_OS == CISST_LINUX)

        // only open if device is closed
        if( !IsOpened() ){

            internals->filename = filename;
            internals->fd = open( filename.c_str(), O_RDWR );

            if( !IsOpened() ){
                CMN_LOG_RUN_ERROR << "Failed to open " << filename << std::endl;
                return osa3Dconnexion::EFAILURE;
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

        // only close if device is open
        if( IsOpened() ){
            if( close( internals->fd ) != -1 )
                { internals->fd = -1; }
            else{
                CMN_LOG_RUN_ERROR << "Failed to close." << std::endl;
                return osa3Dconnexion::EFAILURE;
            }
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
        if( IsOpened() ){
            
            // read the event
            struct js_event e; 
            if( read( internals->fd, &e, sizeof(struct js_event) ) != -1 ){
                
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
