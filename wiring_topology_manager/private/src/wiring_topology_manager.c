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

	celix_thread_mutex_t handleToWiringAdminLock;
	hash_map_pt handleToWiringAdmin;

};

typedef struct wiring_endpoint_registration{
	wiring_endpoint_description_pt wiringEndpointDescription;
	wiring_admin_service_pt wiringAdminService;
} * wiring_endpoint_registration_pt;

static celix_status_t wiringTopologyManager_notifyListenersWiringEndpointAdded(wiring_topology_manager_pt manager, wiring_endpoint_description_pt wEndpoint);
static celix_status_t wiringTopologyManager_notifyListenersWiringEndpointRemoved(wiring_topology_manager_pt manager, wiring_endpoint_description_pt wEndpoint);

static bool properties_match(properties_pt properties,properties_pt reference);


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
	celixThreadMutex_create(&((*manager)->installedWiringEndpointsLock), NULL);
	celixThreadMutex_create(&((*manager)->importedWiringEndpointsLock), NULL);
	celixThreadMutex_create(&((*manager)->handleToWiringAdminLock), NULL);


	(*manager)->installedWiringEndpoints = hashMap_create(NULL,NULL, NULL, NULL); // key=rsa_inaetics_callback_fp, value=wiring_endpoint_registration_pt
	(*manager)->handleToWiringAdmin = hashMap_create(NULL,NULL, NULL, NULL); // key=handle, value=wiring_admin_service_pt
	(*manager)->importedWiringEndpoints = hashMap_create(wiringEndpointDescription_hash, NULL, wiringEndpointDescription_equals, NULL); // key=wiring_endpoint_description_pt, value=wiring_admin_service_pt

	return status;
}

celix_status_t wiringTopologyManager_destroy(wiring_topology_manager_pt manager) {
	celix_status_t status = CELIX_SUCCESS;

	celixThreadMutex_lock(&manager->waListLock);

	arrayList_destroy(manager->waList);

	celixThreadMutex_unlock(&manager->waListLock);
	celixThreadMutex_destroy(&manager->waListLock);

	celixThreadMutex_lock(&manager->installedWiringEndpointsLock);

	hashMap_destroy(manager->installedWiringEndpoints, false, true);

	celixThreadMutex_unlock(&manager->installedWiringEndpointsLock);
	celixThreadMutex_destroy(&manager->installedWiringEndpointsLock);

	celixThreadMutex_lock(&manager->importedWiringEndpointsLock);

	hashMap_destroy(manager->importedWiringEndpoints, false, false);

	celixThreadMutex_unlock(&manager->importedWiringEndpointsLock);
	celixThreadMutex_destroy(&manager->importedWiringEndpointsLock);

	celixThreadMutex_lock(&manager->handleToWiringAdminLock);

	hashMap_destroy(manager->handleToWiringAdmin, false, false);

	celixThreadMutex_unlock(&manager->handleToWiringAdminLock);
	celixThreadMutex_destroy(&manager->handleToWiringAdminLock);


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

	status=bundleContext_getService(manager->context, reference, service);

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

		/* Check if the added WA can match one of the imported WiringEndpoints */

		celixThreadMutex_lock(&manager->importedWiringEndpointsLock);
		hash_map_iterator_pt iter = hashMapIterator_create(manager->importedWiringEndpoints);
		while(hashMapIterator_hasNext(iter)){
			hash_map_entry_pt entry = hashMapIterator_nextEntry(iter);
			wiring_endpoint_description_pt i_wepd = hashMapEntry_getKey(entry);
			wiring_admin_service_pt i_wa = hashMapEntry_getValue(entry);

			/* If we find a proper imported WiringEndpoint but still no WiringProxy is associated */
			if(properties_match(wEndpoint->properties,i_wepd->properties) && (i_wa==NULL)){
				hashMap_put(manager->importedWiringEndpoints,i_wepd,wa);
				printf("WTM: Added WiringAdmin is able to communicate with imported WiringEndpoint %s\n",i_wepd->url);
			}
		}
		hashMapIterator_destroy(iter);
	}
	celixThreadMutex_unlock(&manager->importedWiringEndpointsLock);

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

		/* Check if the removed WA had an installed callback, if yes remove the registration from the installedWiringEndpoints hashmap*/

		celixThreadMutex_lock(&manager->installedWiringEndpointsLock);

		hash_map_iterator_pt iter= hashMapIterator_create(manager->installedWiringEndpoints);

		while(hashMapIterator_hasNext(iter)){
			wiring_endpoint_registration_pt reg = hashMapIterator_nextValue(iter);
			if( (reg !=NULL) && (wiringEndpointDescription_equals(wEndpoint,reg->wiringEndpointDescription)==0) ){
				/*We had a registration for the WiringEndpoint associated to the removed WA */
				hashMapIterator_remove(iter);
				free(reg);
			}
		}

		hashMapIterator_destroy(iter);

		/* Check if the removed WA was acting as a proxy for some imported WiringEndpoint*/

		celixThreadMutex_unlock(&manager->installedWiringEndpointsLock);


		celixThreadMutex_lock(&manager->importedWiringEndpointsLock);

		iter = hashMapIterator_create(manager->importedWiringEndpoints);
		while(hashMapIterator_hasNext(iter)){
			hash_map_entry_pt entry = hashMapIterator_nextEntry(iter);
			wiring_endpoint_description_pt i_wepd = hashMapEntry_getKey(entry);
			wiring_admin_service_pt i_wa = hashMapEntry_getValue(entry);

			/* If we find a proper imported WiringEndpoint associated no WiringProxy is associated */
			if(wa==i_wa){
				printf("WTM: The removed WA was associated as a WiringProxy for imported WiringEndpoint %s: deassociating them...\n",i_wepd->url);
				hashMap_put(manager->importedWiringEndpoints,i_wepd,NULL);
			}

			//TODO: Inform RSA_Inaetics using that proxy that it's not available anymore
		}

		hashMapIterator_destroy(iter);

		celixThreadMutex_unlock(&manager->importedWiringEndpointsLock);

		celixThreadMutex_lock(&manager->handleToWiringAdminLock);

		iter = hashMapIterator_create(manager->handleToWiringAdmin);
		while(hashMapIterator_hasNext(iter)){
			wiring_admin_service_pt i_wa = hashMapIterator_nextValue(iter);

			if(wa==i_wa){
				hashMapIterator_remove(iter);
			}

		}

		hashMapIterator_destroy(iter);

		celixThreadMutex_unlock(&manager->handleToWiringAdminLock);

	}


	celixThreadMutex_lock(&manager->waListLock);
	arrayList_removeElement(manager->waList, wa);
	celixThreadMutex_unlock(&manager->waListLock);


	printf("WTM: Removed WA\n");

	return status;
}

