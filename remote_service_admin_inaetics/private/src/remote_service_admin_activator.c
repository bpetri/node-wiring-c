/**
 * Licensed under Apache License v2. See LICENSE for more information.
 */

#include <stdlib.h>

#include "bundle_activator.h"
#include "constants.h"
#include "service_registration.h"

#include "remote_service_admin_inaetics_impl.h"
#include "export_registration_impl.h"
#include "import_registration_impl.h"

#include "wiring_topology_manager.h"
#include "wiring_endpoint_listener.h"

static celix_status_t bundleActivator_createWTMTracker(struct activator *activator, service_tracker_pt *tracker);


struct activator {
	remote_service_admin_pt admin;
	remote_service_admin_service_pt adminService;
	service_registration_pt registration;

	wiring_endpoint_listener_pt wEndpointListener;
	service_registration_pt wEndpointListenerRegistration;

    service_tracker_pt wtmTracker;
};

celix_status_t bundleActivator_create(bundle_context_pt context, void **userData) {
	celix_status_t status = CELIX_SUCCESS;
	struct activator *activator;

	activator = calloc(1, sizeof(*activator));
	if (!activator) {
		status = CELIX_ENOMEM;
	} else {
		activator->admin = NULL;
		activator->registration = NULL;
	    activator->wtmTracker = NULL;

		*userData = activator;
	}

	return status;
}

celix_status_t bundleActivator_start(void * userData, bundle_context_pt context) {
	celix_status_t status = CELIX_SUCCESS;
	struct activator *activator = userData;
	remote_service_admin_service_pt remoteServiceAdminService = NULL;

	status = remoteServiceAdmin_create(context, &activator->admin);
	if (status == CELIX_SUCCESS) {
		remoteServiceAdminService = calloc(1, sizeof(*remoteServiceAdminService));

		wiring_endpoint_listener_pt wEndpointListener = (wiring_endpoint_listener_pt) calloc(1, sizeof(*wEndpointListener));

		if (!remoteServiceAdminService || !wEndpointListener) {

			if (wEndpointListener) {
				free(wEndpointListener);
			}
			if (remoteServiceAdminService) {
				free(remoteServiceAdminService);
			}

			status = CELIX_ENOMEM;
		} else {
           status = bundleActivator_createWTMTracker(activator, &activator->wtmTracker);

            if (status != CELIX_SUCCESS) {
                printf("RSA: Creation of WTMTracker failed\n");
            }
            else {

                serviceTracker_open(activator->wtmTracker);

                wEndpointListener->handle = (void*) activator->admin;
                wEndpointListener->wiringEndpointAdded = remoteServiceAdmin_addWiringEndpoint;
                wEndpointListener->wiringEndpointRemoved = remoteServiceAdmin_removeWiringEndpoint;

                activator->wEndpointListener = wEndpointListener;
                activator->wEndpointListenerRegistration = NULL;

                remoteServiceAdminService->admin = activator->admin;

                remoteServiceAdminService->exportService = remoteServiceAdmin_exportService;
                remoteServiceAdminService->getExportedServices = remoteServiceAdmin_getExportedServices;
                remoteServiceAdminService->getImportedEndpoints = remoteServiceAdmin_getImportedEndpoints;
                remoteServiceAdminService->importService = remoteServiceAdmin_importService;

                remoteServiceAdminService->exportReference_getExportedEndpoint = exportReference_getExportedEndpoint;
                remoteServiceAdminService->exportReference_getExportedService = exportReference_getExportedService;

                remoteServiceAdminService->exportRegistration_close = exportRegistration_close;
                remoteServiceAdminService->exportRegistration_getException = exportRegistration_getException;
                remoteServiceAdminService->exportRegistration_getExportReference = exportRegistration_getExportReference;

                remoteServiceAdminService->importReference_getImportedEndpoint = importReference_getImportedEndpoint;
                remoteServiceAdminService->importReference_getImportedService = importReference_getImportedService;

                remoteServiceAdminService->importRegistration_close = remoteServiceAdmin_removeImportedService;
                remoteServiceAdminService->importRegistration_getException = importRegistration_getException;
                remoteServiceAdminService->importRegistration_getImportReference = importRegistration_getImportReference;
                char *uuid = NULL;

                properties_pt props = properties_create();
                bundleContext_getProperty(context, OSGI_FRAMEWORK_FRAMEWORK_UUID, &uuid);
                size_t len = 11 + strlen(OSGI_FRAMEWORK_OBJECTCLASS) + strlen(OSGI_RSA_ENDPOINT_FRAMEWORK_UUID) + strlen(uuid);


                // we do not need a scope cause we want to be informed about all wiringEndpointsAdded (imported and exported)
                char scope[len + 1];
                sprintf(scope, "(%s=%s)", OSGI_RSA_ENDPOINT_FRAMEWORK_UUID, uuid);
                properties_set(props, (char *) INAETICS_WIRING_ENDPOINT_LISTENER_SCOPE, scope);

                status = bundleContext_registerService(context, (char *) INAETICS_WIRING_ENDPOINT_LISTENER_SERVICE, wEndpointListener, props, &activator->wEndpointListenerRegistration);

                if (status == CELIX_SUCCESS) {
                    status = bundleContext_registerService(context, OSGI_RSA_REMOTE_SERVICE_ADMIN, remoteServiceAdminService, NULL, &activator->registration);
                    printf("RSA: service registration succeeded\n");
                } else {
                    properties_destroy(props);
                    printf("RSA: service registration failed\n");
                }

                activator->adminService = remoteServiceAdminService;
            }
		}
	}

	return status;
}

celix_status_t bundleActivator_stop(void * userData, bundle_context_pt context) {
	celix_status_t status = CELIX_SUCCESS;
	struct activator *activator = userData;


	if (activator->registration != NULL) {
        serviceRegistration_unregister(activator->registration);
        activator->registration = NULL;
    }

    remoteServiceAdmin_stop(activator->admin);

	remoteServiceAdmin_destroy(&activator->admin);
	free(activator->adminService);

	serviceRegistration_unregister(activator->wEndpointListenerRegistration);
	free(activator->wEndpointListener);

	return status;
}

celix_status_t bundleActivator_destroy(void * userData, bundle_context_pt context) {
	celix_status_t status = CELIX_SUCCESS;
	struct activator *activator = userData;

	free(activator);

	return status;
}


static celix_status_t bundleActivator_createWTMTracker(struct activator *activator, service_tracker_pt *tracker) {
    celix_status_t status = CELIX_SUCCESS;

    service_tracker_customizer_pt customizer = NULL;

    status = serviceTrackerCustomizer_create(activator->admin, remoteServiceAdmin_wtmAdding,
            remoteServiceAdmin_wtmAdded, remoteServiceAdmin_wtmModified, remoteServiceAdmin_wtmRemoved, &customizer);

    if (status == CELIX_SUCCESS) {
        status = serviceTracker_create(activator->admin->context, (char*) INAETICS_WIRING_TOPOLOGY_MANAGER_SERVICE, customizer, tracker);
    }

    return status;
}

