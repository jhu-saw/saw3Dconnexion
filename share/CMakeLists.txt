#
# (C) Copyright 2026 Johns Hopkins University (JHU), All Rights Reserved.
#
# --- begin cisst license - do not edit ---
#
# This software is provided "as is" under an open source license, with
# no warranty.  The complete license can be found in license.txt and
# http://www.cisst.org/cisst/license.txt.
#
# --- end cisst license ---

cmake_minimum_required (VERSION 3.16)
project (three_dconnexion_config VERSION 0.1.0)

# ROS 1
find_package (catkin QUIET)
if (catkin_FOUND)
  catkin_package ()
else (catkin_FOUND)

  # ROS 2
  find_package (ament_cmake QUIET)
  if (ament_cmake_FOUND)
    install (FILES
      saw3Dconnexion-MTMR.json
      saw3Dconnexion-MTMR-Spaceball-5000.json
      system-MTMR-3Dconnexion-PSM1_KIN_SIMULATED-Teleop.json
      system-MTMR-3Dconnexion-PSM1_from_ROS.json
      DESTINATION share/${PROJECT_NAME})

    ament_package ()
  endif (ament_cmake_FOUND)

endif (catkin_FOUND)
