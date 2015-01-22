/**
 *Licensed to the Apache Software Foundation (ASF) under one
 *or more contributor license agreements.  See the NOTICE file
 *distributed with this work for additional information
 *regarding copyright ownership.  The ASF licenses this file
 *to you under the Apache License, Version 2.0 (the
 *"License"); you may not use this file except in compliance
 *with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *Unless required by applicable law or agreed to in writing,
 *software distributed under the License is distributed on an
 *"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 *specific language governing permissions and limitations
 *under the License.
 */
/*
 * wiring_topology_manager.c
 *
 *  \date       Sep 29, 2011
 *  \author    	<a href="mailto:celix-dev@incubator.apache.org">Apache Celix Project Team</a>
 *  \copyright	Apache License, Version 2.0
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
#include "wiring_topology_manager.h"
#include "wiring_admin.h"

struct wiring_topology_manager {
	bundle_context_pt context;

	celix_thread_mutex_t waListLock;
	array_list_pt waList;

	celix_thread_mutex_t exportedWiringEndpointsLock;
	hash_map_pt exportedWiringEndpoints;

	celix_thread_mutex_t importedWiringEndpointsLock;
	hash_map_pt importedWiringEndpoints;

};


celix_status_t wiringTopologyManager_notifyListenersWiringEndpointAdded(wiring_topology_manager_pt manager, wiring_admin_service_pt wa,  array_list_pt registrations);
celix_status_t wiringTopologyManager_notifyListenersWiringEndpointRemoved(wiring_topology_manager_pt manager, wiring_admin_service_pt rsa,  wiring_export_registration_pt export);

static celix_status_t wiringTopologyManager_getWiringEndpointDescriptionForWiringExportRegistration(wiring_admin_service_pt wa, wiring_export_registration_pt export, wiring_endpoint_description_pt *endpoint);

celix_status_t wiringTopologyManager_create(bundle_context_pt context, wiring_topology_manager_pt *manager) {
	celix_status_t status = CELIX_SUCCESS;

	*manager = malloc(sizeof(**manager));
	if (!*manager) {
		return CELIX_ENOMEM;
	}

	(*manager)->context = context;
	(*manager)->waList = NULL;
	arrayList_create(&(*manager)->waList);

	status = celixThreadMutex_create(&(*manager)->waListLock, NULL);
	status = celixThreadMutex_create(&(*manager)->exportedWiringEndpointsLock, NULL);
	status = celixThreadMutex_create(&(*manager)->importedWiringEndpointsLock, NULL);

	(*manager)->exportedWiringEndpoints = hashMap_create(serviceReference_hashCode, NULL, serviceReference_equals2, NULL);
	(*manager)->importedWiringEndpoints = hashMap_create(NULL, NULL, NULL, NULL);

	return status;
}

celix_status_t wiringTopologyManager_destroy(wiring_topology_manager_pt manager) {
	celix_status_t status = CELIX_SUCCESS;

	status = celixThreadMutex_lock(&manager->waListLock);

	arrayList_destroy(manager->waList);

	status = celixThreadMutex_unlock(&manager->waListLock);
	status = celixThreadMutex_destroy(&manager->waListLock);

	status = celixThreadMutex_lock(&manager->exportedWiringEndpointsLock);

	hashMap_destroy(manager->exportedWiringEndpoints, false, false);

	status = celixThreadMutex_unlock(&manager->exportedWiringEndpointsLock);
	status = celixThreadMutex_destroy(&manager->exportedWiringEndpointsLock);

	status = celixThreadMutex_lock(&manager->importedWiringEndpointsLock);

	hashMap_destroy(manager->importedWiringEndpoints, false, false);

	status = celixThreadMutex_unlock(&manager->importedWiringEndpointsLock);
	status = celixThreadMutex_destroy(&manager->importedWiringEndpointsLock);

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
		wiring_admin_service_pt wa = (wiring_admin_service_pt) service;
		printf("WIRING_TOPOLOGY_MANAGER: Added WA\n");

		status = celixThreadMutex_lock(&manager->waListLock);
		arrayList_add(manager->waList, wa);
		status = celixThreadMutex_unlock(&manager->waListLock);

		// add already imported wiring endpoints to new wa
		status = celixThreadMutex_lock(&manager->importedWiringEndpointsLock);
		hash_map_iterator_pt importedWiringEndpointsIterator = hashMapIterator_create(manager->importedWiringEndpoints);

	    while (hashMapIterator_hasNext(importedWiringEndpointsIterator)) {
		    hash_map_entry_pt entry = hashMapIterator_nextEntry(importedWiringEndpointsIterator);
		    wiring_endpoint_description_pt wEndpoint = hashMapEntry_getKey(entry);
			wiring_import_registration_pt import = NULL;

			status = wa->importWiringEndpoint(wa->admin, wEndpoint, &import);

			if (status == CELIX_SUCCESS) {
			    hash_map_pt imports = hashMapEntry_getValue(entry);

				if (imports == NULL) {
					imports = hashMap_create(NULL, NULL, NULL, NULL);
				}

				hashMap_put(imports, service, import);
			}
		}

	    hashMapIterator_destroy(importedWiringEndpointsIterator);

		status = celixThreadMutex_unlock(&manager->importedWiringEndpointsLock);

		// add already exported wiring endpoint to new wa
		status = celixThreadMutex_lock(&manager->exportedWiringEndpointsLock);
	    hash_map_iterator_pt exportedWiringEndpointIterator = hashMapIterator_create(manager->exportedWiringEndpoints);

	    while (hashMapIterator_hasNext(exportedWiringEndpointIterator)) {
		    hash_map_entry_pt entry = hashMapIterator_nextEntry(exportedWiringEndpointIterator);
		    service_reference_pt reference = hashMapEntry_getKey(entry);
		    char *serviceId = NULL;

		    serviceReference_getProperty(reference, (char *)OSGI_FRAMEWORK_SERVICE_ID, &serviceId);

		    array_list_pt wEndpoints = NULL;
			status = wa->exportWiringEndpoint(wa->admin, serviceId, NULL, &wEndpoints);

			if (status == CELIX_SUCCESS) {
				hash_map_pt exports = hashMapEntry_getValue(entry);

				if (exports == NULL) {
					exports = hashMap_create(NULL, NULL, NULL, NULL);
				}

				hashMap_put(exports, wa, wEndpoints);
				status = wiringTopologyManager_notifyListenersWiringEndpointAdded(manager, wa, wEndpoints);
			}
	    }

	    hashMapIterator_destroy(exportedWiringEndpointIterator);

		status = celixThreadMutex_unlock(&manager->exportedWiringEndpointsLock);

		return status;
}

celix_status_t wiringTopologyManager_waModified(void * handle, service_reference_pt reference, void * service) {
	celix_status_t status = CELIX_SUCCESS;

	// Nop...

	return status;
}

celix_status_t wiringTopologyManager_waRemoved(void * handle, service_reference_pt reference, void * service) {
    celix_status_t status = CELIX_SUCCESS;
    wiring_topology_manager_pt manager = handle;

    wiring_admin_service_pt wa = (wiring_admin_service_pt) service;


    status = celixThreadMutex_lock(&manager->exportedWiringEndpointsLock);

    hash_map_iterator_pt iter = hashMapIterator_create(manager->exportedWiringEndpoints);

    while (hashMapIterator_hasNext(iter)) {
        int exportsIter = 0;

        hash_map_entry_pt entry = hashMapIterator_nextEntry(iter);

        service_reference_pt key = hashMapEntry_getKey(entry);
        hash_map_pt exports = hashMapEntry_getValue(entry);

        array_list_pt exports_list = hashMap_get(exports, wa);

        if (exports_list != NULL) {
            for (exportsIter = 0; exportsIter < arrayList_size(exports_list); exportsIter++) {
                wiring_export_registration_pt export = arrayList_get(exports_list, exportsIter);
                wiringTopologyManager_notifyListenersWiringEndpointRemoved(manager, wa, export);
                wa->wiringExportRegistration_close(export);
            }

            arrayList_destroy(exports_list);
            exports_list = NULL;
        }

        hashMap_remove(exports, wa);

        if (hashMap_size(exports) == 0) {
        	hashMap_remove(manager->exportedWiringEndpoints, key);
        	hashMap_destroy(exports, false, false);

            hashMapIterator_destroy(iter);
            iter = hashMapIterator_create(manager->exportedWiringEndpoints);
        }

    }

    hashMapIterator_destroy(iter);

    status = celixThreadMutex_unlock(&manager->exportedWiringEndpointsLock);

    printf("WIRING_TOPOLOGY_MANAGER: Removed WA\n");

    status = celixThreadMutex_lock(&manager->waListLock);
    arrayList_removeElement(manager->waList, wa);
    status = celixThreadMutex_unlock(&manager->waListLock);

    return status;
}

/* Functions for wiring endpoint listener */

