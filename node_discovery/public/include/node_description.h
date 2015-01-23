
#ifndef NODE_DESCRIPTION_H_
#define NODE_DESCRIPTION_H_

#define NODE_DESCRIPTION_ZONE_IDENTIFIER_KEY	"inaetics.zone.identifier"
#define NODE_DESCRIPTION_NODE_IDENTIFIER_KEY	"inaetics.node.identifier"

typedef struct node_description *node_description_pt;

celix_status_t nodeDescription_create(char* uuid,properties_pt properties,node_description_pt *nodeDescription);
celix_status_t nodeDescription_destroy(node_description_pt nodeDescription);

#endif
