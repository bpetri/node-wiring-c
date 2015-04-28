/**
 * Licensed under Apache License v2. See LICENSE for more information.
 */

#include <stdlib.h>
#include <ctype.h>

#include "array_list.h"
#include "bundle_context.h"
#include "command.h"
#include "command_impl.h"
#include "export_command.h"

#include "constants.h"
#include "remote_constants.h"

#include "remote_service_admin.h"
#include "remote_service_admin_inaetics.h"

#include "wiring_endpoint_listener.h"
#include "wiring_topology_manager.h"
#include "wiring_endpoint_description.h"

void exportCommand_execute(command_pt command, char *line, void (*out)(char *), void (*err)(char *));
static celix_status_t exportCommand_registerReceive(command_pt command, char* wireId);
static celix_status_t exportCommand_unregisterReceive(command_pt command, char* wireId);
static celix_status_t exportCommand_addImportedWiringEndpoint(void *handle, wiring_endpoint_description_pt wEndpoint, char *matchedFilter);
static celix_status_t exportCommand_removeImportedWiringEndpoint(void *handle, wiring_endpoint_description_pt wEndpoint, char *matchedFilter);

celix_status_t echo_callback(void* handle, char* data, char**response) {
	celix_status_t status = CELIX_SUCCESS;
	*response = calloc(500, sizeof(char));
	printf("ECHO_SERVER: data received: %s\n", data);
	snprintf(*response, 500, "echo %s echo", data);

	return status;
}

celix_status_t exportCommand_create(bundle_context_pt context, command_pt* command) {
	celix_status_t status = CELIX_SUCCESS;
	export_command_pt exportCommand = (export_command_pt) calloc(1, sizeof(*exportCommand));
	wiring_endpoint_listener_pt wEndpointListener = (wiring_endpoint_listener_pt) calloc(1, sizeof(*wEndpointListener));

	*command = (command_pt) calloc(1, sizeof(**command));

	if (!(*command) || !wEndpointListener || !exportCommand) {
		if (*command) {
			free(*command);
		}
		if (wEndpointListener) {
			free(command);
		}
		if (exportCommand) {
			free(exportCommand);
		}
		status = CELIX_ENOMEM;
	} else {
		char *uuid = NULL;

		(*command)->bundleContext = context;
		(*command)->name = "export";
		(*command)->shortDescription = "triggers the export of an endpoint";
		(*command)->usage = "export";
		(*command)->executeCommand = exportCommand_execute;
		(*command)->handle = (void*) exportCommand;

		wEndpointListener->handle = (void*) (*command);
		wEndpointListener->wiringEndpointAdded = exportCommand_addImportedWiringEndpoint;
		wEndpointListener->wiringEndpointRemoved = exportCommand_removeImportedWiringEndpoint;

		exportCommand->wEndpointListener = wEndpointListener;
		exportCommand->wEndpointListenerRegistration = NULL;
		exportCommand->wiringReceiveService = NULL;
		exportCommand->wiringReceiveServiceRegistration = NULL;
		exportCommand->props = properties_create();

		/* as an example, we put some generic properties in here */
		properties_set(exportCommand->props, WIRING_ADMIN_PROPERTIES_SECURE_KEY, "no");

		status = bundleContext_getProperty(context, OSGI_FRAMEWORK_FRAMEWORK_UUID, &uuid);

		if (status == CELIX_SUCCESS) {
			properties_pt props = properties_create();
			size_t len = 11 + strlen(OSGI_FRAMEWORK_OBJECTCLASS) + strlen(OSGI_RSA_ENDPOINT_FRAMEWORK_UUID) + strlen(uuid);
			char scope[len + 1];

			sprintf(scope, "(%s=%s)", OSGI_RSA_ENDPOINT_FRAMEWORK_UUID, uuid);

			properties_set(props, (char *) INAETICS_WIRING_ENDPOINT_LISTENER_SCOPE, scope);

			status = bundleContext_registerService(context, (char *) INAETICS_WIRING_ENDPOINT_LISTENER_SERVICE, wEndpointListener, props, &exportCommand->wEndpointListenerRegistration);
		}
	}

	return status;
}

void exportCommand_destroy(command_pt command) {

	export_command_pt exportCommand = (export_command_pt) command->handle;

	if (exportCommand->wEndpointListener != NULL) {
		serviceRegistration_unregister(exportCommand->wEndpointListenerRegistration);
		free(exportCommand->wEndpointListener);
	}

	if (exportCommand->wiringReceiveServiceRegistration != NULL) {
		serviceRegistration_unregister(exportCommand->wiringReceiveServiceRegistration);
		free(exportCommand->wiringReceiveService);
	}

	properties_destroy(exportCommand->props);

	free(exportCommand);
	free(command);
}

