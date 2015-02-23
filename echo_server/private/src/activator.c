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

struct echoActivator {
	service_registration_pt sendCommand;
	command_pt sendCmd;
	command_service_pt sendCmdSrv;
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

celix_status_t bundleActivator_start(void * userData, bundle_context_pt context) {
	celix_status_t status = CELIX_SUCCESS;

	struct echoActivator * activator = (struct echoActivator *) userData;

	activator->sendCmd = sendCommand_create(context);
	echoServer_createCommandService(activator->sendCmd, &activator->sendCmdSrv);
	bundleContext_registerService(context, (char *) OSGI_SHELL_COMMAND_SERVICE_NAME, activator->sendCmdSrv, NULL, &activator->sendCommand);

	return status;
}

celix_status_t bundleActivator_stop(void * userData, bundle_context_pt context) {

	return CELIX_SUCCESS;
}

celix_status_t bundleActivator_destroy(void * userData, bundle_context_pt context) {

	return CELIX_SUCCESS;
}
