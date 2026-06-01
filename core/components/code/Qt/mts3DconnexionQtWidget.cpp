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

#include <saw3Dconnexion/mts3DconnexionQtWidget.h>

#include <cisstMultiTask/mtsFunctionRead.h>
#include <cisstMultiTask/mtsFunctionWrite.h>
#include <cisstMultiTask/mtsInterfaceProvided.h>
#include <cisstMultiTask/mtsInterfaceRequired.h>
#include <cisstMultiTask/mtsIntervalStatisticsQtWidget.h>
#include <cisstMultiTask/mtsManagerLocal.h>
#include <cisstMultiTask/mtsMessageQtWidget.h>
#include <cisstParameterTypes/prmEventButtonQtWidget.h>
#include <cisstParameterTypes/prmOperatingStateQtWidget.h>
#include <cisstParameterTypes/prmPositionCartesianGetQtWidget.h>
#include <cisstParameterTypes/prmStateJointQtWidget.h>

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTimerEvent>
#include <QVBoxLayout>

#include <iomanip>
#include <list>
#include <sstream>

CMN_IMPLEMENT_SERVICES_DERIVED_ONEARG(mts3DconnexionQtWidget, mtsComponent, std::string);

namespace {

static QLabel * CreateValueLabel(void)
{
    QLabel * label = new QLabel("0.000, 0.000, 0.000");
    label->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    label->setMinimumWidth(170);
    return label;
}

static QString ToQString(const vct3 & value)
{
    std::stringstream stream;
    stream << std::fixed << std::setprecision(4)
           << value.X() << ", "
           << value.Y() << ", "
           << value.Z();
    return QString::fromStdString(stream.str());
}

} // namespace

mts3DconnexionQtWidget::mts3DconnexionQtWidget(const std::string & componentName,
                                               const double periodInSeconds):
    mtsComponent(componentName),
    TimerPeriodInMilliseconds(static_cast<int>(periodInSeconds / cmn_ms)),
    m_device_interface(nullptr),
    QPCGWidget(nullptr),
    QLLinearVelocity(nullptr),
    QLAngularVelocity(nullptr),
    QSJWidget(nullptr),
    QCBLockOrientation(nullptr),
    QCBLockPosition(nullptr),
    QMIntervalStatistics(nullptr),
    QMMessage(new mtsMessageQtWidget()),
    QPOState(new prmOperatingStateQtWidget()),
    QPBWidgetComponent(nullptr)
{
    this->AddTag("UI");
    m_device_interface = AddInterfaceRequired("Device");
    if (m_device_interface) {
        QMMessage->SetInterfaceRequired(m_device_interface);
        QPOState->SetInterfaceRequired(m_device_interface);
        m_device_interface->AddFunction("measured_cp", Device.measured_cp);
        m_device_interface->AddFunction("measured_cv", Device.measured_cv);
        m_device_interface->AddFunction("gripper/measured_js",
                                        Device.gripper_measured_js, MTS_OPTIONAL);
        m_device_interface->AddFunction("gripper/configuration_js",
                                        Device.gripper_configuration_js, MTS_OPTIONAL);
        m_device_interface->AddFunction("lock_orientation", Device.lock_orientation);
        m_device_interface->AddFunction("lock_position", Device.lock_position);
        m_device_interface->AddFunction("reset_orientation", Device.reset_orientation);
        m_device_interface->AddFunction("reset_position", Device.reset_position);
        m_device_interface->AddFunction("period_statistics", Device.period_statistics);
        m_device_interface->AddFunction("get_button_names", Device.get_button_names);
        m_device_interface->AddEventHandlerWrite(&mts3DconnexionQtWidget::OrientationLockedEventHandler,
                                                 this, "orientation_locked");
        m_device_interface->AddEventHandlerWrite(&mts3DconnexionQtWidget::PositionLockedEventHandler,
                                                 this, "position_locked");
    }

    setupUi();
    startTimer(TimerPeriodInMilliseconds);
}

void mts3DconnexionQtWidget::Configure(const std::string & filename)
{
    CMN_LOG_CLASS_INIT_VERBOSE << "Configure: " << filename << std::endl;
}

