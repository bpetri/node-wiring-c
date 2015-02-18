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
#include "remote_constants.h"

#include "etcd_watcher.h"
#include "wiring_admin.h"
#include "node_description_impl.h"
#include "node_discovery_impl.h"
#include "wiring_endpoint_listener.h"
#include "wiring_common_utils.h"

static celix_status_t node_discovery_createOwnNodeDescription(node_discovery_pt node_discovery, node_description_pt* node_description) {
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

		properties_pt props = properties_create();
		properties_set(props, NODE_DESCRIPTION_NODE_IDENTIFIER_KEY, inNodeIdentifier);
		properties_set(props, NODE_DESCRIPTION_ZONE_IDENTIFIER_KEY, inZoneIdentifier);

		nodeDescription_create(fwuuid, props, node_description);
	}

	return status;
}

celix_status_t node_discovery_create(bundle_context_pt context, node_discovery_pt *node_discovery) {
	celix_status_t status = CELIX_SUCCESS;

	*node_discovery = calloc(1, sizeof(**node_discovery));

	if (!*node_discovery) {
		status = CELIX_ENOMEM;
	} else {
		(*node_discovery)->context = context;
		(*node_discovery)->discoveredNodes = hashMap_create(utils_stringHash, NULL, utils_stringEquals, NULL);
		(*node_discovery)->listenerReferences = hashMap_create(serviceReference_hashCode, NULL, serviceReference_equals2, NULL);

		status = celixThreadMutex_create(&(*node_discovery)->listenerReferencesMutex, NULL);
		status = celixThreadMutex_create(&(*node_discovery)->discoveredNodesMutex, NULL);
		status = celixThreadMutex_create(&(*node_discovery)->ownNodeMutex, NULL);

		node_discovery_createOwnNodeDescription((*node_discovery), &(*node_discovery)->ownNode);

	}

	return status;
}

celix_status_t node_discovery_destroy(node_discovery_pt node_discovery) {
	celix_status_t status = CELIX_SUCCESS;

	celixThreadMutex_lock(&node_discovery->discoveredNodesMutex);

	hash_map_iterator_pt iter = hashMapIterator_create(node_discovery->discoveredNodes);

	while (hashMapIterator_hasNext(iter)) {
		node_description_pt node_desc = (node_description_pt) hashMapIterator_nextValue(iter);
		nodeDescription_destroy(node_desc, true);
	}

	hashMapIterator_destroy(iter);

	hashMap_destroy(node_discovery->discoveredNodes, true, false);
	node_discovery->discoveredNodes = NULL;

	celixThreadMutex_unlock(&node_discovery->discoveredNodesMutex);

	celixThreadMutex_destroy(&node_discovery->discoveredNodesMutex);

	celixThreadMutex_lock(&node_discovery->listenerReferencesMutex);

	hashMap_destroy(node_discovery->listenerReferences, false, false);
	node_discovery->listenerReferences = NULL;

	celixThreadMutex_unlock(&node_discovery->listenerReferencesMutex);

	celixThreadMutex_destroy(&node_discovery->listenerReferencesMutex);

	celixThreadMutex_lock(&node_discovery->ownNodeMutex);
	nodeDescription_destroy(node_discovery->ownNode, false);
	celixThreadMutex_unlock(&node_discovery->ownNodeMutex);
	celixThreadMutex_destroy(&node_discovery->ownNodeMutex);

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
		return CELIX_BUNDLE_EXCEPTION;
	}

	celixThreadMutex_lock(&node_discovery->discoveredNodesMutex);

	hash_map_iterator_pt node_iter = hashMapIterator_create(node_discovery->discoveredNodes);
	while (hashMapIterator_hasNext(node_iter)) {
		node_description_pt node_desc = hashMapIterator_nextValue(node_iter);

		celixThreadMutex_lock(&node_desc->wiring_ep_desc_list_lock);

		array_list_iterator_pt wep_it = arrayListIterator_create(node_desc->wiring_ep_descriptions_list);

		while (arrayListIterator_hasNext(wep_it)) {
			wiring_endpoint_description_pt wep = arrayListIterator_next(wep_it);
			node_discovery_informWiringEndpointListeners(node_discovery, wep, false);
		}

		arrayListIterator_destroy(wep_it);

		celixThreadMutex_unlock(&node_desc->wiring_ep_desc_list_lock);

	}

	hashMapIterator_destroy(node_iter);

	celixThreadMutex_unlock(&node_discovery->discoveredNodesMutex);

	return status;
}

