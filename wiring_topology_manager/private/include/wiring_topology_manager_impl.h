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

celix_status_t wiringTopologyManager_addImportedWiringEndpoint(void *handle, wiring_endpoint_description_pt endpoint, char *matchedFilter);
celix_status_t wiringTopologyManager_removeImportedWiringEndpoint(void *handle, wiring_endpoint_description_pt endpoint, char *matchedFilter);

celix_status_t wiringTopologyManager_installCallbackToWiringEndpoint(wiring_topology_manager_pt manager, properties_pt properties, rsa_inaetics_receive_cb rsa_inaetics_cb);
celix_status_t wiringTopologyManager_uninstallCallbackFromWiringEndpoint(wiring_topology_manager_pt manager, rsa_inaetics_receive_cb rsa_inaetics_cb);

celix_status_t wiringTopologyManager_getWiringProxy(wiring_topology_manager_pt manager, char* wireId, wiring_admin_pt* admin,rsa_inaetics_send* sendFunc,wiring_handle* handle);
celix_status_t wiringTopologyManager_removeWiringProxy(wiring_topology_manager_pt manager, wiring_handle handle);



#endif /* WIRING_TOPOLOGY_MANAGER_IMPL_H_ */
