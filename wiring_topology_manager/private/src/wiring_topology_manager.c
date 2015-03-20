/**
 * Licensed under Apache License v2. See LICENSE for more information.
 */

#include <stdio.h>
#include <stdlib.h>

#include "bundle_context.h"
#include "constants.h"
#include "module.h"
#include "bundle.h"
#include "filter.h"
#include "utils.h"
#include "service_reference.h"
#include "service_registration.h"
#include "wiring_topology_manager_impl.h"
#include "wiring_admin.h"
#include "wiring_endpoint_description.h"

struct wiring_topology_manager {
	bundle_context_pt context;

	celix_thread_mutex_t waListLock;
	array_list_pt waList;

	celix_thread_mutex_t exportedWiringEndpointsLock;
	hash_map_pt exportedWiringEndpoints;

	celix_thread_mutex_t importedWiringEndpointsLock;
	hash_map_pt importedWiringEndpoints;

};

typedef struct wiring_endpoint_registration {
	wiring_endpoint_description_pt wiringEndpointDescription;
	wiring_admin_service_pt wiringAdminService;
}* wiring_endpoint_registration_pt;

static celix_status_t wiringTopologyManager_notifyListenersWiringEndpointAdded(wiring_topology_manager_pt manager, wiring_endpoint_description_pt wEndpoint);
static celix_status_t wiringTopologyManager_notifyListenersWiringEndpointRemoved(wiring_topology_manager_pt manager, wiring_endpoint_description_pt wEndpoint);

static bool properties_match(properties_pt properties, properties_pt reference);

static celix_status_t wiringTopologyManager_WiringAdminServiceExportWiringEndpoint(wiring_topology_manager_pt manager, wiring_admin_service_pt wiringAdminService, properties_pt srvcProperties, wiring_endpoint_description_pt* wEndpoint);
static celix_status_t wiringTopologyManager_WiringAdminServiceImportWiringEndpoint(wiring_topology_manager_pt manager, wiring_admin_service_pt wiringAdminService, wiring_endpoint_description_pt wEndpoint);

celix_status_t wiringTopologyManager_create(bundle_context_pt context, wiring_topology_manager_pt *manager) {
	celix_status_t status = CELIX_SUCCESS;

	*manager = malloc(sizeof(**manager));
	if (!*manager) {
		return CELIX_ENOMEM;
	}

	(*manager)->context = context;
	(*manager)->waList = NULL;
	arrayList_create(&((*manager)->waList));

	celixThreadMutex_create(&((*manager)->waListLock), NULL);
	celixThreadMutex_create(&((*manager)->importedWiringEndpointsLock), NULL);
	celixThreadMutex_create(&((*manager)->exportedWiringEndpointsLock), NULL);

	(*manager)->importedWiringEndpoints = hashMap_create(wiringEndpointDescription_hash, NULL, wiringEndpointDescription_equals, NULL); // key=wiring_endpoint_description_pt, value=array_list_pt wadmins
	(*manager)->exportedWiringEndpoints = hashMap_create(NULL, NULL, NULL, NULL); // key=properties_pt, value=(hash_map_pt  key=wadmin, value=wendpoint)

	return status;
}

celix_status_t wiringTopologyManager_destroy(wiring_topology_manager_pt manager) {
	celix_status_t status = CELIX_SUCCESS;

	celixThreadMutex_lock(&manager->waListLock);

	arrayList_destroy(manager->waList);

	celixThreadMutex_unlock(&manager->waListLock);
	celixThreadMutex_destroy(&manager->waListLock);

	celixThreadMutex_lock(&manager->importedWiringEndpointsLock);

	hashMap_destroy(manager->importedWiringEndpoints, false, false);
	celixThreadMutex_unlock(&manager->importedWiringEndpointsLock);
	celixThreadMutex_destroy(&manager->importedWiringEndpointsLock);

	celixThreadMutex_lock(&manager->exportedWiringEndpointsLock);

	hash_map_iterator_pt iter = hashMapIterator_create(manager->exportedWiringEndpoints);

	while (hashMapIterator_hasNext(iter)) {
		hash_map_entry_pt entry = hashMapIterator_nextEntry(iter);
		hash_map_pt wiringAdminList = hashMapEntry_getValue(entry);

		hashMap_destroy(wiringAdminList, false, false);

	}
	hashMapIterator_destroy(iter);

	hashMap_destroy(manager->exportedWiringEndpoints, false, false);

	celixThreadMutex_unlock(&manager->exportedWiringEndpointsLock);
	celixThreadMutex_destroy(&manager->exportedWiringEndpointsLock);

	free(manager);

	return status;
}

