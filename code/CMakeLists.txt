#
# $Id$
#
# (C) Copyright 2011 Johns Hopkins University (JHU), All Rights
# Reserved.
#
# --- begin cisst license - do not edit ---
#
# This software is provided "as is" under an open source license, with
# no warranty.  The complete license can be found in license.txt and
# http://www.cisst.org/cisst/license.txt.
#
# --- end cisst license ---

# Core library
set (REQUIRED_CISST_LIBRARIES cisstCommon
                              cisstVector
                              cisstOSAbstraction
                              cisstMultiTask
                              cisstParameterTypes)

find_package (cisst REQUIRED ${REQUIRED_CISST_LIBRARIES})

if (cisst_FOUND_AS_REQUIRED)

  # load cisst configuration
  include (${CISST_USE_FILE})

  add_library (saw3Dconnexion
               ${IS_SHARED}
               ../mts3Dconnexion.h
               ../saw3DconnexionExport.h
               mts3Dconnexion.cpp)
  cisst_target_link_libraries (saw3Dconnexion ${REQUIRED_CISST_LIBRARIES})

  # create/configure file for find_package (saw3Dconnexion)
  set (saw3Dconnexion_INCLUDE_DIR "${sawComponents_SOURCE_DIR}")
  set (saw3Dconnexion_LIBRARY_DIR "${LIBRARY_OUTPUT_PATH}")
  set (saw3Dconnexion_LIBRARIES saw3Dconnexion)

  configure_file ("${CMAKE_CURRENT_SOURCE_DIR}/saw3DconnexionConfig.cmake.in"
                  "${sawComponents_BINARY_DIR}/saw3DconnexionConfig.cmake"
                   @ONLY@)

else (cisst_FOUND_AS_REQUIRED)
  message ("Information: code in ${CMAKE_CURRENT_SOURCE_DIR} will not be compiled, it requires ${REQUIRED_CISST_LIBRARIES}")
endif (cisst_FOUND_AS_REQUIRED)
