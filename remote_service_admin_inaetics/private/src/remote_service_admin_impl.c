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
 * remote_service_admin_impl.c
 *
 *  \date       Sep 30, 2011
 *  \author    	<a href="mailto:dev@celix.apache.org">Apache Celix Project Team</a>
 *  \copyright	Apache License, Version 2.0
 */
#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <uuid/uuid.h>

#include <curl/curl.h>

#include "remote_service_admin_inaetics.h"
#include "remote_service_admin_inaetics_impl.h"
#include "export_registration_impl.h"
#include "import_registration_impl.h"
#include "remote_constants.h"
#include "constants.h"
#include "utils.h"
#include "bundle_context.h"
#include "bundle.h"
#include "service_reference.h"
#include "service_registration.h"
#include "log_helper.h"
#include "log_service.h"
#include "celix_threads.h"

#include "wiring_topology_manager.h"
#include "wiring_endpoint_listener.h"
#include "wiring_endpoint_description.h"
#include "wiring_admin.h"

static const char * const CONFIGURATION_TYPE = "org.inaetics.remote.admin.wiring";

static celix_status_t remoteServiceAdmin_registerReceive(remote_service_admin_pt admin, char* wireId);
static celix_status_t remoteServiceAdmin_unregisterReceive(remote_service_admin_pt admin, char* wireId);

static celix_status_t remoteServiceAdmin_createSendServiceTracker(remote_service_admin_pt admin);
static celix_status_t remoteServiceAdmin_destroySendServiceTracker(remote_service_admin_pt admin);

static celix_status_t remoteServiceAdmin_sendServiceAdding(void * handle, service_reference_pt reference, void **service);
static celix_status_t remoteServiceAdmin_sendServiceAdded(void * handle, service_reference_pt reference, void * service);
static celix_status_t remoteServiceAdmin_sendServiceModified(void * handle, service_reference_pt reference, void * service);
static celix_status_t remoteServiceAdmin_sendServiceRemoved(void * handle, service_reference_pt reference, void * service);

celix_status_t remoteServiceAdmin_installEndpoint(remote_service_admin_pt admin, export_registration_pt registration, service_reference_pt reference, char *interface);
celix_status_t remoteServiceAdmin_createEndpointDescription(remote_service_admin_pt admin, service_reference_pt reference, properties_pt endpointProperties, char *interface, endpoint_description_pt *description);

celix_status_t remoteServiceAdmin_create(bundle_context_pt context, remote_service_admin_pt *admin) {
	celix_status_t status = CELIX_SUCCESS;

	*admin = calloc(1, sizeof(**admin));

	if (!*admin) {
		status = CELIX_ENOMEM;
	} else {
		(*admin)->context = context;
		(*admin)->exportedServices = hashMap_create(NULL, NULL, NULL, NULL);
		(*admin)->importedServices = hashMap_create(NULL, NULL, NULL, NULL);
		(*admin)->wiringReceiveServices = hashMap_create(utils_stringHash, NULL, utils_stringEquals, NULL);
		(*admin)->wiringReceiveServiceRegistrations = hashMap_create(utils_stringHash, NULL, utils_stringEquals, NULL);
		(*admin)->sendServicesTracker = NULL;
		(*admin)->sendServices = hashMap_create(utils_stringHash, NULL, utils_stringEquals, NULL);

		celixThreadMutex_create(&(*admin)->exportedServicesLock, NULL);
		celixThreadMutex_create(&(*admin)->importedServicesLock, NULL);

		if (logHelper_create(context, &(*admin)->loghelper) == CELIX_SUCCESS) {
			logHelper_start((*admin)->loghelper);
		}

		status = remoteServiceAdmin_createSendServiceTracker(*admin);

	}
	return status;
}

celix_status_t remoteServiceAdmin_destroy(remote_service_admin_pt *admin) {
	celix_status_t status = CELIX_SUCCESS;

	hashMap_destroy((*admin)->wiringReceiveServices, false, false);
	hashMap_destroy((*admin)->wiringReceiveServiceRegistrations, false, false);
	hashMap_destroy((*admin)->sendServices, false, false);

	celixThreadMutex_destroy(&(*admin)->exportedServicesLock);
	celixThreadMutex_destroy(&(*admin)->importedServicesLock);

	free(*admin);

	*admin = NULL;

	return status;
}

