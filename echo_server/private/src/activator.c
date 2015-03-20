/**
 * Licensed under Apache License v2. See LICENSE for more information.
 */

#include <stdlib.h>

#include <celix_errno.h>

#include "bundle_activator.h"
#include "bundle_context.h"
#include "service_tracker.h"
#include "properties.h"

#include "command_impl.h"
#include "send_command.h"
#include "export_command.h"

struct echoActivator {
	service_registration_pt sendCommand;
	command_pt sendCmd;
	command_service_pt sendCmdSrv;

	service_registration_pt exportCommand;
	command_pt exportCmd;
	command_service_pt exportCmdSrv;
};

celix_status_t bundleActivator_create(bundle_context_pt context, void **userData) {

	celix_status_t status = CELIX_SUCCESS;

	*userData = calloc(1, sizeof(struct echoActivator));

	if (!*userData) {
		status = CELIX_ENOMEM;
	} else {
		((struct echoActivator *) (*userData))->sendCommand = NULL;
		((struct echoActivator *) (*userData))->sendCmd = NULL;
		((struct echoActivator *) (*userData))->sendCmdSrv = NULL;

		((struct echoActivator *) (*userData))->exportCommand = NULL;
		((struct echoActivator *) (*userData))->exportCmd = NULL;
		((struct echoActivator *) (*userData))->exportCmdSrv = NULL;
	}

	return status;
}

static celix_status_t echoServer_createCommandService(command_pt command, command_service_pt *commandService) {
	*commandService = calloc(1, sizeof(**commandService));
	(*commandService)->command = command;
	(*commandService)->executeCommand = command->executeCommand;
	(*commandService)->getName = command_getName;
	(*commandService)->getShortDescription = command_getShortDescription;
	(*commandService)->getUsage = command_getUsage;

	return CELIX_SUCCESS;
}

static celix_status_t echoServer_destroyCommandService(command_service_pt *commandService) {
	free(*commandService);
	*commandService = NULL;

	return CELIX_SUCCESS;
}

celix_status_t bundleActivator_start(void * userData, bundle_context_pt context) {
	celix_status_t status = CELIX_SUCCESS;

	struct echoActivator * activator = (struct echoActivator *) userData;

	if (sendCommand_create(context, &activator->sendCmd) == CELIX_SUCCESS) {
		echoServer_createCommandService(activator->sendCmd, &activator->sendCmdSrv);
		bundleContext_registerService(context, (char *) OSGI_SHELL_COMMAND_SERVICE_NAME, activator->sendCmdSrv, NULL, &activator->sendCommand);
	} else {
		printf("ECHO_SERVER: creation of sendCommand failed.\n");
	}

	if (exportCommand_create(context, &activator->exportCmd) == CELIX_SUCCESS) {
		echoServer_createCommandService(activator->exportCmd, &activator->exportCmdSrv);
		bundleContext_registerService(context, (char *) OSGI_SHELL_COMMAND_SERVICE_NAME, activator->exportCmdSrv, NULL, &activator->exportCommand);
	} else {
		printf("ECHO_SERVER: creation of exportCommand failed.\n");
	}

	return status;
}

celix_status_t bundleActivator_stop(void * userData, bundle_context_pt context) {

	struct echoActivator * activator = (struct echoActivator *) userData;

	if (activator->sendCmdSrv != NULL) {
		echoServer_destroyCommandService(&activator->sendCmdSrv);
	}

	if (activator->sendCmd != NULL) {
		sendCommand_destroy(activator->sendCmd);
	}

	if (activator->exportCmdSrv != NULL) {
		echoServer_destroyCommandService(&activator->exportCmdSrv);
	}

	if (activator->exportCmd != NULL) {
		exportCommand_destroy(activator->exportCmd);
	}

	return CELIX_SUCCESS;
}

celix_status_t bundleActivator_destroy(void * userData, bundle_context_pt context) {

	free(userData);

	return CELIX_SUCCESS;
}
