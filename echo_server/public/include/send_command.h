/**
 * Licensed under Apache License v2. See LICENSE for more information.
 */

#ifndef SEND_COMMAND_H_
#define SEND_COMMAND_H_

#include "command_impl.h"
#include "service_tracker.h"
#include "hash_map.h"

#define WIREID_LENGTH		32
#define MSG_LENGTH			255

struct send_command {
	service_tracker_pt sendServicesTracker;
	hash_map_pt sendServices;
};

typedef struct send_command *send_command_pt;

celix_status_t sendCommand_create(bundle_context_pt context, command_pt* command);
void sendCommand_destroy(command_pt command);

#endif /* SEND_COMMAND_H_ */
