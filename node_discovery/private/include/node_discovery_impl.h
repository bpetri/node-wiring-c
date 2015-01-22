
#ifndef NODE_DISCOVERY_IMPL_H_
#define NODE_DISCOVERY_IMPL_H_

#include "bundle_context.h"
#include "service_reference.h"
#include "node_description.h"

#include "etcd_watcher.h"


#define NODE_DISCOVERY_DEFAULT_ZONE_IDENTIFIER	"inaetics-testing"


struct node_discovery {
	bundle_context_pt context;
	node_description_pt ownNode;
	celix_thread_mutex_t discoveredNodesMutex;
	hash_map_pt discoveredNodes; //key=endpointId (string), value=endpoint_description_pt

	etcd_watcher_pt watcher;
};

celix_status_t node_discovery_create(bundle_context_pt context, node_discovery_pt* node_discovery);
celix_status_t node_discovery_destroy(node_discovery_pt node_discovery);
celix_status_t node_discovery_start(node_discovery_pt node_discovery);
celix_status_t node_discovery_stop(node_discovery_pt node_discovery);


celix_status_t node_discovery_createNodeDescription(node_discovery_pt node_discovery, node_description_pt* node_description);
celix_status_t node_discovery_destroyNodeDescription(node_description_pt* node_description);

celix_status_t node_discovery_addNode(node_discovery_pt node_discovery, char* key, char* value);
celix_status_t node_discovery_removeNode(node_discovery_pt node_discovery, char* key);

#endif /* DISCOVERY_H_ */