celix_status_t remoteServiceAdmin_stop(remote_service_admin_pt admin) {
	celix_status_t status = CELIX_SUCCESS;

	celixThreadMutex_lock(&admin->exportedServicesLock);

	hash_map_iterator_pt iter = hashMapIterator_create(admin->exportedServices);
	while (hashMapIterator_hasNext(iter)) {
		array_list_pt exports = hashMapIterator_nextValue(iter);
		int i;
		for (i = 0; i < arrayList_size(exports); i++) {
			export_registration_pt export = arrayList_get(exports, i);
			exportRegistration_stopTracking(export);
		}
	}
	hashMapIterator_destroy(iter);
	celixThreadMutex_unlock(&admin->exportedServicesLock);

	celixThreadMutex_lock(&admin->importedServicesLock);

	iter = hashMapIterator_create(admin->importedServices);
	while (hashMapIterator_hasNext(iter)) {
		hash_map_entry_pt entry = hashMapIterator_nextEntry(iter);

		import_registration_factory_pt importFactory = hashMapEntry_getValue(entry);

		if (importFactory != NULL) {
			int i;
			for (i = 0; i < arrayList_size(importFactory->registrations); i++) {
				import_registration_pt importRegistration = arrayList_get(importFactory->registrations, i);

				if (importFactory->trackedFactory != NULL) {
					importFactory->trackedFactory->unregisterProxyService(importFactory->trackedFactory->factory, importRegistration->endpointDescription);
				}
			}

			serviceTracker_close(importFactory->proxyFactoryTracker);
			importRegistrationFactory_close(importFactory);

			hashMapIterator_remove(iter);
			importRegistrationFactory_destroy(&importFactory);
		}
	}
	hashMapIterator_destroy(iter);
	celixThreadMutex_unlock(&admin->importedServicesLock);

	hashMap_destroy(admin->exportedServices, false, false);
	hashMap_destroy(admin->importedServices, false, false);

	status = remoteServiceAdmin_destroySendServiceTracker(admin);

	if (logHelper_stop(admin->loghelper) == CELIX_SUCCESS) {
		logHelper_destroy(&admin->loghelper);
	}

	return status;
}

static celix_status_t remoteServiceAdmin_receive(void* handle, char* data, char**response) {
	celix_status_t status = CELIX_ILLEGAL_ARGUMENT;
	remote_service_admin_pt admin = (remote_service_admin_pt) handle;

	//celixThreadMutex_lock(&admin->exportedServicesLock);
	json_error_t jsonError;
	json_t* root;

	printf("RSA: data received: %s\n", data);

	root = json_loads(data, 0, &jsonError);

	if (root) {
		int serviceId = -1;
		char* request = NULL;
		json_t* json_request = NULL;

		json_unpack(root, "{s:i, s:o}", "service.id", &serviceId, "request", &json_request);
		request = json_dumps(json_request, 0);

		hash_map_iterator_pt iter = hashMapIterator_create(admin->exportedServices);
		while (hashMapIterator_hasNext(iter)) {
			hash_map_entry_pt entry = hashMapIterator_nextEntry(iter);
			array_list_pt exports = hashMapEntry_getValue(entry);

			int expIt = 0;
			for (expIt = 0; (expIt < arrayList_size(exports)) && (status != CELIX_SUCCESS); expIt++) {
				export_registration_pt export = arrayList_get(exports, expIt);

				if (serviceId == export->endpointDescription->serviceId) {
					export->endpoint->handleRequest(export->endpoint->endpoint, request, response);
					status = CELIX_SUCCESS;
				}
			}

		}

		json_decref(root);

		//celixThreadMutex_unlock(&admin->exportedServicesLock);

		hashMapIterator_destroy(iter);
	}
	return status;
}

