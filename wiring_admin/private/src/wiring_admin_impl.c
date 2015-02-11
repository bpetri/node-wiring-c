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
 *  \author    	<a href="mailto:celix-dev@incubator.apache.org">Apache Celix Project Team</a>
 *  \copyright	Apache License, Version 2.0
 */
#include <stdio.h>
#include <stdlib.h>


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <uuid/uuid.h>

#include "curl/curl.h"

#include "wiring_admin_impl.h"
#include "wiring_common_utils.h"

#include "civetweb.h"

// defines how often the webserver is restarted (with an increased port number)
#define MAX_NUMBER_OF_RESTARTS 	5

struct post {
	const char *readptr;
	int size;
};

struct get {
	char *writeptr;
	int size;
};

static const char *data_response_headers =
		"HTTP/1.1 200 OK\r\n"
		"Cache: no-cache\r\n"
		"Content-Type: application/json\r\n"
		"\r\n";

static const char *no_content_response_headers = "HTTP/1.1 204 OK\r\n";

static int wiringAdmin_callback(struct mg_connection *conn);

//static size_t remoteServiceAdmin_readCallback(void *ptr, size_t size, size_t nmemb, void *userp);
//static size_t remoteServiceAdmin_write(void *contents, size_t size, size_t nmemb, void *userp);


celix_status_t wiringAdmin_create(bundle_context_pt context, wiring_admin_pt *admin) {
	celix_status_t status = CELIX_SUCCESS;

	*admin = calloc(1, sizeof(**admin));
	if (!*admin) {
		status = CELIX_ENOMEM;
	} else {
		unsigned int port_counter = 0;
		char *port = NULL;
		char *ip = NULL;
		char *detectedIp = NULL;
		(*admin)->context = context;

		char* fwuuid=NULL;
		if (((bundleContext_getProperty((*admin)->context, OSGI_FRAMEWORK_FRAMEWORK_UUID, &fwuuid)) != CELIX_SUCCESS) || (!fwuuid)) {
			printf("WA: Could not retrieve framework UUID!\n");
			free(*admin);
			return CELIX_ILLEGAL_STATE;
		}

		properties_pt props = properties_create();
		properties_set(props,WIRING_ENDPOINT_PROTOCOL_KEY,WIRING_ENDPOINT_PROTOCOL_VALUE);

		if( wiringEndpointDescription_create(fwuuid,props,&((*admin)->wEndpointDescription)) != CELIX_SUCCESS ){
			printf("WA: Could not create our own WiringEndpointDescription!\n");
			free(*admin);
			return CELIX_ENOMEM;
		}

		celixThreadMutex_create(&(*admin)->exportedWiringEndpointFunctionLock, NULL);

		bundleContext_getProperty(context, NODE_DISCOVERY_NODE_WA_PORT, &port);
		if (port == NULL) {
			port = (char *)DEFAULT_WA_PORT;
		}

		bundleContext_getProperty(context, NODE_DISCOVERY_NODE_WA_ADDRESS, &ip);
		if (ip == NULL) {
			char *interface = NULL;

			bundleContext_getProperty(context, NODE_DISCOVERY_NODE_WA_ITF, &interface);
			if ((interface != NULL) && (wiring_getIpAddress(interface, &detectedIp) != CELIX_SUCCESS)) {
				printf("WA: Could not retrieve IP adress for interface %s\n", interface);
			}

			if (ip == NULL) {
				wiring_getIpAddress(NULL, &detectedIp);
			}

			ip = detectedIp;
		}

		if (ip != NULL) {
			(*admin)->wEndpointDescription->url = strdup(ip);
		}
		else {
			printf("WA: No IP address for HTTP Wiring Endpint set. Using %s\n", DEFAULT_WA_ADDRESS);
			(*admin)->wEndpointDescription->url = strdup((char*) DEFAULT_WA_ADDRESS);
		}

		if (detectedIp != NULL) {
			free(detectedIp);
		}

		// Prepare callbacks structure. We have only one callback, the rest are NULL.
		struct mg_callbacks callbacks;
		memset(&callbacks, 0, sizeof(callbacks));
		callbacks.begin_request = wiringAdmin_callback;

		do {
			char newPort[10];
			const char *options[] = { "listening_ports", port, NULL};

			(*admin)->ctx = mg_start(&callbacks, (*admin), options);

			if ((*admin)->ctx != NULL) {
				(*admin)->wEndpointDescription->port=strtol(port,NULL,10);

			}
			else {
				char* endptr = port;
				int currentPort = strtol(port, &endptr, 10);

				errno = 0;

				if (*endptr || errno != 0) {
					currentPort = strtol(DEFAULT_WA_PORT, NULL, 10);
				}

				port_counter++;
				snprintf(&newPort[0], 6,  "%d", (currentPort+1));

				printf("WA: Error while starting WA server on port %s - retrying on port %s...\n", port, newPort);
				port = newPort;
			}
		} while(((*admin)->ctx == NULL) && (port_counter < MAX_NUMBER_OF_RESTARTS));

	}

	if(status==CELIX_SUCCESS){
		printf("WA: HTTP Wiring Endpoint running at %s:%u\n",(*admin)->wEndpointDescription->url,(*admin)->wEndpointDescription->port);
	}
	else{
		printf("WA: Cannot activate HTTP Wiring Endpoint at %s:%u\n",(*admin)->wEndpointDescription->url,(*admin)->wEndpointDescription->port);
	}
	return status;
}


