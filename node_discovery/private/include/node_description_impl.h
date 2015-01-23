
#ifndef NODE_DESCRIPTION_IMPL_H_
#define NODE_DESCRIPTION_IMPL_H_

#include "properties.h"
#include "array_list.h"
#include "node_description.h"

struct node_description {
    char *nodeId;
    array_list_pt wiring_ep_descriptions_list;
    properties_pt properties;
};

void dump_node_description(node_description_pt node_desc);

#endif
