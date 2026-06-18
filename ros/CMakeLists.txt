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

cmake_minimum_required (VERSION 3.10)
project (three_dconnexion VERSION 0.1.0)

find_package (catkin QUIET
  COMPONENTS
  cisst_ros_crtk
  roscpp
  roslib)

if (catkin_FOUND)
  set (ROS_VERSION 1)
  catkin_package (CATKIN_DEPENDS cisst_ros_crtk roscpp roslib)
  include_directories (${catkin_INCLUDE_DIRS})

else (catkin_FOUND)

  find_package (ament_cmake QUIET)
  if (ament_cmake_FOUND)
    set (ROS_VERSION 2)
    set (ROS2_depends
      rclcpp std_msgs std_srvs geometry_msgs sensor_msgs diagnostic_msgs
      tf2_ros tf2_msgs cisst_msgs crtk_msgs cisst_ros_bridge cisst_ros_crtk)
    foreach (_dep ${ROS2_depends})
      find_package (${_dep} REQUIRED)
    endforeach ()
  endif (ament_cmake_FOUND)
endif (catkin_FOUND)

set (REQUIRED_CISST_LIBRARIES
  cisstCommon
  cisstCommonQt
  cisstVector
  cisstVectorQt
  cisstOSAbstraction
  cisstMultiTask
  cisstMultiTaskQt
  cisstParameterTypes
  cisstParameterTypesQt
  cisstQt)

find_package (cisst 1.4.0 REQUIRED COMPONENTS ${REQUIRED_CISST_LIBRARIES})


cisst_set_output_path ()

find_package (saw3Dconnexion 0.1.0)

if (saw3Dconnexion_FOUND)

  include_directories (${saw3Dconnexion_INCLUDE_DIR} ${three_dconnexion_SOURCE_DIR})
  link_directories (${saw3Dconnexion_LIBRARY_DIR})

  add_executable (three_dconnexion src/three_dconnexion.cpp)
    target_link_libraries (three_dconnexion
      ${saw3Dconnexion_LIBRARIES}
      ${catkin_LIBRARIES}
      ${cisst_LIBRARIES})

  install (TARGETS three_dconnexion
    LIBRARY DESTINATION lib/${PROJECT_NAME}
    RUNTIME DESTINATION lib/${PROJECT_NAME}
    LIBRARY DESTINATION lib
    INCLUDES DESTINATION include)

  if (ROS_VERSION EQUAL 2)
    ament_target_dependencies (
      three_dconnexion
      ${ROS2_depends}
      )
    ament_package ()
  endif ()

else (saw3Dconnexion_FOUND)
  message ("Information: code in ${CMAKE_CURRENT_SOURCE_DIR} will not be compiled, it requires saw3Dconnexion")
endif (saw3Dconnexion_FOUND)
