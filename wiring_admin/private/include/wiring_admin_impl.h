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
 * wiring_admin_impl.h
 *
 *  \date       Dec 5, 2013
 *  \author    	<a href="mailto:celix-dev@incubator.apache.org">Apache Celix Project Team</a>
 *  \copyright	Apache License, Version 2.0
 */

#ifndef WIRING_ADMIN_IMPL_H_
#define WIRING_ADMIN_IMPL_H_

#include "remote_constants.h"
#include "constants.h"
#include "utils.h"
#include "bundle_context.h"
#include "bundle.h"
#include "service_reference.h"
#include "service_registration.h"
#include "celix_threads.h"

#include "wiring_admin.h"

/* We are the HTTP Wiring Admin, so out ep_protocol is HTTP */
#define WIRING_ENDPOINT_PROTOCOL_VALUE "HTTP"

struct wiring_admin {
	bundle_context_pt context;

	celix_thread_mutex_t exportedWiringEndpointFunctionLock;
	celix_status_t(*rsa_inetics_callback)(char* data, char**response);

	wiring_endpoint_description_pt wEndpointDescription;

	celix_thread_mutex_t wiringProxiesLock;
	hash_map_pt wiringProxies; //key=void*, value=wiring_proxy_registration_pt

	struct mg_context *ctx;
};

typedef struct wiring_proxy_registration{

	wiring_endpoint_description_pt wiringEndpointDescription;
	wiring_admin_pt wiringAdmin;

} * wiring_proxy_registration_pt;


celix_status_t wiringAdmin_create(bundle_context_pt context, wiring_admin_pt *admin);
celix_status_t wiringAdmin_destroy(wiring_admin_pt* admin);
celix_status_t wiringAdmin_stop(wiring_admin_pt admin);

celix_status_t wiringAdmin_exportWiringEndpoint(wiring_admin_pt admin, rsa_inaetics_receive_cb rsa_inaetics_cb);
celix_status_t wiringAdmin_removeExportedWiringEndpoint(wiring_admin_pt admin, rsa_inaetics_receive_cb rsa_inaetics_cb);
celix_status_t wiringAdmin_getWiringEndpoint(wiring_admin_pt admin,wiring_endpoint_description_pt* wEndpoint);

celix_status_t wiringAdmin_send(wiring_admin_pt admin,void* handle, char *request, char **reply, int* replyStatus);
celix_status_t wiringAdmin_importWiringEndpoint(wiring_admin_pt admin, wiring_endpoint_description_pt endpoint,rsa_inaetics_send* sendFunc,wiring_handle* handle);
celix_status_t wiringAdmin_removeImportedWiringEndpoint(wiring_admin_pt admin, wiring_handle handle);


#endif /* WIRING_ADMIN_IMPL_H_ */
