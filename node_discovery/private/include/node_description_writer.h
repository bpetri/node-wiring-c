
#ifndef NODE_DESCRIPTION_WRITER_H_
#define NODE_DESCRIPTION_WRITER_H_

#include "celix_errno.h"
#include "node_description.h"

celix_status_t node_description_writer_nodeDescToString(node_description_pt inNodeDesc, char** outStr);

#endif
