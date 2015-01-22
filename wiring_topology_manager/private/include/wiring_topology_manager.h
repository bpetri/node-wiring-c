/**
 *Licensed to the Apache Software Foundation (ASF) under one
 *or more contributor license agreements.  See the NOTICE file
 *distributed with this work for additional information
 *regarding copyright ownership.  The ASF licenses this file
 *to you under the Apache License, Version 2.0 (the
 *"License"); you may not use this file except in compliance
 *with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *Unless required by applicable law or agreed to in writing,
 *software distributed under the License is distributed on an
 *"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 *specific language governing permissions and limitations
 *under the License.
 */
/*
 * wiring_topology_manager.h
 *
 *  \date       Sep 29, 2011
 *  \author    	<a href="mailto:celix-dev@incubator.apache.org">Apache Celix Project Team</a>
 *  \copyright	Apache License, Version 2.0
 */

#ifndef WIRING_TOPOLOGY_MANAGER_H_
#define WIRING_TOPOLOGY_MANAGER_H_

#include "wiring_endpoint_listener.h"

#include "bundle_context.h"

typedef struct wiring_topology_manager* wiring_topology_manager_pt;

celix_status_t wiringTopologyManager_create(bundle_context_pt context, wiring_topology_manager_pt *manager);
celix_status_t wiringTopologyManager_destroy(wiring_topology_manager_pt manager);

celix_status_t wiringTopologyManager_waAdding(void *handle, service_reference_pt reference, void **service);
celix_status_t wiringTopologyManager_waAdded(void *handle, service_reference_pt reference, void *service);
celix_status_t wiringTopologyManager_waModified(void *handle, service_reference_pt reference, void *service);
celix_status_t wiringTopologyManager_waRemoved(void *handle, service_reference_pt reference, void *service);

celix_status_t wiringTopologyManager_addImportedWiringEndpoint(void *handle, wiring_endpoint_description_pt endpoint, char *matchedFilter);
celix_status_t wiringTopologyManager_removeImportedWiringEndpoint(void *handle, wiring_endpoint_description_pt endpoint, char *matchedFilter);

celix_status_t topologyManager_addExportedService(wiring_topology_manager_pt manager, service_reference_pt reference, char *serviceId);
celix_status_t topologyManager_removeExportedService(wiring_topology_manager_pt manager, service_reference_pt reference, char *serviceId);


#endif /* WIRING_TOPOLOGY_MANAGER_H_ */
