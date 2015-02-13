/*
 * wiring_topology_manager.h
 *
 *  Created on: Feb 11, 2015
 *      Author: dn234
 */

#ifndef WIRING_TOPOLOGY_MANAGER_H_
#define WIRING_TOPOLOGY_MANAGER_H_

#include "wiring_endpoint_listener.h"
#include "wiring_admin.h"
#include "celix_errno.h"

static const char * const OSGI_WIRING_TOPOLOGY_MANAGER_SERVICE = "wiring_topology_manager";

static const char * const OSGI_WIRING_TOPOLOGY_MANAGER_SCOPE = "wiring.topology_manager.scope";

typedef struct wiring_topology_manager* wiring_topology_manager_pt;

struct wiring_topology_manager_service {
	wiring_topology_manager_pt manager;

	celix_status_t (*installCallbackToWiringEndpoint)(wiring_topology_manager_pt manager,properties_pt properties, celix_status_t(*rsa_inetics)(char* data, char**response));
	celix_status_t (*uninstallCallbackFromWiringEndpoint)(wiring_topology_manager_pt manager, celix_status_t(*rsa_inetics)(char* data, char**response));

	celix_status_t (*getWiringProxy)(wiring_topology_manager_pt manager,properties_pt properties, wiring_admin_pt* admin, celix_status_t(**send)(wiring_admin_pt admin,void* handle, char *request, char **reply, int* replyStatus),void** handle);
	celix_status_t (*removeWiringProxy)(wiring_topology_manager_pt manager, void* handle);

};

typedef struct wiring_topology_manager_service *wiring_topology_manager_service_pt;

#endif /* WIRING_TOPOLOGY_MANAGER_H_ */