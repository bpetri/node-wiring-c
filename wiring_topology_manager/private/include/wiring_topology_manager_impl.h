/**
 * Licensed under Apache License v2. See LICENSE for more information.
 */

#ifndef WIRING_TOPOLOGY_MANAGER_IMPL_H_
#define WIRING_TOPOLOGY_MANAGER_IMPL_H_

#include "wiring_topology_manager.h"
#include "wiring_endpoint_listener.h"

#include "bundle_context.h"


celix_status_t wiringTopologyManager_create(bundle_context_pt context, wiring_topology_manager_pt *manager);
celix_status_t wiringTopologyManager_destroy(wiring_topology_manager_pt manager);

celix_status_t wiringTopologyManager_waAdding(void *handle, service_reference_pt reference, void **service);
celix_status_t wiringTopologyManager_waAdded(void *handle, service_reference_pt reference, void *service);
celix_status_t wiringTopologyManager_waModified(void *handle, service_reference_pt reference, void *service);
celix_status_t wiringTopologyManager_waRemoved(void *handle, service_reference_pt reference, void *service);

celix_status_t wiringTopologyManager_wiringEndpointListenerAdding(void *handle, service_reference_pt reference, void **service);
celix_status_t wiringTopologyManager_wiringEndpointListenerAdded(void *handle, service_reference_pt reference, void *service);
celix_status_t wiringTopologyManager_wiringEndpointListenerModified(void *handle, service_reference_pt reference, void *service);
celix_status_t wiringTopologyManager_wiringEndpointListenerRemoved(void *handle, service_reference_pt reference, void *service);


celix_status_t wiringTopologyManager_WiringEndpointAdded(void *handle, wiring_endpoint_description_pt endpoint, char *matchedFilter);
celix_status_t wiringTopologyManager_WiringEndpointRemoved(void *handle, wiring_endpoint_description_pt endpoint, char *matchedFilter);

celix_status_t wiringTopologyManager_exportWiringEndpoint(wiring_topology_manager_pt manager, properties_pt properties);
celix_status_t wiringTopologyManager_removeExportedWiringEndpoint(wiring_topology_manager_pt manager, properties_pt properties);

celix_status_t wiringTopologyManager_importWiringEndpoint(wiring_topology_manager_pt manager, properties_pt properties);
celix_status_t wiringTopologyManager_removeImportedWiringEndpoint(wiring_topology_manager_pt manager, properties_pt properties);


#endif /* WIRING_TOPOLOGY_MANAGER_IMPL_H_ */
