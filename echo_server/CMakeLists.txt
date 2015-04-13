#
# Licensed under Apache License v2. See LICENSE for more information.
#

include_directories("${PROJECT_SOURCE_DIR}/wiring_common/public/include")
include_directories("${CELIX_INCLUDE_DIRS}/remote_service_admin")
include_directories("${PROJECT_SOURCE_DIR}/remote_service_admin_inaetics/public/include")
include_directories("${CELIX_INCLUDE_DIRS}/shell")
include_directories("private/include")
include_directories("public/include")


SET(BUNDLE_SYMBOLICNAME "org.inaetics.wiring_echoServer")
SET(BUNDLE_VERSION "0.0.1")
SET(BUNDLE_NAME "org.inaetics.wiring_echoServer")


bundle(org.inaetics.wiring_echoServer SOURCES
	private/src/activator.c
	private/src/send_command.c
	private/src/export_command.c
	${CELIX_DIR}/share/celix/shell/command.c
)

install_bundle(org.inaetics.wiring_echoServer)
	
target_link_libraries(org.inaetics.wiring_echoServer)
