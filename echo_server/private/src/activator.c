/**
 * Licensed under Apache License v2. See LICENSE for more information.
 */

#include <stdlib.h>

#include <celix_errno.h>

#include "bundle_activator.h"
#include "bundle_context.h"
#include "service_tracker.h"
#include "properties.h"

#include "wiring_topology_manager.h"

struct echoActivator {

};

celix_status_t bundleActivator_create(bundle_context_pt context, void **userData) {
//	struct echoActivator * act = calloc(1, sizeof(*act));

	//*userData = act;

	return CELIX_SUCCESS;
}



celix_status_t echo_callback(char* data, char**response) {
	celix_status_t status = CELIX_SUCCESS;

	printf("ECHO_SERVER: echo_callback called.\n");
	printf("ECHO_SERVER: received data: %s\n", data);

	return status;
}


celix_status_t bundleActivator_start(void * userData, bundle_context_pt context) {

//	struct activator *activator = userData;
	service_reference_pt wtmServiceRef = NULL;
	celix_status_t status = CELIX_SUCCESS;

	status = bundleContext_getServiceReference(context, (char *) INAETICS_WIRING_TOPOLOGY_MANAGER_SERVICE, &wtmServiceRef);

	if (status == CELIX_SUCCESS && wtmServiceRef != NULL) {
		wiring_topology_manager_service_pt wtmService = NULL;
		bundleContext_getService(context, wtmServiceRef, &wtmService);

		// if the dereferenced instance is null then we know the service has been removed
		if (wtmService != NULL) {
			properties_pt someProperties = properties_create();

			if ( wtmService->installCallbackToWiringEndpoint(wtmService->manager, someProperties, echo_callback) == CELIX_SUCCESS) {
				printf("ECHO_SERVER: Callback sucessfully installed\n");
			}
			else {
				printf("ECHO_SERVER: Installation of Callback failed\n");
			}


			printf("ECHO_SERVER: Try to send sth..\n");
			wiring_admin_pt admin = NULL;
			rsa_inaetics_send sendFunc = NULL;
			wiring_handle handle = NULL;

			properties_set(someProperties, WIRING_ENDPOINT_DESCRIPTION_URL_KEY, "http://localhost:8081/org.inaetics.wiring.admin.http/echoService");

			if ( wtmService->getWiringProxy(wtmService->manager, someProperties, &admin, &sendFunc, &handle) == CELIX_SUCCESS) {
				char* response = calloc(500, sizeof(*response));
				int replyStatus;
				printf("ECHO_SERVER: Wiring Proxy sucessfully retrieved\n");
				sendFunc(admin, handle, "CELIX-TEST", &response, &replyStatus);
			}
			else {
				printf("ECHO_SERVER: Retrieval of Wiring Proxy failed\n");
			}


//			celix_status_t (*getWiringProxy)(wiring_topology_manager_pt manager,properties_pt properties, wiring_admin_pt* admin, rsa_inaetics_send* sendFunc, wiring_handle* handle);

		} else {
			printf("ECHO_SERVER: WTM SERVICE is not availavble.\n");
		}
	}
	else {
		printf("ECHO_SERVER: Could not retrieve service.\n");
	}

		return status;
	}

	celix_status_t bundleActivator_stop(void * userData, bundle_context_pt context) {

		return CELIX_SUCCESS;
	}

	celix_status_t bundleActivator_destroy(void * userData, bundle_context_pt context) {

		return CELIX_SUCCESS;
	}
