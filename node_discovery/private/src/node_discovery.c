
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <netdb.h>
#include <netinet/in.h>

#include "constants.h"
#include "celix_threads.h"
#include "bundle_context.h"
#include "array_list.h"
#include "utils.h"
#include "celix_errno.h"
#include "filter.h"
#include "service_reference.h"
#include "service_registration.h"


#include "etcd_watcher.h"
#include "node_description_impl.h"
#include "node_discovery_impl.h"


celix_status_t node_discovery_create(bundle_context_pt context, node_discovery_pt *node_discovery) {
	celix_status_t status = CELIX_SUCCESS;

	*node_discovery = calloc(1, sizeof(**node_discovery));

	if (!*node_discovery) {
		status = CELIX_ENOMEM;
	}
	else {
		(*node_discovery)->context = context;
		(*node_discovery)->discoveredNodes = hashMap_create(utils_stringHash, NULL, utils_stringEquals, NULL);

	    node_discovery_createNodeDescription((*node_discovery), &(*node_discovery)->ownNode);
		status = celixThreadMutex_create(&(*node_discovery)->discoveredNodesMutex, NULL);
	}

	return status;
}



celix_status_t node_discovery_destroy(node_discovery_pt node_discovery) {
	celix_status_t status = CELIX_SUCCESS;

	celixThreadMutex_lock(&node_discovery->discoveredNodesMutex);
	hashMap_destroy(node_discovery->discoveredNodes, false, false);
	node_discovery->discoveredNodes = NULL;
	celixThreadMutex_unlock(&node_discovery->discoveredNodesMutex);
	celixThreadMutex_destroy(&node_discovery->discoveredNodesMutex);

	node_discovery_destroyNodeDescription(&node_discovery->ownNode);
	free(node_discovery);

	return status;
}

celix_status_t node_discovery_start(node_discovery_pt node_discovery) {
    celix_status_t status = CELIX_SUCCESS;

    status = etcdWatcher_create(node_discovery, node_discovery->context, &node_discovery->watcher);

    if (status != CELIX_SUCCESS) {
    	status = CELIX_BUNDLE_EXCEPTION;
    }

    return status;
}

celix_status_t node_discovery_stop(node_discovery_pt node_discovery) {
	celix_status_t status;

	status = etcdWatcher_destroy(node_discovery->watcher);

	if (status != CELIX_SUCCESS) {
		status = CELIX_BUNDLE_EXCEPTION;
	}

	return status;
}


celix_status_t node_discovery_createNodeDescription(node_discovery_pt node_discovery, node_description_pt* node_description) {
	celix_status_t status = CELIX_SUCCESS;

	char* fwuuid = NULL;
	char* inZoneIdentifier = NULL;
	char* inNodeIdentifier = NULL;

	if (((bundleContext_getProperty(node_discovery->context, OSGI_FRAMEWORK_FRAMEWORK_UUID, &fwuuid)) != CELIX_SUCCESS) || (!fwuuid)) {
		status = CELIX_ILLEGAL_STATE;
	}

	if (((bundleContext_getProperty(node_discovery->context, NODE_DISCOVERY_ZONE_IDENTIFIER, &inZoneIdentifier)) != CELIX_SUCCESS) || (!inZoneIdentifier)) {
		inZoneIdentifier = (char*) NODE_DISCOVERY_DEFAULT_ZONE_IDENTIFIER;
	}

	if (((bundleContext_getProperty(node_discovery->context, NODE_DISCOVERY_NODE_IDENTIFIER, &inNodeIdentifier)) != CELIX_SUCCESS) || (!inNodeIdentifier)) {
		inNodeIdentifier = fwuuid;
	}

	if (status == CELIX_SUCCESS) {
		(*node_description) = calloc(1, sizeof(**node_description));

		if (!*node_description) {
			status = CELIX_ENOMEM;
		}
		else {
			(*node_description)->frameworkUUID = fwuuid;
			(*node_description)->properties = properties_create();

			properties_set((*node_description)->properties, NODE_DESCRIPTION_NODE_IDENTIFIER_KEY, inNodeIdentifier);
			properties_set((*node_description)->properties, NODE_DESCRIPTION_ZONE_IDENTIFIER_KEY, inZoneIdentifier);
		}
	}

	return status;
}

celix_status_t node_discovery_destroyNodeDescription(node_description_pt* node_description) {
	celix_status_t status = CELIX_SUCCESS;

	properties_destroy((*node_description)->properties);
	free(*node_description);

	return status;
}


celix_status_t node_discovery_addNode(node_discovery_pt node_discovery, char* key, char* value) {
	celix_status_t status = CELIX_SUCCESS;

	celixThreadMutex_lock(&node_discovery->discoveredNodesMutex);

	// TODO conv to properties
	hashMap_put(node_discovery->discoveredNodes, key, value);
	celixThreadMutex_unlock(&node_discovery->discoveredNodesMutex);

	printf("\nNode <%s,%s> added\n", key,value);

	return status;
}



celix_status_t node_discovery_removeNode(node_discovery_pt node_discovery, char* key) {
	celix_status_t status = CELIX_SUCCESS;

	celixThreadMutex_lock(&node_discovery->discoveredNodesMutex);
	hashMap_remove(node_discovery->discoveredNodes, key);
	celixThreadMutex_unlock(&node_discovery->discoveredNodesMutex);

	printf("\nNode %s removed\n", key);

	return status;
}

