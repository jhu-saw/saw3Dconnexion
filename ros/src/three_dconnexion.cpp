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

#include <cisstCommon/cmnUnits.h>
#include <cisstMultiTask/mtsCommandLineOptionsQt.h>
#include <cisstMultiTask/mtsTaskManager.h>
#include <saw3Dconnexion/mts3Dconnexion.h>
#include <saw3Dconnexion/mts3DconnexionQtWidget.h>

#include <cisst_ros_crtk/mts_ros_crtk_bridge.h>
#include <cisst_ros_bridge/mtsROSBridge.h>

#include <QApplication>
#include <QTabWidget>

#include <iostream>

int main(int argc, char * argv[])
{
    cmnLogger::SetMask(CMN_LOG_ALLOW_ALL);
    cmnLogger::SetMaskFunction(CMN_LOG_ALLOW_ALL);
    cmnLogger::SetMaskDefaultLog(CMN_LOG_ALLOW_ALL);
    cmnLogger::SetMaskClassMatching("mts3Dconnexion", CMN_LOG_ALLOW_ALL);
    cmnLogger::SetMaskClassMatching("mts3DconnexionQtWidget", CMN_LOG_ALLOW_ALL);
    cmnLogger::AddChannel(std::cerr, CMN_LOG_ALLOW_ERRORS_AND_WARNINGS);

    cisst_ral::ral ral(argc, argv, "three_dconnexion");
    auto rosNode = ral.node();

    mtsCommandLineOptionsQt options;
    std::string jsonConfigFile = "";
    std::string componentName = "3Dconnexion";
    double rosPeriod = 2.0 * cmn_ms;
    double tfPeriod = 20.0 * cmn_ms;

    options.AddOptionOneValue("j", "json-config",
                              "json configuration file",
                              cmnCommandLineOptions::OPTIONAL_OPTION, &jsonConfigFile);
    options.AddOptionOneValue("n", "component-name",
                              "component name",
                              cmnCommandLineOptions::OPTIONAL_OPTION, &componentName);
    options.AddOptionOneValue("p", "ros-period",
                              "period in seconds to publish CRTK topics (default 0.002, 2 ms, 500 Hz)",
                              cmnCommandLineOptions::OPTIONAL_OPTION, &rosPeriod);
    options.AddOptionOneValue("P", "tf-ros-period",
                              "period in seconds to broadcast tf2 (default 0.02, 20 ms, 50 Hz)",
                              cmnCommandLineOptions::OPTIONAL_OPTION, &tfPeriod);

    if (!options.Parse(ral.stripped_arguments(), std::cerr)) {
        return -1;
    }

    std::string arguments;
    options.PrintParsedArguments(arguments);
    std::cout << "Options provided:" << std::endl << arguments << std::endl;
    if (jsonConfigFile == "") {
        CMN_LOG_INIT_WARNING << "No JSON configuration file provided; using built-in defaults"
                             << std::endl;
    } else {
        std::cout << "Using JSON configuration \"" << jsonConfigFile << "\"" << std::endl;
    }

    mts3Dconnexion * spaceMouse = new mts3Dconnexion(componentName);
    spaceMouse->Configure(jsonConfigFile);
    if (!spaceMouse->IsConfigured()) {
        CMN_LOG_INIT_ERROR << "Configure: failed to configure 3Dconnexion component"
                           << std::endl;
        delete spaceMouse;
        return -1;
    }
    const std::string deviceName = spaceMouse->GetDeviceName();
    std::cout << "Using component \"" << componentName
              << "\" and device \"" << deviceName << "\"" << std::endl;

    mtsManagerLocal * componentManager = mtsComponentManager::GetInstance();
    componentManager->AddComponent(spaceMouse);

    mts_ros_crtk_bridge_provided * crtkBridge =
        new mts_ros_crtk_bridge_provided("three_dconnexion_crtk_bridge", rosNode);
    componentManager->AddComponent(crtkBridge);

    QApplication application(argc, argv);

    QTabWidget * tabWidget = new QTabWidget;
    mts3DconnexionQtWidget * deviceWidget =
        new mts3DconnexionQtWidget(deviceName + "-gui");
    deviceWidget->Configure();
    componentManager->AddComponent(deviceWidget);
    componentManager->Connect(deviceWidget->GetName(), "Device",
                              spaceMouse->GetName(), deviceName);
    tabWidget->addTab(deviceWidget, deviceName.c_str());

    crtkBridge->bridge_all_interfaces_provided(spaceMouse->GetName(), "",
                                               rosPeriod, tfPeriod);

    // extra void commands not covered by CRTK auto-bridging
    {
        const std::string req = mts_ros_crtk_bridge_provided::required_interface_name_for(
            spaceMouse->GetName(), deviceName);
        crtkBridge->subscribers_bridge().AddSubscriberToCommandVoid(
            req, "reset_orientation", deviceName + "/reset_orientation");
        crtkBridge->subscribers_bridge().AddSubscriberToCommandVoid(
            req, "reset_position", deviceName + "/reset_position");
    }

    crtkBridge->Connect();

    options.Apply();

    componentManager->CreateAllAndWait(5.0 * cmn_s);
    componentManager->StartAllAndWait(5.0 * cmn_s);

    tabWidget->show();
    application.exec();

    cmnLogger::Kill();
    cisst_ral::shutdown();

    componentManager->KillAllAndWait(5.0 * cmn_s);
    componentManager->Cleanup();

    return 0;
}
