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
#include "wiring_topology_manager_impl.h"
#include "wiring_admin.h"
#include "wiring_endpoint_description.h"

struct wiring_topology_manager {
	bundle_context_pt context;

	celix_thread_mutex_t waListLock;
	array_list_pt waList;

	celix_thread_mutex_t installedWiringEndpointsLock;
	hash_map_pt installedWiringEndpoints;

	celix_thread_mutex_t importedWiringEndpointsLock;
	hash_map_pt importedWiringEndpoints;

};

static celix_status_t wiringTopologyManager_notifyListenersWiringEndpointAdded(wiring_topology_manager_pt manager, wiring_endpoint_description_pt wEndpoint);
static celix_status_t wiringTopologyManager_notifyListenersWiringEndpointRemoved(wiring_topology_manager_pt manager, wiring_endpoint_description_pt wEndpoint);


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
	status = celixThreadMutex_create(&(*manager)->installedWiringEndpointsLock, NULL);
	status = celixThreadMutex_create(&(*manager)->importedWiringEndpointsLock, NULL);

	(*manager)->installedWiringEndpoints = hashMap_create(NULL,wiringEndpointDescription_hash, NULL, wiringEndpointDescription_equals);
	(*manager)->importedWiringEndpoints = hashMap_create(wiringEndpointDescription_hash, NULL, wiringEndpointDescription_equals, NULL);

	return status;
}

celix_status_t wiringTopologyManager_destroy(wiring_topology_manager_pt manager) {
	celix_status_t status = CELIX_SUCCESS;

	status = celixThreadMutex_lock(&manager->waListLock);

	arrayList_destroy(manager->waList);

	status = celixThreadMutex_unlock(&manager->waListLock);
	status = celixThreadMutex_destroy(&manager->waListLock);

	status = celixThreadMutex_lock(&manager->installedWiringEndpointsLock);

	hashMap_destroy(manager->installedWiringEndpoints, false, false);

	status = celixThreadMutex_unlock(&manager->installedWiringEndpointsLock);
	status = celixThreadMutex_destroy(&manager->installedWiringEndpointsLock);

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

	//Nop

	return status;
}

celix_status_t wiringTopologyManager_waAdded(void * handle, service_reference_pt reference, void * service) {
	celix_status_t status = CELIX_SUCCESS;
	wiring_topology_manager_pt manager = handle;
	wiring_admin_service_pt wa = (wiring_admin_service_pt) service;

	celixThreadMutex_lock(&manager->waListLock);
	arrayList_add(manager->waList, wa);
	celixThreadMutex_unlock(&manager->waListLock);

	wiring_endpoint_description_pt wEndpoint = NULL;
	status = wa->getWiringEndpoint(wa->admin,&wEndpoint);

	if(status == CELIX_SUCCESS && wEndpoint!=NULL){
		status=wiringTopologyManager_notifyListenersWiringEndpointAdded(manager,wEndpoint);
	}

	printf("WTM: Added WA\n");

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


	wiring_endpoint_description_pt wEndpoint = NULL;
	status = wa->getWiringEndpoint(wa->admin,&wEndpoint);

	if(status == CELIX_SUCCESS && wEndpoint!=NULL){
		status=wiringTopologyManager_notifyListenersWiringEndpointRemoved(manager,wEndpoint);
	}

	celixThreadMutex_lock(&manager->waListLock);
	arrayList_removeElement(manager->waList, wa);
	celixThreadMutex_unlock(&manager->waListLock);

	//TODO: Remove from the installedWEPD hashmap the endpoint, in case an RSA_Inaetics used it

	printf("WTM: Removed WA\n");

	return status;
}

/* Functions for wiring endpoint listener */

