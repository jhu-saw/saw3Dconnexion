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
project (saw3DconnexionCore VERSION 0.1.0)

find_package (cisst 1.5 REQUIRED)
cisst_cpack_settings (
  VENDOR "JHU"
  MAINTAINER "anton.deguet@jhu.edu")

add_subdirectory (components)

set (saw3Dconnexion_DIR "${saw3DconnexionCore_BINARY_DIR}/components")
add_subdirectory (examples)

include (CPack)
cpack_add_component (saw3Dconnexion)
cpack_add_component (saw3Dconnexion-dev
                     DEPENDS saw3Dconnexion)
cpack_add_component (saw3Dconnexion-Qt
                     DEPENDS saw3Dconnexion)
cpack_add_component (saw3Dconnexion-Examples
                     DEPENDS saw3Dconnexion saw3Dconnexion-Qt)