static celix_status_t wiringTopologyManager_getWAs(wiring_topology_manager_pt manager, array_list_pt *waList) {
	celix_status_t status = CELIX_SUCCESS;

	status = arrayList_create(waList);
	if (status != CELIX_SUCCESS) {
		return CELIX_ENOMEM;
	}

	status = celixThreadMutex_lock(&manager->waListLock);
	arrayList_addAll(*waList, manager->waList);
	status = celixThreadMutex_unlock(&manager->waListLock);

	return status;
}

/* Wiring Admin Tracker functions */

celix_status_t wiringTopologyManager_waAdding(void * handle, service_reference_pt reference, void **service) {
	celix_status_t status = CELIX_SUCCESS;

	wiring_topology_manager_pt manager = handle;

	status = bundleContext_getService(manager->context, reference, service);

	return status;
}

celix_status_t wiringTopologyManager_waAdded(void * handle, service_reference_pt reference, void * service) {
	celix_status_t status = CELIX_SUCCESS;
	wiring_topology_manager_pt manager = handle;
	wiring_admin_service_pt wiringAdminService = (wiring_admin_service_pt) service;

	celixThreadMutex_lock(&manager->waListLock);
	arrayList_add(manager->waList, wiringAdminService);
	celixThreadMutex_unlock(&manager->waListLock);

	/* check whether one of the exported Wires can be exported via the newly available wiringAdmin */
	celixThreadMutex_lock(&manager->exportedWiringEndpointsLock);
	hash_map_iterator_pt iter = hashMapIterator_create(manager->exportedWiringEndpoints);
	while (hashMapIterator_hasNext(iter)) {
		hash_map_entry_pt entry = hashMapIterator_nextEntry(iter);
		properties_pt exportedWireProperties = hashMapEntry_getKey(entry);
		hash_map_pt wiringAdminList = hashMapEntry_getValue(entry);
		wiring_endpoint_description_pt wEndpoint = NULL;

		status = wiringTopologyManager_WiringAdminServiceExportWiringEndpoint(manager, wiringAdminService, exportedWireProperties, &wEndpoint);

		if (status == CELIX_SUCCESS) {
			hashMap_put(wiringAdminList, wiringAdminService, wEndpoint);
		}
	}
	hashMapIterator_destroy(iter);

	celixThreadMutex_unlock(&manager->exportedWiringEndpointsLock);

	/* Check if the added WA can match one of the imported WiringEndpoints */
	celixThreadMutex_lock(&manager->importedWiringEndpointsLock);
	iter = hashMapIterator_create(manager->importedWiringEndpoints);
	while (hashMapIterator_hasNext(iter)) {
		hash_map_entry_pt entry = hashMapIterator_nextEntry(iter);
		wiring_endpoint_description_pt wEndpoint = hashMapEntry_getKey(entry);
		array_list_pt wiringAdminList = hashMapEntry_getValue(entry);

		status = wiringTopologyManager_WiringAdminServiceImportWiringEndpoint(manager, wiringAdminService, wEndpoint);

		if (status == CELIX_SUCCESS) {
			arrayList_add(wiringAdminList, wiringAdminService);
		}

	}
	hashMapIterator_destroy(iter);

	celixThreadMutex_unlock(&manager->importedWiringEndpointsLock);

	printf("WTM: Added WA\n");

	return status;
}

celix_status_t wiringTopologyManager_waModified(void * handle, service_reference_pt reference, void * service) {
	celix_status_t status = CELIX_SUCCESS;

	// Nothing to do here

	return status;
}