celix_status_t wiringAdmin_destroy(wiring_admin_pt* admin)
{
	celix_status_t status = CELIX_SUCCESS;

	celixThreadMutex_lock(&((*admin)->exportedWiringEndpointFunctionLock));
	(*admin)->rsa_inetics_callback=NULL;
	celixThreadMutex_unlock(&((*admin)->exportedWiringEndpointFunctionLock));
	celixThreadMutex_destroy(&((*admin)->exportedWiringEndpointFunctionLock));

	status= wiringEndpointDescription_destroy((*admin)->wEndpointDescription);

	free(*admin);
	*admin = NULL;

	return status;
}

celix_status_t wiringAdmin_stop(wiring_admin_pt admin) {
	celix_status_t status = CELIX_SUCCESS;

	if (admin->ctx != NULL) {
		printf("WA: StoppingHTTP Wiring Endpoint running at %s:%u ...\n",admin->wEndpointDescription->url,admin->wEndpointDescription->port);
		mg_stop(admin->ctx);
		admin->ctx = NULL;
	}

	celixThreadMutex_lock(&admin->exportedWiringEndpointFunctionLock);
	admin->rsa_inetics_callback=NULL;
	celixThreadMutex_unlock(&admin->exportedWiringEndpointFunctionLock);



	return status;
}


static int wiringAdmin_callback(struct mg_connection *conn) {
	int result = 0; // zero means: let civetweb handle it further, any non-zero value means it is handled by us...

	const struct mg_request_info *request_info = mg_get_request_info(conn);
	if (request_info->uri != NULL) {
		wiring_admin_pt wa = request_info->user_data;


		if (strcmp("POST", request_info->request_method) == 0) {

			celixThreadMutex_lock(&wa->exportedWiringEndpointFunctionLock);

			/* Pass all the data segment to the RSA_Inaetics using the callback function */
			if(wa->rsa_inetics_callback!=NULL){

				uint64_t datalength = request_info->content_length;
				char* data = malloc(datalength + 1);
				mg_read(conn, data, datalength);
				data[datalength] = '\0';

				char *response = NULL;
				wa->rsa_inetics_callback(data,&response);

				if (response != NULL) {
					mg_write(conn, data_response_headers, strlen(data_response_headers));
					mg_write(conn, response, strlen(response));

					free(response);
				} else {
					mg_write(conn, no_content_response_headers, strlen(no_content_response_headers));
				}
				result = 1;

				free(data);
			}
			else{
				printf("WA: Received HTTP Request, but no RSA_Inaetics callback is installed. Discarding request.\n");
			}

			celixThreadMutex_unlock(&wa->exportedWiringEndpointFunctionLock);

		}

	}

	return result;
}


celix_status_t wiringAdmin_exportWiringEndpoint(wiring_admin_pt admin, celix_status_t(*rsa_inaetics_cb)(char* data, char**response)) {
	celix_status_t status = CELIX_SUCCESS;

	if(rsa_inaetics_cb==NULL){
		return CELIX_ILLEGAL_ARGUMENT;
	}

	celixThreadMutex_lock(&admin->exportedWiringEndpointFunctionLock);
	admin->rsa_inetics_callback=rsa_inaetics_cb;
	celixThreadMutex_lock(&admin->exportedWiringEndpointFunctionLock);

	return status;
}

celix_status_t wiringAdmin_getWiringEndpoint(wiring_admin_pt admin,wiring_endpoint_description_pt* wEndpoint){

	celix_status_t status = CELIX_SUCCESS;

	*wEndpoint = admin->wEndpointDescription;

	return status;

}

celix_status_t wiringAdmin_removeExportedWiringEndpoint(wiring_admin_pt admin, celix_status_t(*rsa_inaetics_cb)(char* data, char**response)) {
	celix_status_t status = CELIX_SUCCESS;

	if(rsa_inaetics_cb==NULL){
		return CELIX_ILLEGAL_ARGUMENT;
	}

	if(admin->rsa_inetics_callback==rsa_inaetics_cb){
		celixThreadMutex_lock(&admin->exportedWiringEndpointFunctionLock);
		admin->rsa_inetics_callback=NULL;
		celixThreadMutex_lock(&admin->exportedWiringEndpointFunctionLock);
	}

	return status;
}