celix_status_t wiringTopologyManager_addImportedWiringEndpoint(void *handle, wiring_endpoint_description_pt wEndpoint, char *matchedFilter) {
	celix_status_t status = CELIX_SUCCESS;
	wiring_topology_manager_pt manager = handle;

	printf("WTM: Add imported wiring endpoint (%s; %s:%u).\n", wEndpoint->frameworkUUID, wEndpoint->url,wEndpoint->port);

	// Create a local copy of the current list of WAs, to ensure we do not run into threading issues...
	array_list_pt localWAs = NULL;
	wiringTopologyManager_getWAs(manager, &localWAs);

	status = celixThreadMutex_lock(&manager->importedWiringEndpointsLock);

	hashMap_put(manager->importedWiringEndpoints, wEndpoint,NULL);

	int size = arrayList_size(localWAs);
	int iter = 0;
	for (; iter < size; iter++) {
		wiring_admin_service_pt wa = arrayList_get(localWAs, iter);

		status = wa->importWiringEndpoint(wa->admin, wEndpoint);

	}

	arrayList_destroy(localWAs);

	status = celixThreadMutex_unlock(&manager->importedWiringEndpointsLock);

	return status;
}

celix_status_t wiringTopologyManager_removeImportedWiringEndpoint(void *handle, wiring_endpoint_description_pt wEndpoint, char *matchedFilter) {
	celix_status_t status = CELIX_SUCCESS;
	wiring_topology_manager_pt manager = handle;

	printf("WTM: Removing imported wiring endpoint (%s; %s:%u).\n", wEndpoint->frameworkUUID, wEndpoint->url,wEndpoint->port);

	status = celixThreadMutex_lock(&manager->importedWiringEndpointsLock);

	wiring_endpoint_description_pt wepd = (wiring_endpoint_description_pt)hashMap_remove(manager->importedWiringEndpoints,wEndpoint);

	status = celixThreadMutex_unlock(&manager->importedWiringEndpointsLock);
	if(wepd!=NULL){
		wiringEndpointDescription_destroy(wepd);
	}

	return status;
}

celix_status_t wiringTopologyManager_installCallbackToWiringEndpoint(wiring_topology_manager_pt manager, properties_pt properties, celix_status_t(*rsa_inaetics_cb)(char* data, char**response)){

	celix_status_t status = CELIX_SUCCESS;


	celixThreadMutex_lock(&manager->installedWiringEndpointsLock);

	wiring_endpoint_description_pt wepd = hashMap_get(manager->installedWiringEndpoints,rsa_inaetics_cb);

	if( wepd == NULL ){
		array_list_pt localWAs = NULL;
		wiringTopologyManager_getWAs(manager, &localWAs);
		int i=0;
		for(;i<arrayList_size(localWAs);i++){
			wiring_admin_service_pt wa = (wiring_admin_service_pt)arrayList_get(localWAs,i);
			wiring_endpoint_description_pt wEndpoint = NULL;
			status = wa->getWiringEndpoint(wa->admin,&wEndpoint);
			if(status == CELIX_SUCCESS && wEndpoint!=NULL){
				//TODO: Match the passed properties with the WEPD properties
			}

		}

		arrayList_destroy(localWAs);
	}
	else{
		printf("WTM: The passed callback is already installed on WiringEndpoint %s:%u\n",wepd->url,wepd->port);
		status=CELIX_ILLEGAL_STATE;
	}


	celixThreadMutex_unlock(&manager->installedWiringEndpointsLock);


	return status;
}

celix_status_t wiringTopologyManager_uninstallCallbackFromWiringEndpoint(wiring_topology_manager_pt manager, celix_status_t(*rsa_inaetics_cb)(char* data, char**response)){
	celix_status_t status = CELIX_SUCCESS;
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

static celix_status_t wiringTopologyManager_notifyListenersWiringEndpointAdded(wiring_topology_manager_pt manager, wiring_endpoint_description_pt wEndpoint) {
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

		bool matchResult = false;
		filter_match(filter, wEndpoint->properties, &matchResult);
		if (matchResult) {
			status = wepl->wiringEndpointAdded(wepl->handle, wEndpoint, scope);
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

		status = epl->wiringEndpointRemoved(epl->handle, wEndpoint, NULL);
	}

	if (wiringEndpointListeners) {
		arrayList_destroy(wiringEndpointListeners);
	}

	return status;
}