celix_status_t wiringTopologyManager_waRemoved(void * handle, service_reference_pt reference, void * service) {
	celix_status_t status = CELIX_SUCCESS;
	wiring_topology_manager_pt manager = handle;
	wiring_admin_service_pt wiringAdminService = (wiring_admin_service_pt) service;

	/* check whether one of the exported Wires can be exported here via the newly available wiringAdmin*/
	celixThreadMutex_lock(&manager->exportedWiringEndpointsLock);
	hash_map_iterator_pt iter = hashMapIterator_create(manager->exportedWiringEndpoints);
	while (hashMapIterator_hasNext(iter)) {
		hash_map_entry_pt entry = hashMapIterator_nextEntry(iter);
		hash_map_pt wiringAdminMap = hashMapEntry_getValue(entry);

		if (hashMap_containsKey(wiringAdminMap, wiringAdminService)) {
			wiring_endpoint_description_pt wEndpoint = (wiring_endpoint_description_pt) hashMap_remove(wiringAdminMap, wiringAdminService);

			status = wiringTopologyManager_notifyListenersWiringEndpointRemoved(manager, wEndpoint);

			if (status == CELIX_SUCCESS) {
				status = wiringAdminService->removeExportedWiringEndpoint(wiringAdminService->admin, wEndpoint);
			} else {
				printf("WTM: failed while removing WiringAdmin.\n");
			}
		}
	}

	hashMapIterator_destroy(iter);

	celixThreadMutex_unlock(&manager->exportedWiringEndpointsLock);

	/* Check if the added WA can match one of the imported WiringEndpoints */
	celixThreadMutex_lock(&manager->importedWiringEndpointsLock);
	iter = hashMapIterator_create(manager->importedWiringEndpoints);
	while (hashMapIterator_hasNext(iter)) {
		hash_map_entry_pt entry = hashMapIterator_nextEntry(iter);
		wiring_endpoint_description_pt importedWiringEndpointDesc = hashMapEntry_getKey(entry);
		array_list_pt wiringAdminList = hashMapEntry_getValue(entry);

		if (arrayList_contains(wiringAdminList, wiringAdminService)) {
			status = wiringAdminService->removeImportedWiringEndpoint(wiringAdminService->admin, importedWiringEndpointDesc);
			arrayList_removeElement(wiringAdminList, wiringAdminService);
		}

		if (status == CELIX_SUCCESS) {
			arrayList_add(wiringAdminList, wiringAdminService);
		}

	}
	hashMapIterator_destroy(iter);
	celixThreadMutex_unlock(&manager->importedWiringEndpointsLock);

	celixThreadMutex_lock(&manager->waListLock);
	arrayList_removeElement(manager->waList, wiringAdminService);
	celixThreadMutex_unlock(&manager->waListLock);

	printf("WTM: Removed WA\n");

	return status;
}

static celix_status_t wiringTopologyManager_WiringAdminServiceImportWiringEndpoint(wiring_topology_manager_pt manager, wiring_admin_service_pt wiringAdminService, wiring_endpoint_description_pt wEndpoint) {
	celix_status_t status = CELIX_BUNDLE_EXCEPTION;
	properties_pt adminProperties = NULL;

	wiringAdminService->getWiringAdminProperties(wiringAdminService->admin, &adminProperties);

	if (adminProperties != NULL) {

		/* only a wiringAdmin which provides the same config can import the endpoint */
		char* wiringConfigEndpoint = properties_get(wEndpoint->properties, WIRING_ADMIN_PROPERTIES_CONFIG_KEY);
		char* wiringConfigAdmin = properties_get(adminProperties, WIRING_ADMIN_PROPERTIES_CONFIG_KEY);

		if ((wiringConfigEndpoint != NULL) && (wiringConfigAdmin != NULL) && (strcmp(wiringConfigEndpoint, wiringConfigAdmin) == 0)) {
			status = wiringAdminService->importWiringEndpoint(wiringAdminService->admin, wEndpoint);

			if (status != CELIX_SUCCESS) {
				printf("WTM: importWiringEndpoint failed\n");
			}
		} else {
			printf("WTM: Wiring Admin does not match requirements (%s=%s)\n", wiringConfigEndpoint, wiringConfigAdmin);
		}
	}

	return status;
}

