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
project (saw3Dconnexion VERSION 0.1.0)

set (REQUIRED_CISST_LIBRARIES cisstCommon
                              cisstVector
                              cisstOSAbstraction
                              cisstMultiTask
                              cisstParameterTypes)

find_package (cisst 1.4.0 REQUIRED COMPONENTS ${REQUIRED_CISST_LIBRARIES})



if (NOT CISST_HAS_JSON)
  message (FATAL_ERROR "saw3Dconnexion requires cisst to be built with JSON")
endif ()

cisst_set_output_path ()

find_file (hidapi_HEADER_FILE
           hidapi/hidapi.h
           HINTS /usr/include)

if (hidapi_HEADER_FILE)
  string (REPLACE "/hidapi/hidapi.h" "" hidapi_INCLUDE_DIR ${hidapi_HEADER_FILE})
  include_directories (${hidapi_INCLUDE_DIR})
  set (hidapi_LIBRARIES "hidapi-hidraw")

  set (saw3Dconnexion_INCLUDE_DIR
       "${saw3Dconnexion_SOURCE_DIR}/include"
       "${saw3Dconnexion_BINARY_DIR}/include")
  set (saw3Dconnexion_HEADER_DIR "${saw3Dconnexion_SOURCE_DIR}/include/saw3Dconnexion")
  set (saw3Dconnexion_LIBRARY_DIR "${LIBRARY_OUTPUT_PATH}")
  set (saw3Dconnexion_LIBRARIES saw3Dconnexion ${hidapi_LIBRARIES})

  include_directories (BEFORE ${saw3Dconnexion_INCLUDE_DIR})

  if (CISST_HAS_QT)
    add_subdirectory (code/Qt)
    set (saw3Dconnexion_LIBRARIES ${saw3Dconnexion_LIBRARIES} saw3DconnexionQt)
  endif (CISST_HAS_QT)

  cisst_add_config_files (saw3Dconnexion)

  set (_CDG_FILES
       code/mts3DconnexionConfiguration.cdg)

  cisst_data_generator (saw3Dconnexion
                        "${saw3Dconnexion_BINARY_DIR}/include"
                        "saw3Dconnexion/"
                        ${_CDG_FILES})

  add_library (saw3Dconnexion
               ${saw3Dconnexion_HEADER_DIR}/saw3DconnexionExport.h
               ${saw3Dconnexion_HEADER_DIR}/mts3Dconnexion.h
               code/mts3Dconnexion.cpp
               ${saw3Dconnexion_CISST_DG_SRCS}
               ${saw3Dconnexion_CISST_DG_HDRS})
  set_target_properties (saw3Dconnexion PROPERTIES
                         VERSION ${saw3Dconnexion_VERSION}
                         FOLDER "saw3Dconnexion")
  target_link_libraries (saw3Dconnexion
                         ${hidapi_LIBRARIES}
                         ${cisst_LIBRARIES})

  install (DIRECTORY
           "${saw3Dconnexion_SOURCE_DIR}/include/saw3Dconnexion"
           "${saw3Dconnexion_BINARY_DIR}/include/saw3Dconnexion"
           DESTINATION include
           COMPONENT saw3Dconnexion-dev)

  install (TARGETS saw3Dconnexion COMPONENT saw3Dconnexion
           RUNTIME DESTINATION bin
           LIBRARY DESTINATION lib
           ARCHIVE DESTINATION lib)

else (hidapi_HEADER_FILE)
  message ("Information: code in ${CMAKE_CURRENT_SOURCE_DIR} will not be compiled, it requires hidapi/hidapi.h.  On Ubuntu install libhidapi-dev.")
endif (hidapi_HEADER_FILE)
