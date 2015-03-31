/**
 * Licensed under Apache License v2. See LICENSE for more information.
 */

#include <stdlib.h>
#include <ctype.h>

#include "array_list.h"
#include "bundle_context.h"
#include "service_tracker.h"
#include "service_tracker_customizer.h"
#include "command.h"
#include "command_impl.h"
#include "send_command.h"
#include "utils.h"
#include "constants.h"

#include "wiring_topology_manager.h"
#include "wiring_endpoint_description.h"

void sendCommand_execute(command_pt command, char *line, void (*out)(char *), void (*err)(char *));

static celix_status_t sendCommand_sendServiceAdding(void * handle, service_reference_pt reference, void **service);
static celix_status_t sendCommand_sendServiceAdded(void * handle, service_reference_pt reference, void * service);
static celix_status_t sendCommand_sendServiceModified(void * handle, service_reference_pt reference, void * service);
static celix_status_t sendCommand_sendServiceRemoved(void * handle, service_reference_pt reference, void * service);

hash_map_pt sendServices;

celix_status_t sendCommand_create(bundle_context_pt context, command_pt* command) {
	celix_status_t status = CELIX_SUCCESS;
	send_command_pt sendCommand = (send_command_pt) calloc(1, sizeof(*sendCommand));

	*command = (command_pt) calloc(1, sizeof(**command));

	if (!(*command) || !sendCommand) {
		if (*command) {
			free(*command);
		}
		if (sendCommand) {
			free(sendCommand);
		}
		status = CELIX_ENOMEM;
	} else {
		(*command)->bundleContext = context;
		(*command)->name = "send";
		(*command)->shortDescription = "gets the according wire and sends a message";
		(*command)->usage = "send <wireId> <message>";
		(*command)->executeCommand = sendCommand_execute;
		(*command)->handle = (void*) sendCommand;

		sendCommand->sendServices = hashMap_create(utils_stringHash, NULL, utils_stringEquals, NULL);

	}

	return status;
}

void sendCommand_destroy(command_pt command) {
	send_command_pt sendCommand = (send_command_pt) command->handle;

	hashMap_destroy(sendCommand->sendServices, false, false);

	free(command->handle);
	free(command);
}

celix_status_t sendCommand_createSendServiceTracker(command_pt command, char* wireId) {
	celix_status_t status = CELIX_SUCCESS;
	service_tracker_customizer_pt customizer = NULL;

	status = serviceTrackerCustomizer_create(command, sendCommand_sendServiceAdding, sendCommand_sendServiceAdded, sendCommand_sendServiceModified, sendCommand_sendServiceRemoved, &customizer);

	if (status == CELIX_SUCCESS) {
		send_command_pt sendCommand = (send_command_pt) command->handle;
		char filter[512];

		snprintf(filter, 512, "(&(%s=%s)(%s=%s))", (char*) OSGI_FRAMEWORK_OBJECTCLASS, (char*) INAETICS_WIRING_SEND_SERVICE, (char*) INAETICS_WIRING_WIRE_ID, wireId);

		status = serviceTracker_createWithFilter(command->bundleContext, filter, customizer, &sendCommand->sendServicesTracker);

		if (status == CELIX_SUCCESS) {
			printf("ECHO_SERVER: serviceTracker created w/ %s\n", filter);

			serviceTracker_open(sendCommand->sendServicesTracker);
		} else {
			printf("ECHO_SERVER: serviceTracker could not be created w/ %s\n", filter);

		}

	}

	return status;
}

celix_status_t sendCommand_destroySendServiceTracker(command_pt command) {
	celix_status_t status = CELIX_SUCCESS;
	send_command_pt sendCommand = (send_command_pt) command->handle;

	status = serviceTracker_close(sendCommand->sendServicesTracker);
	if (status == CELIX_SUCCESS) {
		status = serviceTracker_destroy(sendCommand->sendServicesTracker);
	}

	return status;
}

static celix_status_t sendCommand_sendServiceAdding(void * handle, service_reference_pt reference, void **service) {
	celix_status_t status = CELIX_SUCCESS;

	command_pt command = (command_pt) handle;

	status = bundleContext_getService(command->bundleContext, reference, service);

	return status;
}

