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

#ifndef _mts3DconnexionQtWidget_h
#define _mts3DconnexionQtWidget_h

#include <cisstCommon/cmnUnits.h>
#include <cisstMultiTask/mtsComponent.h>
#include <cisstMultiTask/mtsFunctionVoid.h>
#include <cisstMultiTask/mtsFunctionWrite.h>
#include <cisstMultiTask/mtsForwardDeclarationsQt.h>
#include <cisstMultiTask/mtsIntervalStatistics.h>
#include <cisstParameterTypes/prmConfigurationJoint.h>
#include <cisstParameterTypes/prmForwardDeclarationsQt.h>
#include <cisstParameterTypes/prmPositionCartesianGet.h>
#include <cisstParameterTypes/prmStateJoint.h>
#include <cisstParameterTypes/prmVelocityCartesianGet.h>

#include <QWidget>

#include <saw3Dconnexion/saw3DconnexionQtExport.h> // always include last

class QCheckBox;
class QLabel;
class QCloseEvent;
class QTimerEvent;

class CISST_EXPORT mts3DconnexionQtWidget : public QWidget, public mtsComponent
{
    Q_OBJECT;
    CMN_DECLARE_SERVICES(CMN_DYNAMIC_CREATION_ONEARG, CMN_LOG_ALLOW_DEFAULT);

 public:
    mts3DconnexionQtWidget(const std::string & componentName,
                           const double periodInSeconds = 50.0 * cmn_ms);
    ~mts3DconnexionQtWidget() {}

    void Configure(const std::string & filename = "") override;
    void Startup(void) override;
    void Cleanup(void) override;

 protected:
    void closeEvent(QCloseEvent * event) override;
    void timerEvent(QTimerEvent * event) override;

 signals:
    void SignalOrientationLocked(bool locked);
    void SignalPositionLocked(bool locked);

 private slots:
    void SlotLockOrientation(bool lock);
    void SlotLockPosition(bool lock);
    void SlotOrientationLockedEventHandler(bool lock);
    void SlotPositionLockedEventHandler(bool lock);
    void SlotResetOrientation(void);
    void SlotResetPosition(void);

 private:
    void setupUi(void);
    void OrientationLockedEventHandler(const bool & lock);
    void PositionLockedEventHandler(const bool & lock);

    int TimerPeriodInMilliseconds;

    mtsInterfaceRequired * m_device_interface;

    struct {
        mtsFunctionRead measured_cp;
        mtsFunctionRead measured_cv;
        mtsFunctionRead gripper_measured_js;
        mtsFunctionRead gripper_configuration_js;
        mtsFunctionWrite state_command;
        mtsFunctionWrite lock_orientation;
        mtsFunctionWrite lock_position;
        mtsFunctionVoid reset_orientation;
        mtsFunctionVoid reset_position;
        mtsFunctionRead period_statistics;
        mtsFunctionRead get_button_names;
    } Device;

    prmPositionCartesianGet m_measured_cp;
    prmVelocityCartesianGet m_measured_cv;
    prmStateJoint m_gripper_measured_js;
    prmConfigurationJoint m_gripper_configuration_js;

    prmPositionCartesianGetQtWidget * QPCGWidget;
    QLabel * QLLinearVelocity;
    QLabel * QLAngularVelocity;
    prmStateJointQtWidget * QSJWidget;
    QCheckBox * QCBLockOrientation;
    QCheckBox * QCBLockPosition;

    mtsIntervalStatistics IntervalStatistics;
    mtsIntervalStatisticsQtWidget * QMIntervalStatistics;

    mtsMessageQtWidget * QMMessage;
    prmOperatingStateQtWidget * QPOState;
    prmEventButtonQtWidgetComponent * QPBWidgetComponent;
};

CMN_DECLARE_SERVICES_INSTANTIATION(mts3DconnexionQtWidget);

#endif // _mts3DconnexionQtWidget_h