/* Functions for wiring endpoint listener */
static celix_status_t exportCommand_addImportedWiringEndpoint(void *handle, wiring_endpoint_description_pt wEndpoint, char *matchedFilter) {
	celix_status_t status = CELIX_SUCCESS;

	wiring_endpoint_listener_pt listener = (wiring_endpoint_listener_pt) handle;
	command_pt command = (command_pt) listener->handle;
	export_command_pt exportCommand = (export_command_pt) command->handle;

	if (exportCommand->wiringReceiveServiceRegistration == NULL) {
		char* wireId = properties_get(wEndpoint->properties, WIRING_ENDPOINT_DESCRIPTION_WIRE_ID_KEY);

		printf("ECHO_SERVER: exportCommand_addImportedWiringEndpoint w/ wireId %s \n", wireId);

		status = exportCommand_registerReceive(command, wireId);

		if (status != CELIX_SUCCESS) {
			printf("ECHO_SERVER: Registration of Receive Service failed\n");
		}
	} else {
		printf(" ECHO_SERVER: exportCommand_addImportedWiringEndpoint already imported.\n");
	}

	return status;
}

static celix_status_t exportCommand_removeImportedWiringEndpoint(void *handle, wiring_endpoint_description_pt wEndpoint, char *matchedFilter) {
	celix_status_t status = CELIX_SUCCESS;

	wiring_endpoint_listener_pt listener = (wiring_endpoint_listener_pt) handle;
	command_pt command = (command_pt) listener->handle;
	char* wireId = properties_get(wEndpoint->properties, WIRING_ENDPOINT_DESCRIPTION_WIRE_ID_KEY);

	status = exportCommand_unregisterReceive(command, wireId);

	return status;
}

static celix_status_t exportCommand_registerReceive(command_pt command, char* wireId) {
	celix_status_t status = CELIX_SUCCESS;

	printf("registerReceive w/ wireId %s\n", wireId);

	wiring_receive_service_pt wiringReceiveService = calloc(1, sizeof(*wiringReceiveService));

	if (!wiringReceiveService) {
		status = CELIX_ENOMEM;
	} else {
		export_command_pt exportCommand = (export_command_pt) command->handle;
		properties_pt props = properties_create();

		properties_set(props, (char*) INAETICS_WIRING_WIRE_ID, wireId);

		wiringReceiveService->handle = command;
		wiringReceiveService->receive = echo_callback;
		wiringReceiveService->wireId = wireId;

		status = bundleContext_registerService(command->bundleContext, (char *) INAETICS_WIRING_RECEIVE_SERVICE, wiringReceiveService, props, &exportCommand->wiringReceiveServiceRegistration);

		if (status == CELIX_SUCCESS) {
			exportCommand->wiringReceiveService = wiringReceiveService;
		} else {
			free(wiringReceiveService);
		}
	}

	return status;
}

static celix_status_t exportCommand_unregisterReceive(command_pt command, char* wireId) {
	celix_status_t status = CELIX_SUCCESS;

	printf("unregisterReceive w/ wireId %s\n", wireId);

	export_command_pt exportCommand = (export_command_pt) command->handle;

	if (exportCommand->wiringReceiveServiceRegistration != NULL) {
		char* serviceWireId = NULL;
		array_list_pt references = NULL;
		int i;

		serviceRegistration_getServiceReferences(exportCommand->wiringReceiveServiceRegistration, &references);

		for (i = 0; i < arrayList_size(references); i++) {
			service_reference_pt reference = (service_reference_pt) arrayList_get(references, i);
			serviceReference_getProperty(reference, (char*) INAETICS_WIRING_WIRE_ID, &serviceWireId);

			if (strcmp(wireId, serviceWireId) == 0) {
				status = serviceRegistration_unregister(exportCommand->wiringReceiveServiceRegistration);

				if (status == CELIX_SUCCESS) {
					free(exportCommand->wiringReceiveService);
				}
			}
		}
	}

	return status;
}

void exportCommand_execute(command_pt command, char *line, void (*out)(char *), void (*err)(char *)) {
	celix_status_t status = CELIX_SUCCESS;
	service_reference_pt wtmServiceRef = NULL;

	status = bundleContext_getServiceReference(command->bundleContext, (char *) INAETICS_WIRING_TOPOLOGY_MANAGER_SERVICE, &wtmServiceRef);

	if (status == CELIX_SUCCESS && wtmServiceRef != NULL) {
		void* service = NULL;
		wiring_topology_manager_service_pt wtmService = NULL;

		if (bundleContext_getService(command->bundleContext, wtmServiceRef, &service) == CELIX_SUCCESS) {
			wtmService = (wiring_topology_manager_service_pt) service;
		}

		// if the dereferenced instance is null then we know the service has been removed
		if (wtmService != NULL) {
			export_command_pt exportCommand = (export_command_pt) command->handle;
			char* wireId = NULL;

			if (wtmService->exportWiringEndpoint(wtmService->manager, exportCommand->props, &wireId) != CELIX_SUCCESS) {
				printf("ECHO_SERVER: Installation of Callback failed\n");
			} else {
				printf("ECHO_SERVER: Receive callback successfully installed\n");
			}

			if (wireId != NULL) {
				free(wireId);
			}

		} else {
			printf("ECHO_SERVER: WTM SERVICE is not available.\n");
		}
	} else {
		printf("ECHO_SERVER: Could not retrieve service.\n");
	}
}