static celix_status_t sendCommand_sendServiceAdded(void * handle, service_reference_pt reference, void * service) {
	celix_status_t status = CELIX_SUCCESS;

	command_pt command = (command_pt) handle;
	send_command_pt sendCommand = (send_command_pt) command->handle;

	printf("ECHO_SERVER: Send Service Added");

	wiring_send_service_pt wiringSendService = (wiring_send_service_pt) service;
	hashMap_put(sendCommand->sendServices, wiringSendService->wiringEndpointDescription->wireId, wiringSendService);

	return status;
}

static celix_status_t sendCommand_sendServiceModified(void * handle, service_reference_pt reference, void * service) {
	celix_status_t status = CELIX_SUCCESS;

	return status;
}

static celix_status_t sendCommand_sendServiceRemoved(void * handle, service_reference_pt reference, void * service) {
	celix_status_t status = CELIX_SUCCESS;
	command_pt command = (command_pt) handle;
	send_command_pt sendCommand = (send_command_pt) command->handle;

	wiring_send_service_pt wiringSendService = (wiring_send_service_pt) service;
	hashMap_remove(sendCommand->sendServices, wiringSendService->wiringEndpointDescription->wireId);

	return status;
}

void sendCommand_execute(command_pt command, char *line, void (*out)(char *), void (*err)(char *)) {
	celix_status_t status = CELIX_SUCCESS;
	service_reference_pt wtmServiceRef = NULL;

	status = bundleContext_getServiceReference(command->bundleContext, (char *) INAETICS_WIRING_TOPOLOGY_MANAGER_SERVICE, &wtmServiceRef);

	if (status == CELIX_SUCCESS) {
		void* service = NULL;
		wiring_topology_manager_service_pt wtmService = NULL;

		if (bundleContext_getService(command->bundleContext, wtmServiceRef, &service) == CELIX_SUCCESS) {
			wtmService = (wiring_topology_manager_service_pt) service;
		}

		// if the dereferenced instance is null then we know the service has been removed
		if (wtmService != NULL) {
			char wireId[WIREID_LENGTH];
			char msg[MSG_LENGTH];

			if (sscanf(line, "send %s %s", &wireId[0], &msg[0]) == 2) {
				printf("ECHO_SERVER: Try to send \"%s\" to %s.\n", msg, wireId);

				status = sendCommand_createSendServiceTracker(command, wireId);

				if (status == CELIX_SUCCESS) {
					properties_pt rsaProperties = properties_create();

					properties_set(rsaProperties, "inaetics.wiring.id", wireId);

					if (wtmService->importWiringEndpoint(wtmService->manager, rsaProperties) == CELIX_SUCCESS) {
						send_command_pt sendCommand = (send_command_pt) command->handle;
						wiring_send_service_pt wiringSendService = NULL;

						printf("ECHO_SERVER: importWiringEndpoint successfully executed.\n");

						wiringSendService = hashMap_get(sendCommand->sendServices, wireId);

						if (wiringSendService == NULL) {
							printf("ECHO_SERVER: No matching SendService found.\n");
						} else {

							char* reply = NULL;
							int replyStatus = 0;

							status = wiringSendService->send(wiringSendService, msg, &reply, &replyStatus);
							printf("ECHO_SERVER: %s sent\n", msg);
							printf("ECHO_SERVER: %s received\n", reply);
						}

						if (wtmService->removeImportedWiringEndpoint(wtmService->manager, rsaProperties) == CELIX_SUCCESS) {
							printf("ECHO_SERVER: removeImportWiringEndpoint successfully executed.\n");
						} else {
							printf("ECHO_SERVER: removeImportWiringEndpoint failed.\n");
						}

					} else {
						printf("ECHO_SERVER: importWiringEndpoint failed.\n");
					}

					properties_destroy(rsaProperties);
					sendCommand_destroySendServiceTracker(command);
				} else {
					printf("ECHO_SERVER: Creation of SendServiceTracker (wireId %s) failed.\n", wireId);
				}
			} else {
				printf("ECHO_SERVER: Usage: %s\n", command->usage);
			}
		} else {
			printf("ECHO_SERVER: Could not get service.\n");
		}
	} else {
		printf("ECHO_SERVER: Could not get service reference.\n");
	}
}