/* Functions for wiring endpoint listener */
celix_status_t remoteServiceAdmin_addImportedWiringEndpoint(void *handle, wiring_endpoint_description_pt wEndpoint, char *matchedFilter) {
	celix_status_t status = CELIX_SUCCESS;

	wiring_endpoint_listener_pt listener = (wiring_endpoint_listener_pt) handle;
	remote_service_admin_pt admin = (remote_service_admin_pt) listener->handle;

	char* wireId = properties_get(wEndpoint->properties, (char*) WIRING_ENDPOINT_DESCRIPTION_WIRE_ID_KEY);
	char* wireServiceId = properties_get(wEndpoint->properties, (char*) OSGI_FRAMEWORK_SERVICE_ID);

	printf("RSA: exportCommand_addImportedWiringEndpoint w/ wireId %s \n", wireId);

	status = remoteServiceAdmin_registerReceive(admin, wireId);

	if (status == CELIX_SUCCESS) {

		hash_map_iterator_pt iter = hashMapIterator_create(admin->exportedServices);
		while (hashMapIterator_hasNext(iter)) {
			array_list_pt exports = hashMapIterator_nextValue(iter);
			int i;
			for (i = 0; i < arrayList_size(exports); i++) {
				export_registration_pt export = arrayList_get(exports, i);
				export_reference_pt reference = NULL;
				endpoint_description_pt endpoint = NULL;

				exportRegistration_getExportReference(export, &reference);
				exportReference_getExportedEndpoint(reference, &endpoint);

				char* exportServiceId = properties_get(endpoint->properties, (char*) OSGI_RSA_ENDPOINT_SERVICE_ID);

				if (exportServiceId == NULL) {
					printf("RSA: exportCommand_addImportedWiringEndpoint no service Id found\n");
				} else if (strcmp(exportServiceId, wireServiceId) == 0) {
					printf("RSA: exportCommand_addImportedWiringEndpoint service Id %s found\n", wireServiceId);
					properties_set(endpoint->properties, WIRING_ENDPOINT_DESCRIPTION_WIRE_ID_KEY, wireId);
				}
				free(reference);
			}
		}
		hashMapIterator_destroy(iter);
	} else {
		printf("ECHO_SERVER: Registration of Receive Service failed\n");
	}

	return status;
}

celix_status_t remoteServiceAdmin_removeImportedWiringEndpoint(void *handle, wiring_endpoint_description_pt wEndpoint, char *matchedFilter) {
	celix_status_t status = CELIX_SUCCESS;

	wiring_endpoint_listener_pt listener = (wiring_endpoint_listener_pt) handle;
	remote_service_admin_pt admin = (remote_service_admin_pt) listener->handle;
	char* wireId = properties_get(wEndpoint->properties, WIRING_ENDPOINT_DESCRIPTION_WIRE_ID_KEY);

	status = remoteServiceAdmin_unregisterReceive(admin, wireId);

	return status;
}

static celix_status_t remoteServiceAdmin_registerReceive(remote_service_admin_pt admin, char* wireId) {
	celix_status_t status = CELIX_SUCCESS;

	printf("RSA: registerReceive w/ wireId %s\n", wireId);

	wiring_receive_service_pt wiringReceiveService = calloc(1, sizeof(*wiringReceiveService));

	if (!wiringReceiveService) {
		status = CELIX_ENOMEM;
	} else {
		properties_pt props = properties_create();
		service_registration_pt wiringReceiveServiceRegistration = NULL;

		properties_set(props, (char*) INAETICS_WIRING_WIRE_ID, wireId);

		wiringReceiveService->handle = admin;
		wiringReceiveService->receive = remoteServiceAdmin_receive;
		wiringReceiveService->wireId = wireId;

		status = bundleContext_registerService(admin->context, (char *) INAETICS_WIRING_RECEIVE_SERVICE, wiringReceiveService, props, &wiringReceiveServiceRegistration);

		if (status == CELIX_SUCCESS) {
			hashMap_put(admin->wiringReceiveServices, wireId, wiringReceiveService);
			hashMap_put(admin->wiringReceiveServiceRegistrations, wireId, wiringReceiveServiceRegistration);

		} else {
			free(wiringReceiveService);
		}
	}

	return status;
}

