/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/*
  Author(s):  Anton Deguet
  Created on: 2026-05-30

  (C) Copyright 2026 Johns Hopkins University (JHU), All Rights Reserved.

--- begin cisst license - do not edit ---

This software is provided "as is" under an open source license, with
no warranty.  The complete license can be found in license.txt and
http://www.cisst.org/cisst/license.txt.

--- end cisst license ---
*/

#ifndef _mts3Dconnexion_h
#define _mts3Dconnexion_h

#include <saw3Dconnexion/mts3DconnexionConfiguration.h>

#include <cisstMultiTask/mtsFunctionWrite.h>
#include <cisstMultiTask/mtsInterfaceProvided.h>
#include <cisstMultiTask/mtsTaskContinuous.h>
#include <cisstParameterTypes/prmBaseFrame.h>
#include <cisstParameterTypes/prmConfigurationJoint.h>
#include <cisstParameterTypes/prmOperatingState.h>
#include <cisstParameterTypes/prmStateCartesian.h>
#include <cisstParameterTypes/prmStateJoint.h>
#include <cisstVector/vctFixedSizeVectorTypes.h>

#include <list>
#include <string>
#include <vector>

struct hid_device_;
typedef struct hid_device_ hid_device;
struct hid_device_info;
class prmPositionCartesianSet;

#include <saw3Dconnexion/saw3DconnexionExport.h> // always include last

class CISST_EXPORT mts3Dconnexion: public mtsTaskContinuous
{
    CMN_DECLARE_SERVICES(CMN_DYNAMIC_CREATION_ONEARG, CMN_LOG_ALLOW_DEFAULT);

 public:
    mts3Dconnexion(const std::string & _component_name);
    mts3Dconnexion(const mtsTaskContinuousConstructorArg & _arg);
    ~mts3Dconnexion(void);

    void Configure(const std::string & _filename = "") override;
    void Startup(void) override;
    void Run(void) override;
    void Cleanup(void) override;

    bool IsConfigured(void) const;
    std::string GetDeviceName(void) const;
    void GetButtonNames(std::list<std::string> & _result) const;

 protected:
    struct ButtonData {
        unsigned int index;
        std::string name;
        bool suppress_events;
        bool pressed;
        mtsFunctionWrite function;
    };

    void Init(void);
    void EnumerateDevices(void) const;
    void ConfigureInterface(void);
    void OpenDevice(void);
    bool ApplyConfiguration(void);
    bool DeviceMatches(const hid_device_info * _device) const;
    std::string DeviceSearchDescription(void) const;
    std::string reference_frame(void) const;
    const std::string & moving_frame(void) const;

    void update_measured_cs(void);
    void PollReports(void);
    void ProcessReport(const unsigned char * _report, const int _length);
    void ProcessMotionReport(const unsigned char * _report,
                             const int _length,
                             const bool _linear);
    void ProcessButtonReport(const unsigned char * _report, const int _length);
    void IntegrateVirtualPose(const double & _period);
    bool ButtonState(const unsigned int & _index) const;

    void state_command(const std::string & _command);
    void lock_orientation(const bool & _lock);
    void lock_position(const bool & _lock);
    void set_base_frame(const prmPositionCartesianSet & _base_frame);
    void reset_pose(void);
    void reset_orientation(void);
    void reset_position(void);
    void configuration_js(prmConfigurationJoint & _configuration) const;

    bool m_configured;
    mts3DconnexionConfiguration m_config;

    unsigned short m_vendor_id;
    unsigned short m_product_id;
    bool m_match_product_id;

    hid_device * m_handle;
    mtsInterfaceProvided * m_interface;
    double m_last_time;
    bool m_sent_connection_error;

    prmOperatingState m_operating_state;
    mtsFunctionWrite m_operating_state_event;
    mtsFunctionWrite m_orientation_locked_event;
    mtsFunctionWrite m_position_locked_event;

    prmStateCartesian m_local_measured_cs;
    prmStateCartesian m_measured_cs;
    prmStateJoint m_gripper_measured_js;
    prmConfigurationJoint m_gripper_configuration_js;

    std::list<ButtonData> m_buttons;
    std::vector<bool> m_button_states;
    vct3 m_raw_linear;
    vct3 m_raw_angular;
};

CMN_DECLARE_SERVICES_INSTANTIATION(mts3Dconnexion);

#endif // _mts3Dconnexion_h
