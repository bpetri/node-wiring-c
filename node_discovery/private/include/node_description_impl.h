
#ifndef NODE_DESCRIPTION_IMPL_H_
#define NODE_DESCRIPTION_IMPL_H_

#include "properties.h"
#include "node_description.h"

struct node_description {
    char *frameworkUUID;
    properties_pt properties;
};

#endif