celix_status_t node_discovery_addNode(node_discovery_pt node_discovery, char* key, node_description_pt node_desc) {
	celix_status_t status = CELIX_SUCCESS;

	celixThreadMutex_lock(&node_discovery->discoveredNodesMutex);

	bool exists = hashMap_get(node_discovery->discoveredNodes, key);
	if (!exists) {
		hashMap_put(node_discovery->discoveredNodes, strdup(key), node_desc);
		printf("\nNODE_DISCOVERY: Node %s added\n", key);
		//dump_node_description(node_desc);
	}
	celixThreadMutex_unlock(&node_discovery->discoveredNodesMutex);

	celixThreadMutex_lock(&node_desc->wiring_ep_desc_list_lock);

	array_list_iterator_pt wep_it = arrayListIterator_create(node_desc->wiring_ep_descriptions_list);

	while (arrayListIterator_hasNext(wep_it)) {
		wiring_endpoint_description_pt wep = arrayListIterator_next(wep_it);
		node_discovery_informWiringEndpointListeners(node_discovery, wep, true);
	}

	arrayListIterator_destroy(wep_it);

	celixThreadMutex_unlock(&node_desc->wiring_ep_desc_list_lock);

	return status;
}

celix_status_t node_discovery_removeNode(node_discovery_pt node_discovery, char* key) {
	celix_status_t status = CELIX_SUCCESS;

	char* node_key = NULL;
	node_description_pt node_desc = NULL;

	celixThreadMutex_lock(&node_discovery->discoveredNodesMutex);

	hash_map_entry_pt entry = hashMap_getEntry(node_discovery->discoveredNodes, key);
	if (entry != NULL) {
		node_key = hashMapEntry_getKey(entry);
		node_desc = hashMapEntry_getValue(entry);

		hashMap_remove(node_discovery->discoveredNodes, key);
		free(node_key);
	}
	celixThreadMutex_unlock(&node_discovery->discoveredNodesMutex);

	if (node_desc != NULL) {

		celixThreadMutex_lock(&node_desc->wiring_ep_desc_list_lock);

		array_list_iterator_pt wep_it = arrayListIterator_create(node_desc->wiring_ep_descriptions_list);

		while (arrayListIterator_hasNext(wep_it)) {
			wiring_endpoint_description_pt wep = arrayListIterator_next(wep_it);
			node_discovery_informWiringEndpointListeners(node_discovery, wep, false);
		}

		arrayListIterator_destroy(wep_it);

		celixThreadMutex_unlock(&node_desc->wiring_ep_desc_list_lock);

		nodeDescription_destroy(node_desc, true);
	}

	printf("\nNODE_DISCOVERY: Node %s removed\n", key);

	return status;
}

celix_status_t node_discovery_informWiringEndpointListeners(node_discovery_pt node_discovery, wiring_endpoint_description_pt wEndpoint, bool wEndpointAdded) {
	celix_status_t status = CELIX_SUCCESS;

	// Inform listeners of new endpoint
	status = celixThreadMutex_lock(&node_discovery->listenerReferencesMutex);

	if (node_discovery->listenerReferences != NULL) {
		hash_map_iterator_pt iter = hashMapIterator_create(node_discovery->listenerReferences);
		while (hashMapIterator_hasNext(iter)) {
			hash_map_entry_pt entry = hashMapIterator_nextEntry(iter);

			service_reference_pt reference = hashMapEntry_getKey(entry);
			wiring_endpoint_listener_pt listener = NULL;

			char *scope = NULL;
			serviceReference_getProperty(reference, (char *) INAETICS_WIRING_ENDPOINT_LISTENER_SCOPE, &scope);

			filter_pt filter = filter_create(scope);
			bool matchResult = false;

			status = filter_match(filter, wEndpoint->properties, &matchResult);
			if (matchResult) {
				bundleContext_getService(node_discovery->context, reference, (void**) &listener);
				if (wEndpointAdded) {

					listener->wiringEndpointAdded(listener->handle, wEndpoint, scope);
				} else {
					listener->wiringEndpointRemoved(listener->handle, wEndpoint, scope);
				}
			}

			filter_destroy(filter);
		}
		hashMapIterator_destroy(iter);
	}

	status = celixThreadMutex_unlock(&node_discovery->listenerReferencesMutex);

	return status;
}