/* Functions for wiring endpoint listener */

celix_status_t wiringTopologyManager_addImportedWiringEndpoint(void *handle, wiring_endpoint_description_pt wEndpoint, char *matchedFilter) {
	celix_status_t status = CELIX_SUCCESS;
	wiring_topology_manager_pt manager = handle;

	printf("WTM: Add imported wiring endpoint (%s; %s).\n", wEndpoint->frameworkUUID, wEndpoint->url);

	// Create a local copy of the current list of WAs, to ensure we do not run into threading issues...
	array_list_pt localWAs = NULL;
	wiringTopologyManager_getWAs(manager, &localWAs);

	status = celixThreadMutex_lock(&manager->importedWiringEndpointsLock);

	wiring_admin_service_pt matching_wa = NULL;

	int size = arrayList_size(localWAs);
	int iter = 0;
	for (; iter < size; iter++) {
		wiring_admin_service_pt wa = arrayList_get(localWAs, iter);

		wiring_endpoint_description_pt wa_wepd = NULL;
		wa->getWiringEndpoint(wa->admin,&wa_wepd);

		/*
		if(wa_wepd!=NULL){
			wiringEndpointDescription_dump(wa_wepd);
		}

		if(wEndpoint!=NULL){
			wiringEndpointDescription_dump(wEndpoint);
		}
		*/

		/* Check if we already have a WiringAdmin able to communicate with the imported wiring endpoint */

		if((wa_wepd!=NULL) && (properties_match(wa_wepd->properties,wEndpoint->properties)==true)){
			matching_wa = wa;
			printf("WTM: Imported WiringEndpoint matches with local WA %s. Associating them...\n",wa_wepd->url);
			break; //Found, we have a proper WA
		}

	}

	hashMap_put(manager->importedWiringEndpoints, wEndpoint,matching_wa);

	arrayList_destroy(localWAs);

	status = celixThreadMutex_unlock(&manager->importedWiringEndpointsLock);

	return status;
}

