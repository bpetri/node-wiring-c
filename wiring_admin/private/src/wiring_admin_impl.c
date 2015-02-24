/**
 * Licensed under Apache License v2. See LICENSE for more information.
 */

#include <stdio.h>
#include <stdlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <uuid/uuid.h>

#include <properties.h>

#include "curl/curl.h"

#include "wiring_admin_impl.h"
#include "wiring_common_utils.h"

#include "civetweb.h"

// defines how often the webserver is restarted (with an increased port number)
#define MAX_NUMBER_OF_RESTARTS 	5
#define MAX_URL_LENGTH 			128

struct post {
	const char *readptr;
	int size;
};

struct get {
	char *writeptr;
	int size;
};

static const char *data_response_headers = "HTTP/1.1 200 OK\r\n"
		"Cache: no-cache\r\n"
		"Content-Type: application/json\r\n"
		"\r\n";

static const char *no_content_response_headers = "HTTP/1.1 204 OK\r\n";

static int wiringAdmin_callback(struct mg_connection *conn);

static size_t wiringAdmin_HTTPReqReadCallback(void *ptr, size_t size, size_t nmemb, void *userp);
static size_t wiringAdmin_HTTPReqWrite(void *contents, size_t size, size_t nmemb, void *userp);

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

		celixThreadMutex_create(&(*admin)->exportedWiringEndpointFunctionLock, NULL);

		celixThreadMutex_create(&(*admin)->wiringProxiesLock, NULL);
		(*admin)->wiringProxies = hashMap_create(NULL, wiringEndpointDescription_hash, NULL, wiringEndpointDescription_equals);

		bundleContext_getProperty(context, NODE_DISCOVERY_NODE_WA_PORT, &port);
		if (port == NULL) {
			port = (char *) DEFAULT_WA_PORT;
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

		// Prepare callbacks structure. We have only one callback, the rest are NULL.
		struct mg_callbacks callbacks;
		memset(&callbacks, 0, sizeof(callbacks));
		callbacks.begin_request = wiringAdmin_callback;

		do {
			char newPort[10];
			const char *options[] = { "listening_ports", port, NULL };

			(*admin)->ctx = mg_start(&callbacks, (*admin), options);

			if ((*admin)->ctx == NULL) {
				char* endptr = port;
				int currentPort = strtol(port, &endptr, 10);

				errno = 0;

				if (*endptr || errno != 0) {
					currentPort = strtol(DEFAULT_WA_PORT, NULL, 10);
				}

				port_counter++;
				snprintf(&newPort[0], 6, "%d", (currentPort + 1));

				printf("WA: Error while starting WA server on port %s - retrying on port %s...\n", port, newPort);
				port = newPort;
			}
		} while (((*admin)->ctx == NULL) && (port_counter < MAX_NUMBER_OF_RESTARTS));

		char* url = calloc(MAX_URL_LENGTH, sizeof(*url));

		if (ip != NULL) {
			snprintf(url, MAX_URL_LENGTH, "http://%s:%s", ip, port);
		} else {
			printf("WA: No IP address for HTTP Wiring Endpint set. Using %s\n", DEFAULT_WA_ADDRESS);
			snprintf(url, MAX_URL_LENGTH, "http://%s:%s", (char*) DEFAULT_WA_ADDRESS, port);
		}

		if (status == CELIX_SUCCESS) {
			char* fwuuid = NULL;

			status = bundleContext_getProperty((*admin)->context, OSGI_FRAMEWORK_FRAMEWORK_UUID, &fwuuid);

			if (status == CELIX_SUCCESS) {
				printf("WA: HTTP Wiring Endpoint running at %s\n", url);

				properties_pt props = properties_create();
				properties_set(props, WIRING_ENDPOINT_DESCRIPTION_PROTOCOL_KEY, WIRING_ENDPOINT_PROTOCOL_VALUE);
				properties_set(props, WIRING_ENDPOINT_DESCRIPTION_URL_KEY, url);

				properties_set(props, (char*) OSGI_RSA_ENDPOINT_FRAMEWORK_UUID, fwuuid);

				status = wiringEndpointDescription_create(NULL, props, &((*admin)->wEndpointDescription));
			}

			if (status != CELIX_SUCCESS) {
				printf("WA: Error while initializing WiringEndpointDescription%s\n", url);
				free(*admin);
			}
		} else {
			printf("WA: Cannot activate HTTP Wiring Endpoint at %s\n", url);
		}

		if (detectedIp != NULL) {
			free(detectedIp);
		}

		if (url != NULL) {
			free(url);
		}
	}

	return status;
}

celix_status_t wiringAdmin_destroy(wiring_admin_pt* admin) {
	celix_status_t status = CELIX_SUCCESS;

	celixThreadMutex_lock(&((*admin)->exportedWiringEndpointFunctionLock));
	(*admin)->rsa_inetics_callback = NULL;
	celixThreadMutex_unlock(&((*admin)->exportedWiringEndpointFunctionLock));
	celixThreadMutex_destroy(&((*admin)->exportedWiringEndpointFunctionLock));

	celixThreadMutex_lock(&((*admin)->wiringProxiesLock));
	hashMap_destroy((*admin)->wiringProxies, true, false);
	celixThreadMutex_unlock(&((*admin)->wiringProxiesLock));
	celixThreadMutex_destroy(&((*admin)->wiringProxiesLock));

	status = wiringEndpointDescription_destroy((*admin)->wEndpointDescription);

	free(*admin);
	*admin = NULL;

	return status;
}