static celix_status_t remoteServiceAdmin_unregisterReceive(remote_service_admin_pt admin, char* wireId) {
	celix_status_t status = CELIX_SUCCESS;

	printf("RSA: unregisterReceive w/ wireId %s\n", wireId);

	service_registration_pt wiringReceiveServiceRegistration = hashMap_remove(admin->wiringReceiveServiceRegistrations, wireId);
	wiring_receive_service_pt wiringReceiveService = hashMap_remove(admin->wiringReceiveServices, wireId);

	if (wiringReceiveServiceRegistration != NULL) {
		char* serviceWireId = NULL;
		array_list_pt references = NULL;
		int i;

		serviceRegistration_getServiceReferences(wiringReceiveServiceRegistration, &references);

		for (i = 0; i < arrayList_size(references); i++) {
			service_reference_pt reference = (service_reference_pt) arrayList_get(references, i);
			serviceReference_getProperty(reference, (char*) INAETICS_WIRING_WIRE_ID, &serviceWireId);

			if (strcmp(wireId, serviceWireId) == 0) {
				status = serviceRegistration_unregister(wiringReceiveServiceRegistration);
			}
		}
	}

	if (wiringReceiveService != NULL) {
		free(wiringReceiveService);
	}

	return status;
}

celix_status_t remoteServiceAdmin_exportService(remote_service_admin_pt admin, char *serviceId, properties_pt properties, array_list_pt *registrations) {
	celix_status_t status = CELIX_SUCCESS;
	arrayList_create(registrations);
	array_list_pt references = NULL;
	service_reference_pt reference = NULL;
	char filter[256];

	snprintf(filter, 256, "(%s=%s)", (char *) OSGI_FRAMEWORK_SERVICE_ID, serviceId);

	bundleContext_getServiceReferences(admin->context, NULL, filter, &references);

	if (arrayList_size(references) >= 1) {
		reference = arrayList_get(references, 0);
	}

	if (references != NULL) {
		arrayList_destroy(references);
	}

	if (reference == NULL) {
		logHelper_log(admin->loghelper, OSGI_LOGSERVICE_ERROR, "ERROR: expected a reference for service id %s.", serviceId);
		return CELIX_ILLEGAL_STATE;
	}

	char *exports = NULL;
	char *provided = NULL;
	serviceReference_getProperty(reference, (char *) OSGI_RSA_SERVICE_EXPORTED_INTERFACES, &exports);
	serviceReference_getProperty(reference, (char *) OSGI_FRAMEWORK_OBJECTCLASS, &provided);

	if (exports == NULL || provided == NULL) {
		logHelper_log(admin->loghelper, OSGI_LOGSERVICE_WARNING, "RSA: No Services to export.");
	} else {

		service_reference_pt wtmServiceRef = NULL;
		array_list_pt interfaces = NULL;

		logHelper_log(admin->loghelper, OSGI_LOGSERVICE_INFO, "RSA: Export services (%s)", exports);
		arrayList_create(&interfaces);
		if (strcmp(utils_stringTrim(exports), "*") == 0) {
			char *save_ptr = NULL;
			char *interface = strtok_r(provided, ",", &save_ptr);
			while (interface != NULL) {
				arrayList_add(interfaces, utils_stringTrim(interface));
				interface = strtok_r(NULL, ",", &save_ptr);
			}
		} else {
			char *provided_save_ptr = NULL;
			char *pinterface = strtok_r(provided, ",", &provided_save_ptr);
			while (pinterface != NULL) {
				char *exports_save_ptr = NULL;
				char *einterface = strtok_r(exports, ",", &exports_save_ptr);
				while (einterface != NULL) {
					if (strcmp(einterface, pinterface) == 0) {
						arrayList_add(interfaces, einterface);
					}
					einterface = strtok_r(NULL, ",", &exports_save_ptr);
				}
				pinterface = strtok_r(NULL, ",", &provided_save_ptr);
			}
		}

		status = bundleContext_getServiceReference(admin->context, (char *) INAETICS_WIRING_TOPOLOGY_MANAGER_SERVICE, &wtmServiceRef);

		if (status == CELIX_SUCCESS && wtmServiceRef != NULL) {
			void* service = NULL;
			wiring_topology_manager_service_pt wtmService = NULL;

			if (bundleContext_getService(admin->context, wtmServiceRef, &service) == CELIX_SUCCESS) {
				wtmService = (wiring_topology_manager_service_pt) service;
			}

			// if the dereferenced instance is null then we know the service has been removed
			if (wtmService != NULL) {
				if (arrayList_size(interfaces) != 0) {
					int iter = 0;
					for (iter = 0; iter < arrayList_size(interfaces) && (status == CELIX_SUCCESS); iter++) {
						char *interface = arrayList_get(interfaces, iter);
						export_registration_pt registration = NULL;

						exportRegistration_create(admin->loghelper, reference, NULL, admin, admin->context, &registration);
						arrayList_add(*registrations, registration);

						status = remoteServiceAdmin_installEndpoint(admin, registration, reference, interface);

						if (status == CELIX_SUCCESS) {
							status = exportRegistration_open(registration);
						}

						if (status == CELIX_SUCCESS) {
							status = exportRegistration_startTracking(registration);
						} else {
							exportRegistration_destroy(&registration);
						}
					}

					if (status == CELIX_SUCCESS) {
						celixThreadMutex_lock(&admin->exportedServicesLock);
						hashMap_put(admin->exportedServices, reference, *registrations);
						celixThreadMutex_unlock(&admin->exportedServicesLock);

						if (properties == NULL) {
							properties = properties_create();

							unsigned int size = 0;
							char **keys;

							serviceReference_getPropertyKeys(reference, &keys, &size);
							int i = 0;
							for (; i < size; i++) {
								char *key = keys[i];
								char *value = NULL;
								serviceReference_getProperty(reference, key, &value);

								properties_set(properties, key, value);
							}

							free(keys);
						}

						if (wtmService->exportWiringEndpoint(wtmService->manager, properties) != CELIX_SUCCESS) {
							properties_destroy(properties);
							printf("RSA: Installation of Callback failed\n");
						} else {
							printf("RSA: Receive callback successfully installed\n");
						}
					}
				}
			} else {
				printf("RSA: WTM service is not available.\n");
			}
		} else {
			printf("RSA: Could not retrieve wtm service reference.\n");
		}

		arrayList_destroy(interfaces);
	}

	return status;
}

