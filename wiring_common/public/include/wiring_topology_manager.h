/**
 * Licensed under Apache License v2. See LICENSE for more information.
 */

#ifndef WIRING_TOPOLOGY_MANAGER_H_
#define WIRING_TOPOLOGY_MANAGER_H_

#include "wiring_endpoint_listener.h"
#include "wiring_admin.h"
#include "celix_errno.h"

static const char * const INAETICS_WIRING_TOPOLOGY_MANAGER_SERVICE = "wiring_topology_manager";

static const char * const INAETICS_WIRING_TOPOLOGY_MANAGER_SCOPE = "wiring.topology_manager.scope";

typedef struct wiring_topology_manager* wiring_topology_manager_pt;

struct wiring_topology_manager_service {
	wiring_topology_manager_pt manager;

	celix_status_t (*installCallbackToWiringEndpoint)(wiring_topology_manager_pt manager,properties_pt properties, rsa_inaetics_receive_cb rsa_inaetics_cb);
	celix_status_t (*uninstallCallbackFromWiringEndpoint)(wiring_topology_manager_pt manager, rsa_inaetics_receive_cb rsa_inaetics_cb);

	celix_status_t (*getWiringProxy)(wiring_topology_manager_pt manager, char* wireId, wiring_admin_pt* admin, rsa_inaetics_send* sendFunc, wiring_handle* handle);
	celix_status_t (*removeWiringProxy)(wiring_topology_manager_pt manager, wiring_handle handle);

};

typedef struct wiring_topology_manager_service *wiring_topology_manager_service_pt;

#endif /* WIRING_TOPOLOGY_MANAGER_H_ */
