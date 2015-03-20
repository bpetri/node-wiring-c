#
# Licensed under Apache License v2. See LICENSE for more information.
#

find_package(CURL REQUIRED)

include_directories(${CURL_INCLUDE_DIR})
include_directories("${CELIX_INCLUDE_DIRS}/remote_service_admin")
include_directories("${PROJECT_SOURCE_DIR}/wiring_common/public/include")
include_directories("${PROJECT_SOURCE_DIR}/wiring_common/private/include")
include_directories("private/include")
# supposed to be the rsa
include_directories("${PROJECT_SOURCE_DIR}/echo_server/public/include")
include_directories("${CELIX_INCLUDE_DIRS}/shell")


SET(BUNDLE_SYMBOLICNAME "org.inaetics.wiring_admin")
SET(BUNDLE_VERSION "0.0.1")
SET(BUNDLE_NAME "org.inaetics.wiring_admin.WiringAdmin")

bundle(org.inaetics.wiring_admin.WiringAdmin SOURCES 
	private/src/civetweb.c
	private/src/wiring_admin_impl
	private/src/wiring_admin_activator
   ${PROJECT_SOURCE_DIR}/wiring_common/private/src/wiring_common_utils.c
   ${PROJECT_SOURCE_DIR}/wiring_common/private/src/wiring_endpoint_description.c
)

install_bundle(org.inaetics.wiring_admin.WiringAdmin)
    
target_link_libraries(org.inaetics.wiring_admin.WiringAdmin ${CURL_LIBRARIES})
