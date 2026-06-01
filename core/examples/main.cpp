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

    mtsCommandLineOptionsQt options;
    std::string jsonConfigFile = "";
    std::string componentName = "3Dconnexion";

    options.AddOptionOneValue("j", "json-config",
                              "json configuration file",
                              cmnCommandLineOptions::OPTIONAL_OPTION, &jsonConfigFile);
    options.AddOptionOneValue("n", "component-name",
                              "component name",
                              cmnCommandLineOptions::OPTIONAL_OPTION, &componentName);

    if (!options.Parse(argc, argv, std::cerr)) {
        return -1;
    }

    std::string arguments;
    options.PrintParsedArguments(arguments);
    std::cout << "Options provided:" << std::endl << arguments << std::endl;
    if (jsonConfigFile == "") {
        CMN_LOG_INIT_WARNING << "No JSON configuration file provided; using built-in defaults.  "
                             << "For the sample MTMR configuration use "
                             << "-j <install-prefix>/share/saw3Dconnexion/saw3Dconnexion-MTMR.json"
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

    QApplication application(argc, argv);

    QTabWidget * tabWidget = new QTabWidget;
    mts3DconnexionQtWidget * deviceWidget =
        new mts3DconnexionQtWidget(deviceName + "-gui");
    deviceWidget->Configure();
    componentManager->AddComponent(deviceWidget);
    componentManager->Connect(deviceWidget->GetName(), "Device",
                              spaceMouse->GetName(), deviceName);
    tabWidget->addTab(deviceWidget, deviceName.c_str());

    options.Apply();

    componentManager->CreateAllAndWait(5.0 * cmn_s);
    componentManager->StartAllAndWait(5.0 * cmn_s);

    tabWidget->show();
    application.exec();

    cmnLogger::Kill();

    componentManager->KillAllAndWait(5.0 * cmn_s);
    componentManager->Cleanup();

    return 0;
}
