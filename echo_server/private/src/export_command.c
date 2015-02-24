/**
 * Licensed under Apache License v2. See LICENSE for more information.
 */

#include <stdlib.h>
#include <ctype.h>

#include "array_list.h"
#include "bundle_context.h"
#include "command.h"
#include "command_impl.h"

#include "wiring_topology_manager.h"
#include "wiring_endpoint_description.h"

void exportCommand_execute(command_pt command, char *line, void (*out)(char *), void (*err)(char *));

celix_status_t echo_callback(char* data, char**response) {
	celix_status_t status = CELIX_SUCCESS;
	*response = calloc(500, sizeof(char));
	printf("ECHO_SERVER: data received: %s\n", data);
	snprintf(*response, 500, "echo %s echo", data);

	return status;
}

command_pt exportCommand_create(bundle_context_pt context) {
	command_pt command = (command_pt) calloc(1, sizeof(struct command));
	if (command) {
		command->bundleContext = context;
		command->name = "export";
		command->shortDescription = "triggers the export of an endpoint";
		command->usage = "export";
		command->executeCommand = exportCommand_execute;
	}
	return command;
}

void exportCommand_destroy(command_pt command) {
	free(command);
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

			properties_pt someProperties = properties_create();

			if (wtmService->installCallbackToWiringEndpoint(wtmService->manager, someProperties, echo_callback) == CELIX_SUCCESS) {
				printf("ECHO_SERVER: Callback successfully installed\n");
			} else {
				printf("ECHO_SERVER: Installation of Callback failed\n");
			}
		} else {
			printf("ECHO_SERVER: WTM SERVICE is not available.\n");
		}
	} else {
		printf("ECHO_SERVER: Could not retrieve service.\n");
	}
}