celix_status_t remoteServiceAdmin_removeExportedService(export_registration_pt registration) {
	celix_status_t status = CELIX_SUCCESS;
	remote_service_admin_pt admin = registration->rsa;

	celixThreadMutex_lock(&admin->exportedServicesLock);

	hashMap_remove(admin->exportedServices, registration->reference);

	celixThreadMutex_unlock(&admin->exportedServicesLock);

	return status;
}

celix_status_t remoteServiceAdmin_installEndpoint(remote_service_admin_pt admin, export_registration_pt registration, service_reference_pt reference, char *interface) {
	celix_status_t status = CELIX_SUCCESS;
	properties_pt endpointProperties = properties_create();

	unsigned int size = 0;
	char **keys;

	serviceReference_getPropertyKeys(reference, &keys, &size);

	int i = 0;

	for (; i < size; i++) {
		char *key = keys[i];
		char *value = NULL;

		if (serviceReference_getProperty(reference, key, &value) == CELIX_SUCCESS && strcmp(key, (char*) OSGI_RSA_SERVICE_EXPORTED_INTERFACES) != 0 && strcmp(key, (char*) OSGI_FRAMEWORK_OBJECTCLASS) != 0) {
			properties_set(endpointProperties, key, value);
		}
	}

	hash_map_entry_pt entry = hashMap_getEntry(endpointProperties, (void *) OSGI_FRAMEWORK_SERVICE_ID);

	char* key = hashMapEntry_getKey(entry);
	char *serviceId = (char *) hashMap_remove(endpointProperties, (void *) OSGI_FRAMEWORK_SERVICE_ID);
	char *uuid = NULL;

	uuid_t endpoint_uid;
	uuid_generate(endpoint_uid);
	char endpoint_uuid[37];
	uuid_unparse_lower(endpoint_uid, endpoint_uuid);

	bundleContext_getProperty(admin->context, OSGI_FRAMEWORK_FRAMEWORK_UUID, &uuid);
	properties_set(endpointProperties, (char*) OSGI_RSA_ENDPOINT_FRAMEWORK_UUID, uuid);
	properties_set(endpointProperties, (char*) OSGI_FRAMEWORK_OBJECTCLASS, interface);
	properties_set(endpointProperties, (char*) OSGI_RSA_ENDPOINT_SERVICE_ID, serviceId);
	properties_set(endpointProperties, (char*) OSGI_RSA_ENDPOINT_ID, endpoint_uuid);
	properties_set(endpointProperties, (char*) OSGI_RSA_SERVICE_IMPORTED, "true");
	properties_set(endpointProperties, (char*) OSGI_RSA_SERVICE_IMPORTED_CONFIGS, (char*) CONFIGURATION_TYPE);

	endpoint_description_pt endpointDescription = NULL;
	remoteServiceAdmin_createEndpointDescription(admin, reference, endpointProperties, interface, &endpointDescription);
	exportRegistration_setEndpointDescription(registration, endpointDescription);

	free(key);
	free(serviceId);
	free(keys);

	return status;
}

