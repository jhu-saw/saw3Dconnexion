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

set (REQUIRED_CISST_LIBRARIES
     cisstCommon
     cisstVector
     cisstVectorQt
     cisstOSAbstraction
     cisstMultiTask
     cisstMultiTaskQt
     cisstParameterTypes
     cisstParameterTypesQt
     cisstQt)

find_package (cisst REQUIRED COMPONENTS ${REQUIRED_CISST_LIBRARIES})



include_directories (${CMAKE_CURRENT_BINARY_DIR})
if (CISST_HAS_QT4)
  qt4_wrap_cpp (SAW_3D_CONNEXION_QT_WRAP_CPP
                ${saw3Dconnexion_HEADER_DIR}/mts3DconnexionQtWidget.h)
else (CISST_HAS_QT4)
  set (CMAKE_AUTOMOC ON)
endif (CISST_HAS_QT4)

add_library (saw3DconnexionQt
             ${saw3Dconnexion_HEADER_DIR}/saw3DconnexionQtExport.h
             ${saw3Dconnexion_HEADER_DIR}/mts3DconnexionQtWidget.h
             mts3DconnexionQtWidget.cpp
             ${SAW_3D_CONNEXION_QT_WRAP_CPP})
set_target_properties (saw3DconnexionQt PROPERTIES
                       VERSION ${saw3Dconnexion_VERSION}
                       FOLDER "saw3Dconnexion")
target_link_libraries (saw3DconnexionQt ${cisst_LIBRARIES})

install (TARGETS saw3DconnexionQt COMPONENT saw3Dconnexion-Qt
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib)

