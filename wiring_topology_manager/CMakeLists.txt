# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
# 
#   http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

find_package(CURL REQUIRED)
find_package(Jansson REQUIRED)

include_directories("${CURL_INCLUDE_DIR}")
include_directories("${JANSSON_INCLUDE_DIR}")
include_directories("${CELIX_INCLUDE_DIRS}/log_service")
include_directories("${CELIX_INCLUDE_DIRS}/remote_service_admin")
include_directories("${PROJECT_SOURCE_DIR}/wiring_endpoint/public/include")
include_directories("private/include")


SET(BUNDLE_SYMBOLICNAME "org.inaetics.wiring_topology_manager")
SET(BUNDLE_VERSION "0.0.1")
SET(BUNDLE_NAME "org.inaetics.wiring_topology_manager.WiringTopologyManager")


bundle(org.inaetics.wiring_topology_manager.WiringTopologyManager SOURCES
	private/src/wiring_topology_manager.c
	private/src/wiring_topology_manager_activator.c
)

install_bundle(org.inaetics.wiring_topology_manager.WiringTopologyManager)
	
target_link_libraries(org.inaetics.wiring_topology_manager.WiringTopologyManager ${CURL_LIBRARIES} ${JANSSON_LIBRARIES})