celix_status_t remoteServiceAdmin_createEndpointDescription(remote_service_admin_pt admin, service_reference_pt reference, properties_pt endpointProperties, char *interface, endpoint_description_pt *description) {
	celix_status_t status = CELIX_SUCCESS;

	*description = calloc(1, sizeof(**description));
	if (!*description) {
		status = CELIX_ENOMEM;
	} else {
		(*description)->id = properties_get(endpointProperties, (char*) OSGI_RSA_ENDPOINT_ID);
		char *serviceId = NULL;
		serviceReference_getProperty(reference, (char*) OSGI_FRAMEWORK_SERVICE_ID, &serviceId);
		(*description)->serviceId = strtoull(serviceId, NULL, 0);
		(*description)->frameworkUUID = properties_get(endpointProperties, (char*) OSGI_RSA_ENDPOINT_FRAMEWORK_UUID);
		(*description)->service = interface;
		(*description)->properties = endpointProperties;
	}

	return status;
}

celix_status_t remoteServiceAdmin_destroyEndpointDescription(endpoint_description_pt *description) {
	celix_status_t status = CELIX_SUCCESS;

	properties_destroy((*description)->properties);
	free(*description);

	return status;
}

celix_status_t remoteServiceAdmin_getExportedServices(remote_service_admin_pt admin, array_list_pt *services) {
	celix_status_t status = CELIX_SUCCESS;
	return status;
}

celix_status_t remoteServiceAdmin_getImportedEndpoints(remote_service_admin_pt admin, array_list_pt *services) {
	celix_status_t status = CELIX_SUCCESS;
	return status;
}

celix_status_t remoteServiceAdmin_importService(remote_service_admin_pt admin, endpoint_description_pt endpointDescription, import_registration_pt *registration) {
	celix_status_t status = CELIX_SUCCESS;

	logHelper_log(admin->loghelper, OSGI_LOGSERVICE_INFO, "RSA: Import service %s", endpointDescription->service);

	celixThreadMutex_lock(&admin->importedServicesLock);

	/* get WireId */
	char* wireId = properties_get(endpointDescription->properties, WIRING_ENDPOINT_DESCRIPTION_WIRE_ID_KEY);

	if (wireId == NULL) {
		logHelper_log(admin->loghelper, OSGI_LOGSERVICE_WARNING, "RSA: Missing WireId for service %s", endpointDescription->service);
	} else {
		void* service = NULL;
		wiring_topology_manager_service_pt wtmService = NULL;
		service_reference_pt wtmServiceRef = NULL;

		status = bundleContext_getServiceReference(admin->context, (char *) INAETICS_WIRING_TOPOLOGY_MANAGER_SERVICE, &wtmServiceRef);

		if (bundleContext_getService(admin->context, wtmServiceRef, &service) == CELIX_SUCCESS) {
			wtmService = (wiring_topology_manager_service_pt) service;
		}

		// if the dereferenced instance is null then we know the service has been removed
		if (wtmService != NULL) {
			properties_pt rsaProperties = properties_create();

			properties_set(rsaProperties, WIRING_ENDPOINT_DESCRIPTION_WIRE_ID_KEY, wireId);

			if (wtmService->importWiringEndpoint(wtmService->manager, rsaProperties) == CELIX_SUCCESS) {

				import_registration_factory_pt registration_factory = (import_registration_factory_pt) hashMap_get(admin->importedServices, endpointDescription->service);

				// check whether we already have a registration_factory registered in the hashmap
				if (registration_factory == NULL) {
					status = importRegistrationFactory_install(admin->loghelper, endpointDescription->service, admin->context, &registration_factory);
					if (status == CELIX_SUCCESS) {
						hashMap_put(admin->importedServices, endpointDescription->service, registration_factory);
					}
				}

				// factory available
				if (status != CELIX_SUCCESS || (registration_factory->trackedFactory == NULL)) {
					logHelper_log(admin->loghelper, OSGI_LOGSERVICE_WARNING, "RSA: no proxyFactory available.");
					if (status == CELIX_SUCCESS) {
						status = CELIX_SERVICE_EXCEPTION;
					}
				} else {
					// we create an importRegistration per imported service
					importRegistration_create(endpointDescription, admin, (sendToHandle) &remoteServiceAdmin_send, admin->context, registration);
					registration_factory->trackedFactory->registerProxyService(registration_factory->trackedFactory->factory, endpointDescription, admin, (sendToHandle) &remoteServiceAdmin_send);

					arrayList_add(registration_factory->registrations, *registration);
				}

			}
		}
	} // hmmm...

	celixThreadMutex_unlock(&admin->importedServicesLock);

	return status;
}