celix_status_t wiringTopologyManager_removeImportedWiringEndpoint(void *handle, wiring_endpoint_description_pt wEndpoint, char *matchedFilter) {
	celix_status_t status = CELIX_SUCCESS;
	wiring_topology_manager_pt manager = handle;



	status = celixThreadMutex_lock(&manager->importedWiringEndpointsLock);

	if(hashMap_remove(manager->importedWiringEndpoints,wEndpoint)!=NULL){
		printf("WTM: Removing imported wiring endpoint (%s; %s).\n", wEndpoint->frameworkUUID, wEndpoint->url);
	}

	status = celixThreadMutex_unlock(&manager->importedWiringEndpointsLock);

	//TODO: Notify RSA_Inaetics communicating with the removed imported WiringEndpoint

	return status;
}

/* Return true if all pairs in properties are contained in reference */
static bool properties_match(properties_pt properties,properties_pt reference){

	bool matching=true;

	hash_map_iterator_pt iter = hashMapIterator_create(properties);
	while(hashMapIterator_hasNext(iter) && matching){
		hash_map_entry_pt prop_pair=hashMapIterator_nextEntry(iter);
		char* prop_key=(char*)hashMapEntry_getKey(prop_pair);
		char* prop_value=(char*)hashMapEntry_getValue(prop_pair);

		/*OSGI_RSA_ENDPOINT_FRAMEWORK_UUID (endpoint.framework.uuid) is a special property that shouldn't be taken in account*/
		if(strcmp(prop_key,OSGI_RSA_ENDPOINT_FRAMEWORK_UUID)==0){
			continue;
		}

		char* ref_value=(char*)hashMap_get(reference,prop_key);
		if( ref_value==NULL || (strcmp(ref_value,prop_value)!=0)){
			matching=false; //We found a pair in properties not included in reference
		}
	}
	hashMapIterator_destroy(iter);

	return matching;

}

celix_status_t wiringTopologyManager_installCallbackToWiringEndpoint(wiring_topology_manager_pt manager, properties_pt properties, rsa_inaetics_receive_cb rsa_inaetics_cb){

	celix_status_t status = CELIX_BUNDLE_EXCEPTION;

	if(properties==NULL || rsa_inaetics_cb==NULL){
		return CELIX_ILLEGAL_ARGUMENT;
	}

	celixThreadMutex_lock(&manager->installedWiringEndpointsLock);

	wiring_endpoint_registration_pt wepr = hashMap_get(manager->installedWiringEndpoints,rsa_inaetics_cb);

	if( wepr == NULL ){
		array_list_pt localWAs = NULL;
		wiringTopologyManager_getWAs(manager, &localWAs);
		int i=0;
		for(;i<arrayList_size(localWAs);i++){
			wiring_admin_service_pt wa = (wiring_admin_service_pt)arrayList_get(localWAs,i);
			wiring_endpoint_description_pt wEndpoint = NULL;
			wa->getWiringEndpoint(wa->admin,&wEndpoint);
			if(wEndpoint!=NULL){
				if(properties_match(properties,wEndpoint->properties) == true){ //We found our Wiring Endpoint, let's try to install the callback.
					status=wa->exportWiringEndpoint(wa->admin,rsa_inaetics_cb);
					if(status==CELIX_SUCCESS){
						wiring_endpoint_registration_pt reg = calloc(1,sizeof(struct wiring_endpoint_registration));
						reg->wiringEndpointDescription=wEndpoint;
						reg->wiringAdminService = wa;
						hashMap_put(manager->installedWiringEndpoints,rsa_inaetics_cb,reg);
						printf("WTM: Installed callback to matching Wiring Endpoint %s\n",wEndpoint->url);
						break;
					}
					else{
						printf("WTM: Could not callback to matching Wiring Endpoint %s. Going on...\n",wEndpoint->url);
					}

				}
			}
		}

		arrayList_destroy(localWAs);
	}
	else{
		printf("WTM: The passed callback is already installed on WiringEndpoint %s\n",wepr->wiringEndpointDescription->url);
		status=CELIX_ILLEGAL_STATE;
	}

	celixThreadMutex_unlock(&manager->installedWiringEndpointsLock);

	if(status!=CELIX_SUCCESS){
		printf("WTM: Could not install callback to any Wiring Endpoint\n");
	}

	return status;
}

