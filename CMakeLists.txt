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

# 3DconnexionClient is required for Mac OS only
if (APPLE)

  # To find Find3Dconnexion.cmake
  set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/code)

  find_package (3DconnexionClient REQUIRED)

  if (3DconnexionClient_FOUND)
    add_subdirectory (code)
    add_subdirectory (examples)
  else (3DconnexionClient_FOUND)
    message (SEND_ERROR "code in ${CMAKE_CURRENT_SOURCE_DIR} requires 3DconnexionClient.")
  endif (3DconnexionClient_FOUND)

else (APPLE)

  add_subdirectory (code)
  add_subdirectory (examples)

endif (APPLE)