celix_status_t wiringTopologyManager_addImportedWiringEndpoint(void *handle, wiring_endpoint_description_pt wEndpoint, char *matchedFilter) {
	celix_status_t status = CELIX_SUCCESS;
	wiring_topology_manager_pt manager = handle;

	printf("WIRING_TOPOLOGY_MANAGER: Add imported wiring endpoint (%s; %s:%u).\n", wEndpoint->frameworkUUID, wEndpoint->url,wEndpoint->port);

	// Create a local copy of the current list of WAs, to ensure we do not run into threading issues...
	array_list_pt localWAs = NULL;
	wiringTopologyManager_getWAs(manager, &localWAs);

	status = celixThreadMutex_lock(&manager->importedWiringEndpointsLock);

	hash_map_pt imports = hashMap_create(NULL, NULL, NULL, NULL);
	hashMap_put(manager->importedWiringEndpoints, wEndpoint,imports);

	int size = arrayList_size(localWAs);
	int iter = 0;
	for (; iter < size; iter++) {
		wiring_admin_service_pt wa = arrayList_get(localWAs, iter);

		wiring_import_registration_pt import = NULL;
		status = wa->importWiringEndpoint(wa->admin, wEndpoint, &import);
		if (status == CELIX_SUCCESS) {
			hashMap_put(imports, wa, import);
		}
	}

	arrayList_destroy(localWAs);

	status = celixThreadMutex_unlock(&manager->importedWiringEndpointsLock);

	return status;
}