celix_status_t node_discovery_wiringEndpointAdded(void *handle, wiring_endpoint_description_pt wEndpoint, char *matchedFilter) {
	celix_status_t status = CELIX_SUCCESS;

	node_discovery_pt node_discovery = handle;

	celixThreadMutex_lock(&node_discovery->ownNodeMutex);

	// Wiring Endpoint Description added by WTM must have the same UUID as the local frameworkUUID
	if (strcmp(wEndpoint->frameworkUUID, node_discovery->ownNode->nodeId) != 0) {
		printf("NODE_DISCOVERY: WEPListener service trying to add WEPDescription with wrong UUID (wep_uuid=%s,local_uuid=%s)\n", wEndpoint->frameworkUUID, node_discovery->ownNode->nodeId);
		status = CELIX_ILLEGAL_STATE;
	}

	celixThreadMutex_lock(&(node_discovery->ownNode->wiring_ep_desc_list_lock));

	array_list_iterator_pt wep_it = arrayListIterator_create(node_discovery->ownNode->wiring_ep_descriptions_list);

	while (arrayListIterator_hasNext(wep_it) && status == CELIX_SUCCESS) {
		wiring_endpoint_description_pt wep = arrayListIterator_next(wep_it);

		// Trying to add twice the same Wiring Endpoint Description should never happen!!
		if (!strcmp(wEndpoint->url, wep->url)) {
			printf("NODE_DISCOVERY: WEPListener service trying to add already existing WEPDescription (wep_url=%s)\n", wEndpoint->url);
			status = CELIX_ILLEGAL_STATE;
		}
	}

	arrayListIterator_destroy(wep_it);

	if (status == CELIX_SUCCESS) { // No problems , we can add the new Wiring Endpoint Description
		arrayList_add(node_discovery->ownNode->wiring_ep_descriptions_list, wEndpoint);
		etcdWatcher_addOwnNode(node_discovery->watcher);
	}

	celixThreadMutex_unlock(&(node_discovery->ownNode->wiring_ep_desc_list_lock));
	celixThreadMutex_unlock(&node_discovery->ownNodeMutex);

	return status;
}

celix_status_t node_discovery_wiringEndpointRemoved(void *handle, wiring_endpoint_description_pt wEndpoint, char *matchedFilter) {
	celix_status_t status = CELIX_SUCCESS;

	node_discovery_pt node_discovery = handle;

	celixThreadMutex_lock(&node_discovery->ownNodeMutex);

	// Wiring Endpoint Description removed by WTM must have the same UUID as the local frameworkUUID
	if (strcmp(wEndpoint->frameworkUUID, node_discovery->ownNode->nodeId) != 0) {
		printf("NODE_DISCOVERY: WEPListener service trying to remove WEPDescription with wrong UUID (wep_uuid=%s,local_uuid=%s)\n", wEndpoint->frameworkUUID, node_discovery->ownNode->nodeId);
		status = CELIX_ILLEGAL_STATE;
	}

	int i = 0;

	celixThreadMutex_lock(&(node_discovery->ownNode->wiring_ep_desc_list_lock));

	for (; i < arrayList_size(node_discovery->ownNode->wiring_ep_descriptions_list); i++) {
		wiring_endpoint_description_pt wep = arrayList_get(node_discovery->ownNode->wiring_ep_descriptions_list, i);

		if (!strcmp(wEndpoint->url, wep->url)) {
			wep = arrayList_remove(node_discovery->ownNode->wiring_ep_descriptions_list, i);
			//wiringEndpointDescription_destroy(wep);
			break;
		}

	}

	celixThreadMutex_unlock(&(node_discovery->ownNode->wiring_ep_desc_list_lock));

	// Node Description update in ETCD is done by the etcd_watcher_run thread periodically

	celixThreadMutex_unlock(&node_discovery->ownNodeMutex);

	return status;
}

