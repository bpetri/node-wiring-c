
#ifndef NODE_DESCRIPTION_IMPL_H_
#define NODE_DESCRIPTION_IMPL_H_

#include "properties.h"

#define NODE_DESCRIPTION_ZONE_IDENTIFIER_KEY	"inaetics.zone.identifier"
#define NODE_DESCRIPTION_NODE_IDENTIFIER_KEY	"inaetics.node.identifier"

struct node_description {
    char *frameworkUUID;
    properties_pt properties;
};

typedef struct node_description *node_description_pt;

#endif