celix_status_t wiringAdmin_stop(wiring_admin_pt admin) {
	celix_status_t status = CELIX_SUCCESS;

	if (admin->ctx != NULL) {
		char* url = properties_get(admin->wEndpointDescription->properties, WIRING_ENDPOINT_DESCRIPTION_URL_KEY);
		printf("WA: Stopping HTTP Wiring Endpoint running at %s ...\n", url);
		mg_stop(admin->ctx);
		admin->ctx = NULL;
	}

	celixThreadMutex_lock(&admin->exportedWiringEndpointFunctionLock);
	admin->rsa_inetics_callback = NULL;
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
			if (wa->rsa_inetics_callback != NULL) {

				uint64_t datalength = request_info->content_length;
				char* data = malloc(datalength + 1);
				mg_read(conn, data, datalength);
				data[datalength] = '\0';

				char *response = NULL;
				wa->rsa_inetics_callback(data, &response);

				if (response != NULL) {
					mg_write(conn, data_response_headers, strlen(data_response_headers));
					mg_write(conn, response, strlen(response));

					free(response);
				} else {
					mg_write(conn, no_content_response_headers, strlen(no_content_response_headers));
				}
				result = 1;

				free(data);
			} else {
				printf("WA: Received HTTP Request, but no RSA_Inaetics callback is installed. Discarding request.\n");
			}

			celixThreadMutex_unlock(&wa->exportedWiringEndpointFunctionLock);

		}

	}

	return result;
}

celix_status_t wiringAdmin_exportWiringEndpoint(wiring_admin_pt admin, rsa_inaetics_receive_cb rsa_inaetics_cb) {
	celix_status_t status = CELIX_SUCCESS;

	if (rsa_inaetics_cb == NULL) {
		return CELIX_ILLEGAL_ARGUMENT;
	}

	celixThreadMutex_lock(&admin->exportedWiringEndpointFunctionLock);
	admin->rsa_inetics_callback = rsa_inaetics_cb;
	celixThreadMutex_unlock(&admin->exportedWiringEndpointFunctionLock);

	return status;
}

celix_status_t wiringAdmin_getWiringEndpoint(wiring_admin_pt admin, wiring_endpoint_description_pt* wEndpoint) {

	celix_status_t status = CELIX_SUCCESS;

	*wEndpoint = admin->wEndpointDescription;

	return status;

}

celix_status_t wiringAdmin_removeExportedWiringEndpoint(wiring_admin_pt admin, rsa_inaetics_receive_cb rsa_inaetics_cb) {
	celix_status_t status = CELIX_SUCCESS;

	if (rsa_inaetics_cb == NULL) {
		return CELIX_ILLEGAL_ARGUMENT;
	}

	if (admin->rsa_inetics_callback == rsa_inaetics_cb) {
		celixThreadMutex_lock(&admin->exportedWiringEndpointFunctionLock);
		admin->rsa_inetics_callback = NULL;
		celixThreadMutex_unlock(&admin->exportedWiringEndpointFunctionLock);
	}

	return status;
}

celix_status_t wiringAdmin_importWiringEndpoint(wiring_admin_pt admin, wiring_endpoint_description_pt wEndpointDescription, rsa_inaetics_send* send, wiring_handle* handle) {
	celix_status_t status = CELIX_SUCCESS;

	// WTM already checked for us that wEndpointDescription properties matches our properties

	celixThreadMutex_lock(&admin->wiringProxiesLock);

	void* h = malloc(1);
	hashMap_put(admin->wiringProxies, h, wEndpointDescription);
	*handle = h;
	*send = wiringAdmin_send;

	celixThreadMutex_unlock(&admin->wiringProxiesLock);

	return status;
}

celix_status_t wiringAdmin_removeImportedWiringEndpoint(wiring_admin_pt admin, wiring_handle handle) {
	celix_status_t status = CELIX_SUCCESS;

	celixThreadMutex_lock(&admin->wiringProxiesLock);

	hashMap_remove(admin->wiringProxies, handle);
	free(handle);

	celixThreadMutex_unlock(&admin->wiringProxiesLock);

	return status;
}

celix_status_t wiringAdmin_send(wiring_admin_pt admin, void* handle, char *request, char **reply, int* replyStatus) {

	celix_status_t status = CELIX_SUCCESS;

	celixThreadMutex_lock(&admin->wiringProxiesLock);
	wiring_endpoint_description_pt wepd = (wiring_endpoint_description_pt) hashMap_get(admin->wiringProxies, handle);
	celixThreadMutex_unlock(&admin->wiringProxiesLock);

	if (wepd == NULL) {
		return CELIX_ILLEGAL_STATE;
	}

	struct post post;
	post.readptr = request;
	post.size = strlen(request);

	struct get get;
	get.size = 0;
	get.writeptr = malloc(1);

	char* url = properties_get(wepd->properties, WIRING_ENDPOINT_DESCRIPTION_URL_KEY);

	CURL *curl;
	CURLcode res;

	curl = curl_easy_init();
	if (!curl) {
		status = CELIX_ILLEGAL_STATE;
	} else {
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, wiringAdmin_HTTPReqReadCallback);
		curl_easy_setopt(curl, CURLOPT_READDATA, &post);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, wiringAdmin_HTTPReqWrite);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&get);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (curl_off_t)post.size);
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		*reply = get.writeptr;
		*replyStatus = res;
	}

	return status;
}

static size_t wiringAdmin_HTTPReqReadCallback(void *ptr, size_t size, size_t nmemb, void *userp) {
	struct post *post = userp;

	if (post->size) {
		*(char *) ptr = post->readptr[0];
		post->readptr++;
		post->size--;
		return 1;
	}

	return 0;
}

static size_t wiringAdmin_HTTPReqWrite(void *contents, size_t size, size_t nmemb, void *userp) {
	size_t realsize = size * nmemb;
	struct get *mem = (struct get *) userp;

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