/* Functions for wiring endpoint listener */
celix_status_t wiringTopologyManager_addImportedWiringEndpoint(void *handle, wiring_endpoint_description_pt wEndpoint, char *matchedFilter) {
	celix_status_t status = CELIX_SUCCESS;
	wiring_endpoint_listener_pt listener = (wiring_endpoint_listener_pt) handle;
	wiring_topology_manager_pt manager = (wiring_topology_manager_pt) listener->handle;

	celixThreadMutex_lock(&manager->importedWiringEndpointsLock);

	array_list_pt wiringAdminList = hashMap_get(manager->importedWiringEndpoints, wEndpoint);

	if (wiringAdminList == NULL) {
		arrayList_create(&wiringAdminList);
		hashMap_put(manager->importedWiringEndpoints, wEndpoint, wiringAdminList);
	}

	celixThreadMutex_unlock(&manager->importedWiringEndpointsLock);

	return status;
}

celix_status_t wiringTopologyManager_removeImportedWiringEndpoint(void *handle, wiring_endpoint_description_pt wEndpoint, char *matchedFilter) {
	celix_status_t status = CELIX_SUCCESS;
	wiring_endpoint_listener_pt listener = (wiring_endpoint_listener_pt) handle;
	wiring_topology_manager_pt manager = (wiring_topology_manager_pt) listener->handle;

	celixThreadMutex_lock(&manager->importedWiringEndpointsLock);
	char* wireId = wEndpoint->wireId;

	array_list_pt wiringAdminList = hashMap_remove(manager->importedWiringEndpoints, wEndpoint);

	if (wiringAdminList != NULL) {

		int i = 0;
		int size = arrayList_size(wiringAdminList);

		for (; i < size; ++i) {
			wiring_admin_service_pt wiringAdminService = (wiring_admin_service_pt) arrayList_get(wiringAdminList, i);

			wiringAdminService->removeImportedWiringEndpoint(wiringAdminService->admin, wEndpoint);
		}

		arrayList_destroy(wiringAdminList);

		printf("WTM: Removing imported wiring endpoint (%s).\n", wireId);
	} else {
		printf("WTM: Removing of imported wiring endpoint (%s) failed.\n", wireId);
	}

	celixThreadMutex_unlock(&manager->importedWiringEndpointsLock);

	return status;
}

/* Return true if all pairs in properties are contained in reference */
static bool properties_match(properties_pt properties, properties_pt reference) {

	bool matching = true;

	hash_map_iterator_pt iter = hashMapIterator_create(properties);
	while (hashMapIterator_hasNext(iter) && matching) {
		hash_map_entry_pt prop_pair = hashMapIterator_nextEntry(iter);
		char* prop_key = (char*) hashMapEntry_getKey(prop_pair);
		char* prop_value = (char*) hashMapEntry_getValue(prop_pair);

		printf("check prop %s\n", prop_key);

		/*OSGI_RSA_ENDPOINT_FRAMEWORK_UUID (endpoint.framework.uuid) is a special property that shouldn't be taken in account*/
		if (strcmp(prop_key, OSGI_RSA_ENDPOINT_FRAMEWORK_UUID) == 0) {
			continue;
		}

		char* ref_value = (char*) hashMap_get(reference, prop_key);
		if (ref_value == NULL || (strcmp(ref_value, prop_value) != 0)) {
			printf("comparisionn of  val %s against %s failed s\n", ref_value, prop_value);
			matching = false; // We found a pair in properties not included in reference
		}
	}
	hashMapIterator_destroy(iter);

	return matching;

}

