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

void sendCommand_execute(command_pt command, char *line, void (*out)(char *), void (*err)(char *)) ;

celix_status_t echo_callback(char* data, char**response) {
	celix_status_t status = CELIX_SUCCESS;

	printf("ECHO_SERVER: echo_callback called.\n");
	printf("ECHO_SERVER: received data: %s\n", data);

	return status;
}

command_pt sendCommand_create(bundle_context_pt context) {
    command_pt command = (command_pt) calloc(1, sizeof(struct command));
    if (command) {
		command->bundleContext = context;
		command->name = "send";
		command->shortDescription = "gets the according wire and sends a message";
		command->usage = "send <wireId> <message>";
		command->executeCommand = sendCommand_execute;
    }
    return command;
}

void sendCommand_destroy(command_pt command) {
	free(command);
}

void sendCommand_execute(command_pt command, char *line, void (*out)(char *), void (*err)(char *)) {
	celix_status_t status = CELIX_SUCCESS;
    service_reference_pt wtmServiceRef = NULL;

	status = bundleContext_getServiceReference(command->bundleContext, (char *) INAETICS_WIRING_TOPOLOGY_MANAGER_SERVICE, &wtmServiceRef);

	if (status == CELIX_SUCCESS && wtmServiceRef != NULL) {
		wiring_topology_manager_service_pt wtmService = NULL;
		bundleContext_getService(command->bundleContext, wtmServiceRef, &wtmService);

		// if the dereferenced instance is null then we know the service has been removed
		if (wtmService != NULL) {
			properties_pt someProperties = properties_create();

			wiring_admin_pt admin = NULL;
			rsa_inaetics_send sendFunc = NULL;
			wiring_handle handle = NULL;
			char *token;

			strtok_r(line, " ", &token);

			char* wireId = strtok_r(NULL, " ", &token);
			char* msg = strtok_r(NULL, " ", &token);


			if (wtmService->installCallbackToWiringEndpoint(wtmService->manager, someProperties, echo_callback) == CELIX_SUCCESS) {
				printf("ECHO_SERVER: Callback sucessfully installed\n");
			} else {
				printf("ECHO_SERVER: Installation of Callback failed\n");
			}

			printf("ECHO_SERVER: Try to send \"%s\" to %s..\n", msg, wireId);

			properties_set(someProperties, WIRING_ENDPOINT_DESCRIPTION_UUID_KEY, wireId);

			if (wtmService->getWiringProxy(wtmService->manager, someProperties, &admin, &sendFunc, &handle) == CELIX_SUCCESS) {
				char* response = calloc(500, sizeof(*response));
				int replyStatus;
				printf("ECHO_SERVER: Wiring Proxy successfully retrieved\n");

				// TODO: JSON WRAPPER

				sendFunc(admin, handle, msg, &response, &replyStatus);
			} else {
				printf("ECHO_SERVER: Retrieval of Wiring Proxy failed\n");
			}
		} else {
			printf("ECHO_SERVER: WTM SERVICE is not available.\n");
		}
	} else {
		printf("ECHO_SERVER: Could not retrieve service.\n");
	}
}