celix_status_t remoteServiceAdmin_removeImportedService(remote_service_admin_pt admin, import_registration_pt registration) {
	celix_status_t status = CELIX_SUCCESS;
	endpoint_description_pt endpointDescription = (endpoint_description_pt) registration->endpointDescription;
	import_registration_factory_pt registration_factory = NULL;

	celixThreadMutex_lock(&admin->importedServicesLock);

	registration_factory = (import_registration_factory_pt) hashMap_get(admin->importedServices, endpointDescription->service);

	// factory available
	if ((registration_factory == NULL) || (registration_factory->trackedFactory == NULL)) {
		logHelper_log(admin->loghelper, OSGI_LOGSERVICE_ERROR, "RSA: Error while retrieving registration factory for imported service %s", endpointDescription->service);
	} else {
		registration_factory->trackedFactory->unregisterProxyService(registration_factory->trackedFactory->factory, endpointDescription);
		arrayList_removeElement(registration_factory->registrations, registration);
		importRegistration_destroy(registration);

		if (arrayList_isEmpty(registration_factory->registrations)) {
			logHelper_log(admin->loghelper, OSGI_LOGSERVICE_INFO, "RSA: closing proxy.");

			serviceTracker_close(registration_factory->proxyFactoryTracker);
			importRegistrationFactory_close(registration_factory);

			hashMap_remove(admin->importedServices, endpointDescription->service);

			importRegistrationFactory_destroy(&registration_factory);
		}
	}

	celixThreadMutex_unlock(&admin->importedServicesLock);

	return status;
}

celix_status_t remoteServiceAdmin_send(remote_service_admin_pt admin, endpoint_description_pt endpointDescription, char *request, char **reply, int* replyStatus) {
	celix_status_t status = CELIX_ILLEGAL_ARGUMENT;

	char* wireId = properties_get(endpointDescription->properties, (char*) WIRING_ENDPOINT_DESCRIPTION_WIRE_ID_KEY);

	if (wireId == NULL) {
		printf("RSA: send called w/ proper endpoint_description: wireId missing.\n");
	} else {
		wiring_send_service_pt wiringSendService = NULL;

		wiringSendService = hashMap_get(admin->sendServices, wireId);

		if (wiringSendService == NULL) {
			printf("RSA: No SendService w/ wireId %s found.\n", wireId);
		} else {
			json_t *root;
			json_t *json_request;
			json_error_t jsonError;

			int replyStatus = 0;

			json_request = json_loads(request, 0, &jsonError);
			root = json_pack("{s:i, s:o}", "service.id", endpointDescription->serviceId, "request", json_request);
			char *json_data = json_dumps(root, 0);

			status = wiringSendService->send(wiringSendService, json_data, reply, &replyStatus);

			printf("RSA: %s sent\n", request);
			printf("RSA: %s received\n", *reply);
		}
	}

	return status;
}

