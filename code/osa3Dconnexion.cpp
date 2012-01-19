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
#include <X11/Xlib.h>
#endif

// OS dependent structure
struct osa3Dconnexion::Internals{

#if (CISST_OS == CISST_LINUX)

    Display* display;
    Window window;
    Atom event_motion;
    Atom event_buttonpress;
    Atom event_buttonrelease;
    Atom event_command;

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
    internals->display = NULL;
#else
#endif

}


osa3Dconnexion::~osa3Dconnexion(){

    // deallocate the internal structure
    if( internals != NULL ){
        
#if (CISST_OS == CISST_LINUX)
        if( internals->display != NULL ){
            XDestroyWindow( internals->display, internals->window );
            XCloseDisplay( internals->display );
        }
#else
#endif

        delete internals;

    }

}


osa3Dconnexion::Errno osa3Dconnexion::Initialize(){

#if (CISST_OS == CISST_LINUX)

    // open the default display (DISPLAY environment variable)
    Display* display = XOpenDisplay( NULL );
    if( display == NULL ){
        CMN_LOG_RUN_ERROR << "Failed to open X display." << std::endl;
        return osa3Dconnexion::EFAILURE;
    }

    // get the root window
    Window root = DefaultRootWindow( display );

    // get some info about the default screen
    int screennumber = DefaultScreen( display );    
    int width  = DisplayWidth( display, screennumber );
    int height = DisplayHeight( display, screennumber );
    unsigned long black = BlackPixel( display, screennumber );
    unsigned long white = WhitePixel( display, screennumber );

    // create a unmapped window
    Window window = XCreateSimpleWindow( display,   // display
                                         root,      // root window
                                         0,         // x
                                         0,         // y
                                         width/5*3, // width
                                         height/8,  // height
                                         20,        // border width
                                         black,     // border pixel
                                         white );   // background pixel

    // get the atom identifiers for 3Dconnexion events
    Atom event_motion       =XInternAtom( display, "MotionEvent", True );
    Atom event_buttonpress  =XInternAtom( display, "ButtonPressEvent", True );
    Atom event_buttonrelease=XInternAtom( display, "ButtonReleaseEvent",True );
    Atom event_command      =XInternAtom( display, "CommandEvent", True );
    
    if( event_motion        == None ||
        event_buttonpress   == None ||
        event_buttonrelease == None ||
        event_command       == None ){
        CMN_LOG_RUN_ERROR << "Cannot get atom identifiers." << std::endl;
        return osa3Dconnexion::EFAILURE;
    }
    
    XSelectInput( display, window, KeyPressMask | KeyReleaseMask );

    // copy the info in the internals
    internals->display             = display;
    internals->window              = window;
    internals->event_motion        = event_motion;
    internals->event_buttonpress   = event_buttonpress;
    internals->event_buttonrelease = event_buttonrelease;
    internals->event_command       = event_command;

#endif
    
    return osa3Dconnexion::ESUCCESS;
    
}

osa3Dconnexion::Event osa3Dconnexion::WaitForEvent(){

    osa3Dconnexion::Event event;

#if (CISST_OS == CISST_LINUX)

    if( internals->display != NULL ){
        
        // get the next X event
        XEvent xevent;
        XNextEvent( internals->display, &xevent );
        
        // check the event type
        if( xevent.type == ClientMessage ){
            
            if( xevent.xclient.message_type == internals->event_motion ){
                event.type = osa3Dconnexion::Event::MOTION;
                event.data[0] = xevent.xclient.data.s[2];
                event.data[1] = xevent.xclient.data.s[3];
                event.data[2] = xevent.xclient.data.s[4];
                event.data[3] = xevent.xclient.data.s[5];
                event.data[4] = xevent.xclient.data.s[6];
                event.data[5] = xevent.xclient.data.s[7];
                event.period  = xevent.xclient.data.s[8]*1000/60;
            }
            
            if( xevent.xclient.message_type == internals->event_buttonpress ){
                event.type = osa3Dconnexion::Event::BUTTON_PRESSED;
                switch( xevent.xclient.data.s[2] ){
                case 1:
                    event.button = osa3Dconnexion::Event::BUTTON1;
                    break;
                case 2:
                    event.button = osa3Dconnexion::Event::BUTTON2;
                    break;
                default:
                    event.button = osa3Dconnexion::Event::UNKNOWN;
                    break;
                }
            }
            
            if( xevent.xclient.message_type == internals->event_buttonrelease ){
                event.type = osa3Dconnexion::Event::BUTTON_RELEASED;
                switch( xevent.xclient.data.s[2] ){
                case 1:
                    event.button = osa3Dconnexion::Event::BUTTON1;
                    break;
                case 2:
                    event.button = osa3Dconnexion::Event::BUTTON2;
                    break;
                default:
                    event.button = osa3Dconnexion::Event::UNKNOWN;
                    break;
                }
            }
        }
    }

#endif

    return event;
}