void mts3DconnexionQtWidget::Startup(void)
{
    CMN_LOG_CLASS_INIT_VERBOSE << "mts3DconnexionQtWidget::Startup" << std::endl;
    if (!parent()) {
        show();
    }

    if (Device.gripper_configuration_js.IsValid()) {
        const mtsExecutionResult executionResult =
            Device.gripper_configuration_js(m_gripper_configuration_js);
        if (executionResult) {
            QSJWidget->SetConfiguration(m_gripper_configuration_js);
        }
    }

    typedef std::list<std::string> ButtonsType;
    ButtonsType buttons;
    if (Device.get_button_names.IsValid() && Device.get_button_names(buttons)) {
        const mtsInterfaceProvided * connectedInterface = m_device_interface->GetConnectedInterface();
        if (!connectedInterface) {
            CMN_LOG_CLASS_INIT_WARNING << "Startup: Device interface is not connected" << std::endl;
            return;
        }
        mtsManagerLocal * componentManager = mtsManagerLocal::GetInstance();
        const std::string deviceName = connectedInterface->GetComponent()->GetName();
        std::string deviceInterfaceName = connectedInterface->GetName();
        const size_t aliasSeparator = deviceInterfaceName.find('[');
        if (aliasSeparator != std::string::npos) {
            deviceInterfaceName.resize(aliasSeparator);
        }

        const ButtonsType::const_iterator end = buttons.end();
        for (ButtonsType::const_iterator iter = buttons.begin();
             iter != end;
             ++iter) {
            QPBWidgetComponent->AddEventButton(*iter);
        }
        for (ButtonsType::const_iterator iter = buttons.begin();
             iter != end;
             ++iter) {
            componentManager->Connect(QPBWidgetComponent->GetName(), *iter,
                                      deviceName, deviceInterfaceName + "/" + *iter);
        }
    }
}

void mts3DconnexionQtWidget::OrientationLockedEventHandler(const bool & lock)
{
    emit SignalOrientationLocked(lock);
}

void mts3DconnexionQtWidget::PositionLockedEventHandler(const bool & lock)
{
    emit SignalPositionLocked(lock);
}

void mts3DconnexionQtWidget::Cleanup(void)
{
    this->hide();
    CMN_LOG_CLASS_INIT_VERBOSE << "mts3DconnexionQtWidget::Cleanup" << std::endl;
}

void mts3DconnexionQtWidget::closeEvent(QCloseEvent * event)
{
    const int answer = QMessageBox::warning(this, tr("mts3DconnexionQtWidget"),
                                            tr("Do you really want to quit this application?"),
                                            QMessageBox::No | QMessageBox::Yes);
    if (answer == QMessageBox::Yes) {
        event->accept();
        QCoreApplication::exit();
    } else {
        event->ignore();
    }
}