static celix_status_t wiringTopologyManager_WiringAdminServiceExportWiringEndpoint(wiring_topology_manager_pt manager, wiring_admin_service_pt wiringAdminService, properties_pt srvcProperties, wiring_endpoint_description_pt* wEndpoint) {
	celix_status_t status = CELIX_BUNDLE_EXCEPTION;
	properties_pt adminProperties = NULL;

	/* retrieve capabilities of wiringAdmin */
	wiringAdminService->getWiringAdminProperties(wiringAdminService->admin, &adminProperties);

	if (adminProperties != NULL) {

		/* check whether the wiringAdmin can fulfill what is requested by the service */
		if (properties_match(srvcProperties, adminProperties) == true) {
			status = wiringAdminService->exportWiringEndpoint(wiringAdminService->admin, wEndpoint);

			if (status != CELIX_SUCCESS) {
				printf("WA: export of WiringAdmin failed\n");
			} else {
				status = wiringTopologyManager_notifyListenersWiringEndpointAdded(manager, *wEndpoint);
			}
		}
	}

	return status;
}

celix_status_t wiringTopologyManager_exportWiringEndpoint(wiring_topology_manager_pt manager, properties_pt srvcProperties) {
	celix_status_t status = CELIX_BUNDLE_EXCEPTION;

	if (srvcProperties == NULL) {
		status = CELIX_ILLEGAL_ARGUMENT;
	} else {
		celixThreadMutex_lock(&manager->exportedWiringEndpointsLock);

		hash_map_pt wiringAdminList = hashMap_get(manager->exportedWiringEndpoints, srvcProperties);

		if (wiringAdminList == NULL) {
			wiringAdminList = hashMap_create(NULL, NULL, NULL, NULL);
			hashMap_put(manager->exportedWiringEndpoints, srvcProperties, wiringAdminList);
		}

		array_list_pt localWAs = NULL;
		wiring_endpoint_description_pt wEndpoint = NULL;
		wiringTopologyManager_getWAs(manager, &localWAs);

		int listCnt = 0;
		int listSize = arrayList_size(localWAs);

		for (; listCnt < listSize && (wEndpoint == NULL); ++listCnt) {
			wiring_admin_service_pt wiringAdminService = (wiring_admin_service_pt) arrayList_get(localWAs, listCnt);

			status = wiringTopologyManager_WiringAdminServiceExportWiringEndpoint(manager, wiringAdminService, srvcProperties, &wEndpoint);

			if (status == CELIX_SUCCESS) {
				hashMap_put(wiringAdminList, wiringAdminService, wEndpoint);
			}
		}

		arrayList_destroy(localWAs);

		celixThreadMutex_unlock(&manager->exportedWiringEndpointsLock);

		if (status != CELIX_SUCCESS) {
			printf("WTM: Could not install callback to any Wiring Endpoint\n");
		}
	}

	return status;
}

celix_status_t wiringTopologyManager_importWiringEndpoint(wiring_topology_manager_pt manager, properties_pt rsaProperties) {
	celix_status_t status = CELIX_SUCCESS;
	hash_map_iterator_pt iter = NULL;

	celixThreadMutex_lock(&manager->importedWiringEndpointsLock);
	iter = hashMapIterator_create(manager->importedWiringEndpoints);

	while (hashMapIterator_hasNext(iter)) {
		hash_map_entry_pt importedWiringEndpointEntry = (hash_map_entry_pt) hashMapIterator_nextEntry(iter);
		wiring_endpoint_description_pt wiringEndpointDesc = (wiring_endpoint_description_pt) hashMapEntry_getKey(importedWiringEndpointEntry);
		array_list_pt wiringAdminList = (array_list_pt) hashMapEntry_getValue(importedWiringEndpointEntry);

		// do we have a matching wiring endpoint
		if (properties_match(rsaProperties, wiringEndpointDesc->properties)) {
			array_list_pt localWAs = NULL;
			wiringTopologyManager_getWAs(manager, &localWAs);

			int listCnt = 0;
			int listSize = arrayList_size(localWAs);

			for (; listCnt < listSize; ++listCnt) {
				wiring_admin_service_pt wiringAdminService = (wiring_admin_service_pt) arrayList_get(localWAs, listCnt);

				if (arrayList_contains(wiringAdminList, wiringAdminService)) {
					printf("WTM: WiringEndpoint %s is already imported by WiringAdminService %p\n", wiringEndpointDesc->wireId, wiringAdminService);
				} else {
					status = wiringTopologyManager_WiringAdminServiceImportWiringEndpoint(manager, wiringAdminService, wiringEndpointDesc);

					if (status == CELIX_SUCCESS) {
						arrayList_add(wiringAdminList, wiringAdminService);
					}
				}
			}

			arrayList_destroy(localWAs);
		} else {
			printf("WTM: rsaProperties do not match imported Endpoint\n");
		}
	}

	hashMapIterator_destroy(iter);
	celixThreadMutex_unlock(&manager->importedWiringEndpointsLock);

	return status;
}