celix_status_t wiringTopologyManager_removeImportedWiringEndpoint(void *handle, wiring_endpoint_description_pt wEndpoint, char *matchedFilter) {
	celix_status_t status = CELIX_SUCCESS;
	wiring_topology_manager_pt manager = handle;

	printf("WIRING_TOPOLOGY_MANAGER: Removing imported wiring endpoint (%s; %s:%u).\n", wEndpoint->frameworkUUID, wEndpoint->url,wEndpoint->port);

	status = celixThreadMutex_lock(&manager->importedWiringEndpointsLock);

	hash_map_iterator_pt iter = hashMapIterator_create(manager->importedWiringEndpoints);
	while (hashMapIterator_hasNext(iter)) {
	    hash_map_entry_pt entry = hashMapIterator_nextEntry(iter);
	    wiring_endpoint_description_pt wep = hashMapEntry_getKey(entry);
	    hash_map_pt imports = hashMapEntry_getValue(entry);

	    if ( (strcmp(wEndpoint->url, wep->url) == 0) && (wEndpoint->port==wep->port)) {
	        hash_map_iterator_pt importsIter = hashMapIterator_create(imports);

            while (hashMapIterator_hasNext(importsIter)) {
                hash_map_entry_pt entry = hashMapIterator_nextEntry(importsIter);

                wiring_admin_service_pt wa = hashMapEntry_getKey(entry);
                wiring_import_registration_pt import = hashMapEntry_getValue(entry);

                status = wa->wiringImportRegistration_close(wa->admin, import);
                if (status == CELIX_SUCCESS) {
                    hashMapIterator_remove(importsIter);
                }
            }
            hashMapIterator_destroy(importsIter);

        	hashMapIterator_remove(iter);

        	if (imports != NULL) {
        		hashMap_destroy(imports, false, false);
        	}

	    }
	}
	hashMapIterator_destroy(iter);

	status = celixThreadMutex_unlock(&manager->importedWiringEndpointsLock);

	return status;
}