celix_status_t wiringAdmin_importWiringEndpoint(wiring_admin_pt admin, wiring_endpoint_description_pt wEndpointDescription) {
	celix_status_t status = CELIX_SUCCESS;

	/* +8 = :+max_len_for_port+\0 */

	/*
	char* wEndpointId = (char*)calloc(strlen(wEndpointDescription->url)+8,sizeof(char));

	printf("WA: Import Wiring Endpoint %s", wEndpointId);

	celixThreadMutex_lock(&admin->importedWiringEndpointLock);

	wiring_import_registration_factory_pt registration_factory = (wiring_import_registration_factory_pt) hashMap_get(admin->importedWiringEndpoint, wEndpointId);

	// check whether we already have a registration_factory registered in the hashmap
	if (registration_factory == NULL)
	{
		status = wiring_importRegistrationFactory_install(wEndpointId, admin->context, &registration_factory);
		if (status == CELIX_SUCCESS) {
			hashMap_put(admin->importedWiringEndpoint, wEndpointId, registration_factory);
		}
	}

	// factory available
	if (status != CELIX_SUCCESS || (registration_factory->trackedFactory == NULL))
	{
		printf("WA: no proxyFactory available.");
		if (status == CELIX_SUCCESS) {
			status = CELIX_SERVICE_EXCEPTION;
		}
	}
	else
	{
		// we create an importRegistration per imported service
		//wiring_importRegistration_create(wEndpointDescription, admin, (sendToHandle) &remoteServiceAdmin_send, admin->context, registration);
		//registration_factory->trackedFactory->registerProxyService(registration_factory->trackedFactory->factory,  wEndpointDescription, admin, (sendToHandle) &remoteServiceAdmin_send);
		wiring_importRegistration_create(wEndpointDescription, admin, NULL, admin->context, registration);
		registration_factory->trackedFactory->registerProxyService(registration_factory->trackedFactory->factory,  wEndpointDescription, admin, NULL);

		arrayList_add(registration_factory->registrations, *registration);
	}

	celixThreadMutex_unlock(&admin->importedWiringEndpointLock);
	 */

	return status;
}


celix_status_t wiringAdmin_removeImportedWiringEndpoint(wiring_admin_pt admin, wiring_endpoint_description_pt description) {
	celix_status_t status = CELIX_SUCCESS;

	/*
	endpoint_description_pt endpointDescription = (endpoint_description_pt) registration->endpointDescription;
	import_registration_factory_pt registration_factory = NULL;

	celixThreadMutex_lock(&admin->importedServicesLock);

	registration_factory = (import_registration_factory_pt) hashMap_get(admin->importedServices, endpointDescription->service);

	// factory available
	if ((registration_factory == NULL) || (registration_factory->trackedFactory == NULL))
	{
		logHelper_log(admin->loghelper, OSGI_LOGSERVICE_ERROR, "RSA: Error while retrieving registration factory for imported service %s", endpointDescription->service);
	}
	else
	{
		registration_factory->trackedFactory->unregisterProxyService(registration_factory->trackedFactory->factory, endpointDescription);
		arrayList_removeElement(registration_factory->registrations, registration);
		importRegistration_destroy(registration);

		if (arrayList_isEmpty(registration_factory->registrations))
		{
			logHelper_log(admin->loghelper, OSGI_LOGSERVICE_INFO, "RSA: closing proxy.");

			serviceTracker_close(registration_factory->proxyFactoryTracker);
			importRegistrationFactory_close(registration_factory);

			hashMap_remove(admin->importedServices, endpointDescription->service);

			importRegistrationFactory_destroy(&registration_factory);
		}
	}

	celixThreadMutex_unlock(&admin->importedServicesLock);
	 */

	return status;
}

/*
celix_status_t remoteServiceAdmin_send(remote_service_admin_pt rsa, endpoint_description_pt endpointDescription, char *request, char **reply, int* replyStatus) {

	struct post post;
	post.readptr = request;
	post.size = strlen(request);

	struct get get;
	get.size = 0;
	get.writeptr = malloc(1);

	char *serviceUrl = properties_get(endpointDescription->properties, (char*) ENDPOINT_URL);
	char url[256];
	snprintf(url, 256, "%s", serviceUrl);

	celix_status_t status = CELIX_SUCCESS;
	CURL *curl;
	CURLcode res;

	curl = curl_easy_init();
	if(!curl) {
		status = CELIX_ILLEGAL_STATE;
	} else {
		curl_easy_setopt(curl, CURLOPT_URL, &url[0]);
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, remoteServiceAdmin_readCallback);
		curl_easy_setopt(curl, CURLOPT_READDATA, &post);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, remoteServiceAdmin_write);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&get);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (curl_off_t)post.size);
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

 *reply = get.writeptr;
 *replyStatus = res;
	}

	return status;
}

static size_t remoteServiceAdmin_readCallback(void *ptr, size_t size, size_t nmemb, void *userp) {
	struct post *post = userp;

	if (post->size) {
 *(char *) ptr = post->readptr[0];
		post->readptr++;
		post->size--;
		return 1;
	}

	return 0;
}

static size_t remoteServiceAdmin_write(void *contents, size_t size, size_t nmemb, void *userp) {
	size_t realsize = size * nmemb;
	struct get *mem = (struct get *)userp;

	mem->writeptr = realloc(mem->writeptr, mem->size + realsize + 1);
	if (mem->writeptr == NULL) {

		printf("not enough memory (realloc returned NULL)");
		exit(EXIT_FAILURE);
	}

	memcpy(&(mem->writeptr[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->writeptr[mem->size] = 0;

	return realsize;
}

 */
