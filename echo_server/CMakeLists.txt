#
# Licensed under Apache License v2. See LICENSE for more information.
#

include_directories("${PROJECT_SOURCE_DIR}/wiring_common/public/include")
include_directories("${CELIX_INCLUDE_DIRS}/remote_service_admin")
include_directories("private/include")
include_directories("public/include")


SET(BUNDLE_SYMBOLICNAME "org.inaetics.wiring_echoServer")
SET(BUNDLE_VERSION "0.0.1")
SET(BUNDLE_NAME "org.inaetics.wiring_echoServer")


bundle(org.inaetics.wiring_echoServer SOURCES
	private/src/activator.c
)

install_bundle(org.inaetics.wiring_echoServer)
	
target_link_libraries(org.inaetics.wiring_echoServer)
