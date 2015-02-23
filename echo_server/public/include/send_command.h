/**
 * Licensed under Apache License v2. See LICENSE for more information.
 */

#ifndef SEND_COMMAND_H_
#define SEND_COMMAND_H_

#include "command_impl.h"

command_pt sendCommand_create(bundle_context_pt context);
void sendCommand_destroy(command_pt command);

#endif /* SEND_COMMAND_H_ */
