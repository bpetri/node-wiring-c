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

#include "wiring_endpoint_listener.h"

struct activator {
	remote_service_admin_pt admin;
	remote_service_admin_service_pt adminService;
	service_registration_pt registration;

	wiring_endpoint_listener_pt wEndpointListener;
	service_registration_pt wEndpointListenerRegistration;

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
			wEndpointListener->handle = (void*) activator->admin;
			wEndpointListener->wiringEndpointAdded = remoteServiceAdmin_addImportedWiringEndpoint;
			wEndpointListener->wiringEndpointRemoved = remoteServiceAdmin_removeImportedWiringEndpoint;

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

	return status;
}

celix_status_t bundleActivator_stop(void * userData, bundle_context_pt context) {
	celix_status_t status = CELIX_SUCCESS;
	struct activator *activator = userData;

	remoteServiceAdmin_stop(activator->admin);
	serviceRegistration_unregister(activator->registration);
	activator->registration = NULL;

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