static celix_status_t remoteServiceAdmin_createSendServiceTracker(remote_service_admin_pt admin) {
	celix_status_t status = CELIX_SUCCESS;
	service_tracker_customizer_pt customizer = NULL;

	status = serviceTrackerCustomizer_create(admin, remoteServiceAdmin_sendServiceAdding, remoteServiceAdmin_sendServiceAdded, remoteServiceAdmin_sendServiceModified, remoteServiceAdmin_sendServiceRemoved, &customizer);

	if (status == CELIX_SUCCESS) {
		char filter[512];

		snprintf(filter, 512, "(%s=%s)", (char*) OSGI_FRAMEWORK_OBJECTCLASS, (char*) INAETICS_WIRING_SEND_SERVICE);

		status = serviceTracker_createWithFilter(admin->context, filter, customizer, &admin->sendServicesTracker);

		if ((status == CELIX_SUCCESS) && (serviceTracker_open(admin->sendServicesTracker) == CELIX_SUCCESS)) {
			printf("RSA: sendServiceTracker created w/ filter %s\n", filter);
		} else {
			printf("RSA: sendServiceTracker could not be created w/ %s\n", filter);
		}
	}

	return status;
}

static celix_status_t remoteServiceAdmin_destroySendServiceTracker(remote_service_admin_pt admin) {
	celix_status_t status = CELIX_SUCCESS;

	status = serviceTracker_close(admin->sendServicesTracker);

	if (status == CELIX_SUCCESS) {
		status = serviceTracker_destroy(admin->sendServicesTracker);
	}

	return status;
}

static celix_status_t remoteServiceAdmin_sendServiceAdding(void * handle, service_reference_pt reference, void **service) {
	celix_status_t status = CELIX_SUCCESS;

	remote_service_admin_pt admin = (remote_service_admin_pt) handle;

	status = bundleContext_getService(admin->context, reference, service);

	return status;
}

static celix_status_t remoteServiceAdmin_sendServiceAdded(void * handle, service_reference_pt reference, void * service) {
	celix_status_t status = CELIX_SUCCESS;

	remote_service_admin_pt admin = (remote_service_admin_pt) handle;

	printf("RSA: Send Service Added\n");

	wiring_send_service_pt wiringSendService = (wiring_send_service_pt) service;
	char* wireId = properties_get(wiringSendService->wiringEndpointDescription->properties, WIRING_ENDPOINT_DESCRIPTION_WIRE_ID_KEY);

	hashMap_put(admin->sendServices, wireId, wiringSendService);

	return status;
}

static celix_status_t remoteServiceAdmin_sendServiceModified(void * handle, service_reference_pt reference, void * service) {
	celix_status_t status = CELIX_SUCCESS;

	return status;
}

static celix_status_t remoteServiceAdmin_sendServiceRemoved(void * handle, service_reference_pt reference, void * service) {
	celix_status_t status = CELIX_SUCCESS;
	remote_service_admin_pt admin = (remote_service_admin_pt) handle;

	wiring_send_service_pt wiringSendService = (wiring_send_service_pt) service;
	char* wireId = properties_get(wiringSendService->wiringEndpointDescription->properties, WIRING_ENDPOINT_DESCRIPTION_WIRE_ID_KEY);

	hashMap_remove(admin->sendServices, wireId);

	return status;
}

celix_status_t exportReference_getExportedEndpoint(export_reference_pt reference, endpoint_description_pt *endpoint) {
	celix_status_t status = CELIX_SUCCESS;

	*endpoint = reference->endpoint;

	return status;
}

celix_status_t exportReference_getExportedService(export_reference_pt reference) {
	celix_status_t status = CELIX_SUCCESS;
	return status;
}

celix_status_t importReference_getImportedEndpoint(import_reference_pt reference) {
	celix_status_t status = CELIX_SUCCESS;
	return status;
}

celix_status_t importReference_getImportedService(import_reference_pt reference) {
	celix_status_t status = CELIX_SUCCESS;
	return status;
}