void mts3DconnexionQtWidget::setupUi(void)
{
    QHBoxLayout * mainLayout = new QHBoxLayout;

    QVBoxLayout * controlLayout = new QVBoxLayout;
    mainLayout->addLayout(controlLayout);

    QPCGWidget = new prmPositionCartesianGetQtWidget();
    controlLayout->addWidget(QPCGWidget);

    QGroupBox * velocityBox = new QGroupBox("Cartesian velocity");
    QGridLayout * velocityLayout = new QGridLayout;
    velocityBox->setLayout(velocityLayout);
    velocityLayout->addWidget(new QLabel("linear"), 0, 0);
    QLLinearVelocity = CreateValueLabel();
    velocityLayout->addWidget(QLLinearVelocity, 0, 1);
    velocityLayout->addWidget(new QLabel("angular"), 1, 0);
    QLAngularVelocity = CreateValueLabel();
    velocityLayout->addWidget(QLAngularVelocity, 1, 1);
    controlLayout->addWidget(velocityBox);

    QSJWidget = new prmStateJointQtWidget();
    controlLayout->addWidget(QSJWidget);

    QPBWidgetComponent = new prmEventButtonQtWidgetComponent(GetName() + "-buttons");
    QPBWidgetComponent->SetNumberOfColumns(2);
    mtsManagerLocal * componentManager = mtsManagerLocal::GetInstance();
    componentManager->AddComponent(QPBWidgetComponent);
    controlLayout->addWidget(QPBWidgetComponent);

    QGridLayout * commandLayout = new QGridLayout;
    QCBLockOrientation = new QCheckBox("Lock Orientation");
    commandLayout->addWidget(QCBLockOrientation, 0, 0);
    QCBLockPosition = new QCheckBox("Lock Position");
    commandLayout->addWidget(QCBLockPosition, 0, 1);
    QPushButton * resetOrientationButton = new QPushButton("Reset Orientation");
    commandLayout->addWidget(resetOrientationButton, 1, 0);
    connect(resetOrientationButton, &QPushButton::clicked,
            [this](void) { this->SlotResetOrientation(); });
    QPushButton * resetPositionButton = new QPushButton("Reset Position");
    commandLayout->addWidget(resetPositionButton, 1, 1);
    connect(resetPositionButton, &QPushButton::clicked,
            [this](void) { this->SlotResetPosition(); });
    controlLayout->addLayout(commandLayout);
    controlLayout->addStretch();

    QVBoxLayout * systemLayout = new QVBoxLayout();
    mainLayout->addLayout(systemLayout);

    QMIntervalStatistics = new mtsIntervalStatisticsQtWidget();
    systemLayout->addWidget(QMIntervalStatistics);

    QMMessage->setupUi();
    systemLayout->addWidget(QMMessage);

    QPOState->setupUi();
    systemLayout->addWidget(QPOState);
    systemLayout->addStretch();

    setLayout(mainLayout);
    setWindowTitle("saw3Dconnexion");
    resize(sizeHint());

        connect(QCBLockOrientation, SIGNAL(clicked(bool)),
            this, SLOT(SlotLockOrientation(bool)));
        connect(QCBLockPosition, SIGNAL(clicked(bool)),
            this, SLOT(SlotLockPosition(bool)));
        connect(this, SIGNAL(SignalOrientationLocked(bool)),
            this, SLOT(SlotOrientationLockedEventHandler(bool)));
        connect(this, SIGNAL(SignalPositionLocked(bool)),
            this, SLOT(SlotPositionLockedEventHandler(bool)));
}

void mts3DconnexionQtWidget::timerEvent(QTimerEvent * CMN_UNUSED(event))
{
    if (this->isHidden()) {
        return;
    }

    mtsExecutionResult executionResult;
    executionResult = Device.measured_cp(m_measured_cp);
    if (executionResult) {
        QPCGWidget->SetValue(m_measured_cp);
    }

    executionResult = Device.measured_cv(m_measured_cv);
    if (executionResult) {
        QLLinearVelocity->setText(ToQString(m_measured_cv.VelocityLinear()));
        QLAngularVelocity->setText(ToQString(m_measured_cv.VelocityAngular()));
    }

    if (Device.gripper_measured_js.IsValid()) {
        executionResult = Device.gripper_measured_js(m_gripper_measured_js);
        if (executionResult) {
            QSJWidget->SetValue(m_gripper_measured_js);
        }
    }

    if (Device.period_statistics.IsValid()) {
        executionResult = Device.period_statistics(IntervalStatistics);
        if (executionResult) {
            QMIntervalStatistics->SetValue(IntervalStatistics);
        }
    }
}

void mts3DconnexionQtWidget::SlotResetOrientation(void)
{
    if (Device.reset_orientation.IsValid()) {
        Device.reset_orientation();
    }
}

void mts3DconnexionQtWidget::SlotResetPosition(void)
{
    if (Device.reset_position.IsValid()) {
        Device.reset_position();
    }
}

void mts3DconnexionQtWidget::SlotLockOrientation(bool lock)
{
    if (Device.lock_orientation.IsValid()) {
        Device.lock_orientation(lock);
    }
}

void mts3DconnexionQtWidget::SlotLockPosition(bool lock)
{
    if (Device.lock_position.IsValid()) {
        Device.lock_position(lock);
    }
}

void mts3DconnexionQtWidget::SlotOrientationLockedEventHandler(bool lock)
{
    if (QCBLockOrientation) {
        QCBLockOrientation->setChecked(lock);
    }
}

void mts3DconnexionQtWidget::SlotPositionLockedEventHandler(bool lock)
{
    if (QCBLockPosition) {
        QCBLockPosition->setChecked(lock);
    }
}