celix_status_t wiringTopologyManager_uninstallCallbackFromWiringEndpoint(wiring_topology_manager_pt manager, rsa_inaetics_receive_cb rsa_inaetics_cb){

	celix_status_t status = CELIX_BUNDLE_EXCEPTION;

	if(rsa_inaetics_cb==NULL){
		return CELIX_ILLEGAL_ARGUMENT;
	}

	celixThreadMutex_lock(&manager->installedWiringEndpointsLock);

	wiring_endpoint_registration_pt wepr = hashMap_get(manager->installedWiringEndpoints,rsa_inaetics_cb);

	if( wepr != NULL ){
		status=wepr->wiringAdminService->removeExportedWiringEndpoint(wepr->wiringAdminService->admin,rsa_inaetics_cb);
		if(status==CELIX_SUCCESS){
			hashMap_remove(manager->installedWiringEndpoints,rsa_inaetics_cb);
			printf("WTM: Uninstalled callback from Wiring Endpoint %s\n",wepr->wiringEndpointDescription->url);
			free(wepr);
		}
	}
	else{
		printf("WTM: The passed callback was never installed on any WiringEndpoint \n");
		status=CELIX_ILLEGAL_STATE;
	}

	celixThreadMutex_unlock(&manager->installedWiringEndpointsLock);


	return status;
}

celix_status_t wiringTopologyManager_getWiringProxy(wiring_topology_manager_pt manager,properties_pt properties, wiring_admin_pt* admin, rsa_inaetics_send* sendFunc, wiring_handle* handle){
	celix_status_t status = CELIX_ILLEGAL_STATE;

	if(properties==NULL){
		return CELIX_ILLEGAL_ARGUMENT;
	}

	char* url=hashMap_get(properties,WIRING_ENDPOINT_DESCRIPTION_URL_KEY);

	if(url==NULL){
		printf("WTM: No URL specified. Cannot look for a proper WiringProxy.\n");
		return CELIX_ILLEGAL_ARGUMENT;
	}

	celixThreadMutex_lock(&manager->importedWiringEndpointsLock);
	hash_map_iterator_pt iter = hashMapIterator_create(manager->importedWiringEndpoints);
	while(hashMapIterator_hasNext(iter)){
		wiring_endpoint_description_pt wepd = hashMapIterator_nextKey(iter);
		wiring_admin_service_pt wa = hashMapIterator_nextValue(iter);
		/* If we are aware of such a node asked by RSA_Inaetics... */
		if(strcmp(wepd->url,url) == 0){
			/* ... and we have a valid WiringProxy */
			if(wa!=NULL){
				status=wa->importWiringEndpoint(wa->admin,wepd,sendFunc,handle);
				if( (status==CELIX_SUCCESS) && (*handle)!=NULL){
					*admin=wa->admin;
					celixThreadMutex_lock(&manager->handleToWiringAdminLock);
					hashMap_put(manager->handleToWiringAdmin,*handle,wa);
					celixThreadMutex_unlock(&manager->handleToWiringAdminLock);
				}
			}
			else{
				printf("WTM: Found asked WiringEndpoint, but no WiringProxy is available for communication.\n");
			}
		}

	}

	hashMapIterator_destroy(iter);

	celixThreadMutex_unlock(&manager->importedWiringEndpointsLock);

	if(status!=CELIX_SUCCESS){
		printf("WTM: Cannot connect to the asked WiringEndpoint.\n");
	}


	return status;
}


celix_status_t wiringTopologyManager_removeWiringProxy(wiring_topology_manager_pt manager, wiring_handle handle){
	celix_status_t status = CELIX_SUCCESS;

	celixThreadMutex_lock(&manager->handleToWiringAdminLock);
	wiring_admin_service_pt wa = hashMap_get(manager->handleToWiringAdmin,handle);
	if(wa!=NULL){
		status = wa->removeImportedWiringEndpoint(wa->admin,handle);
		hashMap_remove(manager->handleToWiringAdmin,handle);
	}
	celixThreadMutex_unlock(&manager->handleToWiringAdminLock);


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
	status = bundleContext_getServiceReferences(manager->context, INAETICS_WIRING_ENDPOINT_LISTENER_SERVICE, NULL, &wiringEndpointListeners);
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
