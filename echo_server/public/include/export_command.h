/**
 * Licensed under Apache License v2. See LICENSE for more information.
 */

#ifndef EXPORT_COMMAND_H_
#define EXPORT_COMMAND_H_

#include "celix_errno.h"
#include "wiring_endpoint_listener.h"
#include "service_registration.h"
#include "command_impl.h"

#include "remote_service_admin.h"
#include "remote_service_admin_inaetics.h"


struct export_command {
	wiring_endpoint_listener_pt wEndpointListener;
	service_registration_pt wEndpointListenerRegistration;

	char* wireId;
	properties_pt props;
	wiring_receive_service_pt wiringReceiveService;
	service_registration_pt wiringReceiveServiceRegistration;
};

typedef struct export_command* export_command_pt;


celix_status_t exportCommand_create(bundle_context_pt context, command_pt* command);
void exportCommand_destroy(command_pt command);

#endif /* EXPORT_COMMAND_H_ */
