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
project (saw3DconnexionExamples VERSION 0.1.0)

set (REQUIRED_CISST_LIBRARIES cisstCommon
                              cisstCommonQt
                              cisstVector
                              cisstVectorQt
                              cisstOSAbstraction
                              cisstMultiTask
                              cisstMultiTaskQt
                              cisstParameterTypes
                              cisstParameterTypesQt
)

find_package (cisst 1.4.0 REQUIRED COMPONENTS ${REQUIRED_CISST_LIBRARIES})

cisst_set_output_path ()

find_package (saw3Dconnexion 0.1.0)

if (saw3Dconnexion_FOUND AND CISST_HAS_QT)

  include_directories (${saw3Dconnexion_INCLUDE_DIR})
  link_directories (${saw3Dconnexion_LIBRARY_DIR})

  add_executable (saw3DconnexionQtExample main.cpp)
  set_target_properties (saw3DconnexionQtExample PROPERTIES
                         COMPONENTS saw3Dconnexion-Examples
                         FOLDER "saw3Dconnexion")

  target_link_libraries (saw3DconnexionQtExample
                         ${saw3Dconnexion_LIBRARIES}
                         ${QT_LIBRARIES}
                         ${cisst_LIBRARIES})

  install (TARGETS saw3DconnexionQtExample COMPONENT saw3Dconnexion-Examples
           RUNTIME DESTINATION bin
           LIBRARY DESTINATION lib
           ARCHIVE DESTINATION lib)

else (saw3Dconnexion_FOUND AND CISST_HAS_QT)
  message ("Information: code in ${CMAKE_CURRENT_SOURCE_DIR} will not be compiled, it requires saw3Dconnexion Qt support")
endif (saw3Dconnexion_FOUND AND CISST_HAS_QT)