/*
celix_status_t topologyManager_addExportedService(wiring_topology_manager_pt manager, service_reference_pt reference, char *serviceId) {
	celix_status_t status = CELIX_SUCCESS;

	logHelper_log(manager->loghelper, OSGI_LOGSERVICE_INFO, "TOPOLOGY_MANAGER: Add exported service (%s).", serviceId);

	// Create a local copy of the current list of RSAs, to ensure we do not run into threading issues...
	array_list_pt localRSAs = NULL;
	wiringTopologyManager_getWAs(manager, &localRSAs);

	status = celixThreadMutex_lock(&manager->exportedWiringEndpointsLock);

	hash_map_pt exports = hashMap_create(NULL, NULL, NULL, NULL);
	hashMap_put(manager->exportedWiringEndpoints, reference, exports);

	int size = arrayList_size(localRSAs);
	if (size == 0) {
		logHelper_log(manager->loghelper, OSGI_LOGSERVICE_WARNING, "TOPOLOGY_MANAGER: No RSA available yet.");
	}

	for (int iter = 0; iter < size; iter++) {
		wiring_admin_service_pt rsa = arrayList_get(localRSAs, iter);

		array_list_pt endpoints = NULL;
		status = rsa->exportService(rsa->admin, serviceId, NULL, &endpoints);

		if (status == CELIX_SUCCESS) {
			hashMap_put(exports, rsa, endpoints);
			status = wiringTopologyManager_notifyListenersWiringEndpointAdded(manager, rsa, endpoints);
		}
	}

	arrayList_destroy(localRSAs);

	status = celixThreadMutex_unlock(&manager->exportedWiringEndpointsLock);

	return status;
}

celix_status_t topologyManager_removeExportedService(wiring_topology_manager_pt manager, service_reference_pt reference, char *serviceId) {
	celix_status_t status = CELIX_SUCCESS;

	logHelper_log(manager->loghelper, OSGI_LOGSERVICE_INFO, "TOPOLOGY_MANAGER: Remove exported service (%s).", serviceId);

	status = celixThreadMutex_lock(&manager->exportedWiringEndpointsLock);

	hash_map_pt exports = hashMap_get(manager->exportedWiringEndpoints, reference);
	if (exports) {
		hash_map_iterator_pt iter = hashMapIterator_create(exports);
		while (hashMapIterator_hasNext(iter)) {
			hash_map_entry_pt entry = hashMapIterator_nextEntry(iter);

			wiring_admin_service_pt rsa = hashMapEntry_getKey(entry);
			array_list_pt exportRegistrations = hashMapEntry_getValue(entry);

			for (int exportsIter = 0; exportsIter < arrayList_size(exportRegistrations); exportsIter++) {
				export_registration_pt export = arrayList_get(exportRegistrations, exportsIter);
				wiringTopologyManager_notifyListenersWiringEndpointRemoved(manager, rsa, export);
				rsa->exportRegistration_close(export);
			}
			arrayList_destroy(exportRegistrations);
			exportRegistrations = NULL;

			hashMap_remove(exports, rsa);
			hashMapIterator_destroy(iter);
			iter = hashMapIterator_create(exports);

		}
		hashMapIterator_destroy(iter);
	}

	exports = hashMap_remove(manager->exportedWiringEndpoints, reference);

	if (exports != NULL) {
		hashMap_destroy(exports, false, false);
    }

	status = celixThreadMutex_unlock(&manager->exportedWiringEndpointsLock);

	return status;
}
*/