celix_status_t node_discovery_wiringEndpointListenerAdding(void * handle, service_reference_pt reference, void **service) {
	celix_status_t status = CELIX_SUCCESS;

	node_discovery_pt node_discovery = handle;

	status = bundleContext_getService(node_discovery->context, reference, service);

	return status;
}

celix_status_t node_discovery_wiringEndpointListenerAdded(void * handle, service_reference_pt reference, void * service) {
	celix_status_t status = CELIX_SUCCESS;

	node_discovery_pt nodeDiscovery = handle;

	char *nodeDiscoveryListener = NULL;
	serviceReference_getProperty(reference, "NODE_DISCOVERY", &nodeDiscoveryListener);
	char *scope = NULL;
	serviceReference_getProperty(reference, (char *) INAETICS_WIRING_ENDPOINT_LISTENER_SCOPE, &scope);

	filter_pt filter = filter_create(scope);

	if (nodeDiscoveryListener != NULL && strcmp(nodeDiscoveryListener, "true") == 0) {
		//printf("NODE_DISCOVERY: Ignoring my WiringEndpointListener\n");
	} else {
		celixThreadMutex_lock(&nodeDiscovery->discoveredNodesMutex);

		hash_map_iterator_pt iter = hashMapIterator_create(nodeDiscovery->discoveredNodes);
		while (hashMapIterator_hasNext(iter)) {
			node_description_pt node_desc = hashMapIterator_nextValue(iter);

			int i = 0;

			celixThreadMutex_lock(&node_desc->wiring_ep_desc_list_lock);

			for (; i < arrayList_size(node_desc->wiring_ep_descriptions_list); i++) {
				wiring_endpoint_description_pt ep_desc = arrayList_get(node_desc->wiring_ep_descriptions_list, i);

				bool matchResult = false;
				filter_match(filter, ep_desc->properties, &matchResult);
				if (matchResult) {
					wiring_endpoint_listener_pt listener = service;

					listener->wiringEndpointAdded(listener, ep_desc, NULL);
				}

			}

			celixThreadMutex_unlock(&node_desc->wiring_ep_desc_list_lock);
		}
		hashMapIterator_destroy(iter);

		celixThreadMutex_unlock(&nodeDiscovery->discoveredNodesMutex);

		celixThreadMutex_lock(&nodeDiscovery->listenerReferencesMutex);

		hashMap_put(nodeDiscovery->listenerReferences, reference, NULL);

		printf("NODE_DISCOVERY: WiringEndpointListener Added\n");

		celixThreadMutex_unlock(&nodeDiscovery->listenerReferencesMutex);
	}

	filter_destroy(filter);

	return status;
}

celix_status_t node_discovery_wiringEndpointListenerModified(void * handle, service_reference_pt reference, void * service) {
	celix_status_t status = CELIX_SUCCESS;

	status = node_discovery_wiringEndpointListenerRemoved(handle, reference, service);
	status = node_discovery_wiringEndpointListenerAdded(handle, reference, service);

	return status;
}

celix_status_t node_discovery_wiringEndpointListenerRemoved(void * handle, service_reference_pt reference, void * service) {
	celix_status_t status = CELIX_SUCCESS;

	node_discovery_pt nodeDiscovery = handle;

	status = celixThreadMutex_lock(&nodeDiscovery->listenerReferencesMutex);

	if (nodeDiscovery->listenerReferences != NULL) {
		if (hashMap_remove(nodeDiscovery->listenerReferences, reference)) {
			printf("NODE_DISCOVERY: WiringEndpointListener Removed\n");
		}
	}

	status = celixThreadMutex_unlock(&nodeDiscovery->listenerReferencesMutex);

	return status;
}

