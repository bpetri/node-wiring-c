/**
 * Licensed under Apache License v2. See LICENSE for more information.
 */

#ifndef REMOTE_SERVICE_ADMIN_HTTP_IMPL_H_
#define REMOTE_SERVICE_ADMIN_HTTP_IMPL_H_

#include "remote_service_admin_impl.h"
#include "wiring_endpoint_description.h"
#include "log_helper.h"
#include "service_tracker.h"


struct remote_service_admin {
	bundle_context_pt context;
	log_helper_pt loghelper;

	celix_thread_mutex_t exportedServicesLock;
	hash_map_pt exportedServices;

	celix_thread_mutex_t importedServicesLock;
	hash_map_pt importedServices;

	hash_map_pt wiringReceiveServices;
	hash_map_pt wiringReceiveServiceRegistrations;

	array_list_pt exportedWires;

	service_tracker_pt sendServicesTracker;
	hash_map_pt sendServices;
};

celix_status_t remoteServiceAdmin_destroy(remote_service_admin_pt *admin);
celix_status_t remoteServiceAdmin_stop(remote_service_admin_pt admin);
celix_status_t remoteServiceAdmin_addImportedWiringEndpoint(void *handle, wiring_endpoint_description_pt wEndpoint, char *matchedFilter);
celix_status_t remoteServiceAdmin_removeImportedWiringEndpoint(void *handle, wiring_endpoint_description_pt wEndpoint, char *matchedFilter);

#endif /* REMOTE_SERVICE_ADMIN_HTTP_IMPL_H_ */
