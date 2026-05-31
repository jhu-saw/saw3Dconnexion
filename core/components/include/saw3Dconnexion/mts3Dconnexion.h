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

#include <cisstMultiTask/mtsTaskContinuous.h>

#include <list>
#include <string>

class mts3DconnexionData;
class prmConfigurationJoint;
class prmPositionCartesianSet;

#include <saw3Dconnexion/saw3DconnexionExport.h> // always include last

class CISST_EXPORT mts3Dconnexion: public mtsTaskContinuous
{
    CMN_DECLARE_SERVICES(CMN_DYNAMIC_CREATION_ONEARG, CMN_LOG_ALLOW_DEFAULT);

 public:
   mts3Dconnexion(const std::string & componentName);
   mts3Dconnexion(const mtsTaskContinuousConstructorArg & arg);
   ~mts3Dconnexion(void);

    void Configure(const std::string & filename = "") override;
    void Startup(void) override;
    void Run(void) override;
    void Cleanup(void) override;

   bool IsConfigured(void) const;
    void GetButtonNames(std::list<std::string> & result) const;

 protected:
    void Init(void);
    void EnumerateDevices(void) const;
    void ConfigureInterface(void);
    void OpenDevice(void);

    void UpdateTimestamps(const double & timestamp);
    void PollReports(void);
    void ProcessReport(const unsigned char * report, const int length);
    void ProcessMotionReport(const unsigned char * report,
                             const int length,
                             const bool linear);
    void ProcessButtonReport(const unsigned char * report, const int length);
    void IntegrateVirtualPose(const double & period);
    bool ButtonState(const unsigned int & index) const;

    void state_command(const std::string & command);
   void lock_orientation(const bool & lock);
   void lock_position(const bool & lock);
    void move_cp(const prmPositionCartesianSet & position);
    void reset_pose(void);
    void GetConfigurationJs(prmConfigurationJoint & configuration) const;

   mts3DconnexionData * m_data;
};

CMN_DECLARE_SERVICES_INSTANTIATION(mts3Dconnexion);

#endif // _mts3Dconnexion_h