static celix_status_t wiringTopologyManager_getWiringEndpointDescriptionForWiringExportRegistration(wiring_admin_service_pt wa, wiring_export_registration_pt export, wiring_endpoint_description_pt *endpoint) {
	celix_status_t status = CELIX_SUCCESS;

	wiring_export_reference_pt reference = NULL;
	status = wa->wiringExportRegistration_getWiringExportReference(export, &reference);
	if (status != CELIX_SUCCESS) {
		return status;
	}

	status = wa->wiringExportReference_getExportedWiringEndpoint(reference, endpoint);

	return status;
}


celix_status_t wiringTopologyManager_notifyListenersWiringEndpointAdded(wiring_topology_manager_pt manager, wiring_admin_service_pt wa, array_list_pt registrations) {
	celix_status_t status = CELIX_SUCCESS;

	array_list_pt wiringEndpointListeners = NULL;
	status = bundleContext_getServiceReferences(manager->context, OSGI_WIRING_ENDPOINT_LISTENER_SERVICE, NULL, &wiringEndpointListeners);
	if (status != CELIX_SUCCESS || !wiringEndpointListeners) {
		return CELIX_BUNDLE_EXCEPTION;
	}

	int eplSize = arrayList_size(wiringEndpointListeners);
	int eplIt = 0;
	for (; eplIt < eplSize; eplIt++) {
		service_reference_pt eplRef = arrayList_get(wiringEndpointListeners, eplIt);

		char *scope = NULL;
		serviceReference_getProperty(eplRef, (char *) OSGI_WIRING_ENDPOINT_LISTENER_SCOPE, &scope);

		wiring_endpoint_listener_pt wepl = NULL;
		status = bundleContext_getService(manager->context, eplRef, (void **) &wepl);
		if (status != CELIX_SUCCESS || !wepl) {
			continue;
		}

		filter_pt filter = filter_create(scope);

		int regSize = arrayList_size(registrations);
		int regIt = 0;
		for (; regIt < regSize; regIt++) {
			wiring_export_registration_pt export = arrayList_get(registrations, regIt);

			wiring_endpoint_description_pt wEndpoint = NULL;
			status = wiringTopologyManager_getWiringEndpointDescriptionForWiringExportRegistration(wa, export, &wEndpoint);
			if (status != CELIX_SUCCESS || !wEndpoint) {
				continue;
			}

			bool matchResult = false;
			filter_match(filter, wEndpoint->properties, &matchResult);
			if (matchResult) {
				status = wepl->wiringEndpointAdded(wepl->handle, wEndpoint, scope);
			}
		}

		filter_destroy(filter);
	}

	if (wiringEndpointListeners) {
		arrayList_destroy(wiringEndpointListeners);
	}

	return status;
}

celix_status_t wiringTopologyManager_notifyListenersWiringEndpointRemoved(wiring_topology_manager_pt manager, wiring_admin_service_pt rsa,  wiring_export_registration_pt export) {
	celix_status_t status = CELIX_SUCCESS;

	array_list_pt wiringEndpointListeners = NULL;
	status = bundleContext_getServiceReferences(manager->context, OSGI_WIRING_ENDPOINT_LISTENER_SERVICE, NULL, &wiringEndpointListeners);
	if (status != CELIX_SUCCESS || !wiringEndpointListeners) {
		return CELIX_BUNDLE_EXCEPTION;
	}

	int eplIt = 0;
	for (; eplIt < arrayList_size(wiringEndpointListeners); eplIt++) {
		service_reference_pt eplRef = arrayList_get(wiringEndpointListeners, eplIt);

		wiring_endpoint_listener_pt epl = NULL;
		status = bundleContext_getService(manager->context, eplRef, (void **) &epl);
		if (status != CELIX_SUCCESS || !epl) {
			continue;
		}

		wiring_endpoint_description_pt wEndpoint = NULL;
		status = wiringTopologyManager_getWiringEndpointDescriptionForWiringExportRegistration(rsa, export, &wEndpoint);
		if (status != CELIX_SUCCESS || !wEndpoint) {
			continue;
		}

		status = epl->wiringEndpointRemoved(epl->handle, wEndpoint, NULL);
	}

	if (wiringEndpointListeners) {
		arrayList_destroy(wiringEndpointListeners);
	}

	return status;
}
