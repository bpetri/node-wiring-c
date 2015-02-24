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
		void* service = NULL;
		wiring_topology_manager_service_pt wtmService = NULL;

		if (bundleContext_getService(command->bundleContext, wtmServiceRef, &service) == CELIX_SUCCESS) {
			wtmService = (wiring_topology_manager_service_pt) service;
		}

		// if the dereferenced instance is null then we know the service has been removed
		if (wtmService != NULL) {
			wiring_admin_pt admin = NULL;
			rsa_inaetics_send sendFunc = NULL;
			wiring_handle handle = NULL;
			char *token;

			strtok_r(line, " ", &token);

			char* wireId = strtok_r(NULL, " ", &token);
			char* msg = strtok_r(NULL, " ", &token);

			printf("ECHO_SERVER: Try to send \"%s\" to %s..\n", msg, wireId);

			if (wtmService->getWiringProxy(wtmService->manager, wireId, &admin, &sendFunc, &handle) == CELIX_SUCCESS) {
				char* response = calloc(500, sizeof(*response));
				int replyStatus;
				printf("ECHO_SERVER: Wiring Proxy successfully retrieved\n");

				// TODO: JSON WRAPPER

				sendFunc(admin, handle, msg, &response, &replyStatus);
				printf("ECHO_SERVER: Reply was: %s\n", response);

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