static celix_status_t wiringTopologyManager_notifyListenersWiringEndpointAdded(wiring_topology_manager_pt manager, wiring_endpoint_description_pt wEndpoint) {
	celix_status_t status = CELIX_SUCCESS;

	array_list_pt wiringEndpointListeners = NULL;
	status = bundleContext_getServiceReferences(manager->context, INAETICS_WIRING_ENDPOINT_LISTENER_SERVICE, NULL, &wiringEndpointListeners);
	if (status != CELIX_SUCCESS || !wiringEndpointListeners) {
		return CELIX_BUNDLE_EXCEPTION;
	}

	int eplSize = arrayList_size(wiringEndpointListeners);
	int eplIt = 0;
	for (; eplIt < eplSize; eplIt++) {
		service_reference_pt eplRef = arrayList_get(wiringEndpointListeners, eplIt);

		char *scope = NULL;
		serviceReference_getProperty(eplRef, (char *) INAETICS_WIRING_ENDPOINT_LISTENER_SCOPE, &scope);

		wiring_endpoint_listener_pt wepl = NULL;
		status = bundleContext_getService(manager->context, eplRef, (void **) &wepl);
		if (status != CELIX_SUCCESS || !wepl) {
			continue;
		}

		filter_pt filter = filter_create(scope);

		bool matchResult = false;
		filter_match(filter, wEndpoint->properties, &matchResult);

		if (matchResult) {
			status = wepl->wiringEndpointAdded(wepl, wEndpoint, scope);
		}

		filter_destroy(filter);
	}

	if (wiringEndpointListeners) {
		arrayList_destroy(wiringEndpointListeners);
	}

	return status;
}

static celix_status_t wiringTopologyManager_notifyListenersWiringEndpointRemoved(wiring_topology_manager_pt manager, wiring_endpoint_description_pt wEndpoint) {
	celix_status_t status = CELIX_SUCCESS;

	array_list_pt wiringEndpointListeners = NULL;
	status = bundleContext_getServiceReferences(manager->context, INAETICS_WIRING_ENDPOINT_LISTENER_SERVICE, NULL, &wiringEndpointListeners);
	if (status != CELIX_SUCCESS || !wiringEndpointListeners) {
		return CELIX_BUNDLE_EXCEPTION;
	}

	int eplIt = 0;
	for (; eplIt < arrayList_size(wiringEndpointListeners); ++eplIt) {
		service_reference_pt eplRef = arrayList_get(wiringEndpointListeners, eplIt);

		char *scope = NULL;
		serviceReference_getProperty(eplRef, (char *) INAETICS_WIRING_ENDPOINT_LISTENER_SCOPE, &scope);

		wiring_endpoint_listener_pt epl = NULL;
		status = bundleContext_getService(manager->context, eplRef, (void **) &epl);
		if (status != CELIX_SUCCESS || !epl) {
			continue;
		}

		filter_pt filter = filter_create(scope);

		bool matchResult = false;
		filter_match(filter, wEndpoint->properties, &matchResult);

		if (matchResult) {
			status = epl->wiringEndpointRemoved(epl->handle, wEndpoint, scope);
		}

		filter_destroy(filter);
	}

	if (wiringEndpointListeners) {
		arrayList_destroy(wiringEndpointListeners);
	}

	return status;
}
