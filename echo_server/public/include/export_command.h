/**
 * Licensed under Apache License v2. See LICENSE for more information.
 */

#ifndef EXPORT_COMMAND_H_
#define EXPORT_COMMAND_H_

#include "command_impl.h"

command_pt exportCommand_create(bundle_context_pt context);
void exportCommand_destroy(command_pt command);

#endif /* EXPORT_COMMAND_H_ */
