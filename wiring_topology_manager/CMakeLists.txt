#
# Licensed under Apache License v2. See LICENSE for more information.
#

find_package(CURL REQUIRED)
find_package(Jansson REQUIRED)

include_directories("${CURL_INCLUDE_DIR}")
include_directories("${JANSSON_INCLUDE_DIR}")
include_directories("${CELIX_INCLUDE_DIRS}/remote_service_admin")
include_directories("${PROJECT_SOURCE_DIR}/wiring_common/public/include")
include_directories("private/include")


SET(BUNDLE_SYMBOLICNAME "org.inaetics.wiring_topology_manager")
SET(BUNDLE_VERSION "0.0.1")
SET(BUNDLE_NAME "org.inaetics.wiring_topology_manager.WiringTopologyManager")


bundle(org.inaetics.wiring_topology_manager.WiringTopologyManager SOURCES
	private/src/wiring_topology_manager.c
	private/src/wiring_topology_manager_activator.c
   ${PROJECT_SOURCE_DIR}/wiring_common/private/src/wiring_endpoint_description.c
)

install_bundle(org.inaetics.wiring_topology_manager.WiringTopologyManager)
	
target_link_libraries(org.inaetics.wiring_topology_manager.WiringTopologyManager ${CURL_LIBRARIES} ${JANSSON_LIBRARIES})
