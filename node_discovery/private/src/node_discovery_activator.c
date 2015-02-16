/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *  KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*
 * discovery_activator.c
 *
 * \date        Aug 8, 2014
 * \author    	<a href="mailto:celix-dev@incubator.apache.org">Apache Celix Project Team</a>
 * \copyright	Apache License, Version 2.0
 */

#include <stdio.h>
#include <stdlib.h>

#include "bundle_activator.h"
#include "service_tracker.h"
#include "service_registration.h"
#include "constants.h"
#include "remote_constants.h"
#include "celix_log.h"

#include "node_discovery.h"
#include "node_discovery_impl.h"
#include "wiring_endpoint_listener.h"
#include "wiring_endpoint_description.h"


struct activator {
	bundle_context_pt context;
	node_discovery_pt node_discovery;

	service_tracker_pt wiringEndpointListenerTracker;
	wiring_endpoint_listener_pt wiringEndpointListener;
	service_registration_pt wiringEndpointListenerService;
};


static celix_status_t createWiringEndpointListenerTracker(struct activator *activator, service_tracker_pt *tracker) {
	celix_status_t status = CELIX_SUCCESS;

	service_tracker_customizer_pt customizer = NULL;

	status = serviceTrackerCustomizer_create(activator->node_discovery,
			node_discovery_wiringEndpointListenerAdding,
			node_discovery_wiringEndpointListenerAdded,
			node_discovery_wiringEndpointListenerModified,
			node_discovery_wiringEndpointListenerRemoved,
			&customizer);

	if (status == CELIX_SUCCESS) {
		status = serviceTracker_create(activator->context, (char *) INAETICS_WIRING_ENDPOINT_LISTENER_SERVICE, customizer, tracker);
	}

	return status;
}


celix_status_t bundleActivator_create(bundle_context_pt context, void **userData) {
	celix_status_t status = CELIX_SUCCESS;

	struct activator* activator = calloc(1, sizeof(*activator));

	if (!activator) {
		return CELIX_ENOMEM;
	}

	status = node_discovery_create(context, &activator->node_discovery);
	if (status != CELIX_SUCCESS) {
		return status;
	}

	activator->context=context;
	activator->wiringEndpointListenerTracker=NULL;
	activator->wiringEndpointListener=NULL;
	activator->wiringEndpointListenerService=NULL;

	status = createWiringEndpointListenerTracker(activator,&(activator->wiringEndpointListenerTracker));

	*userData = activator;

	return status;

}

celix_status_t bundleActivator_start(void * userData, bundle_context_pt context) {
	celix_status_t status = CELIX_SUCCESS;
	struct activator *activator = userData;
	char *uuid = NULL;

	status = bundleContext_getProperty(context, OSGI_FRAMEWORK_FRAMEWORK_UUID, &uuid);
	if (!uuid) {
		printf("NODE_DISCOVERY: no framework UUID defined?!\n");
		return CELIX_ILLEGAL_STATE;
	}

	size_t len = 11 + strlen(OSGI_FRAMEWORK_OBJECTCLASS) + strlen(OSGI_RSA_ENDPOINT_FRAMEWORK_UUID) + strlen(uuid);
	char *scope = calloc(len+1,sizeof(char));
	if (!scope) {
		return CELIX_ENOMEM;
	}

	//sprintf(scope, "(&(%s=*)(%s=%s))", OSGI_FRAMEWORK_OBJECTCLASS, OSGI_RSA_ENDPOINT_FRAMEWORK_UUID, uuid);
	sprintf(scope, "(%s=%s)", OSGI_RSA_ENDPOINT_FRAMEWORK_UUID, uuid);

	wiring_endpoint_listener_pt wEndpointListener = calloc(1,sizeof(struct wiring_endpoint_listener));

	if (!wEndpointListener) {
		return CELIX_ENOMEM;
	}

	wEndpointListener->handle=activator->node_discovery;
	wEndpointListener->wiringEndpointAdded=node_discovery_wiringEndpointAdded;
	wEndpointListener->wiringEndpointRemoved=node_discovery_wiringEndpointRemoved;
	activator->wiringEndpointListener=wEndpointListener;

	properties_pt props = properties_create();
	properties_set(props, "NODE_DISCOVERY", "true");
	properties_set(props, (char *) INAETICS_WIRING_ENDPOINT_LISTENER_SCOPE, scope);

	free(scope);

	status=bundleContext_registerService(context, (char *) INAETICS_WIRING_ENDPOINT_LISTENER_SERVICE, wEndpointListener, props, &activator->wiringEndpointListenerService);

	if(status == CELIX_SUCCESS){
		status=serviceTracker_open(activator->wiringEndpointListenerTracker);
	}

	if(status == CELIX_SUCCESS){
		status = node_discovery_start(activator->node_discovery);
	}

	return status;
}

celix_status_t bundleActivator_stop(void * userData, bundle_context_pt context) {
	celix_status_t status = CELIX_SUCCESS;
	struct activator *activator = userData;

	status=serviceTracker_close(activator->wiringEndpointListenerTracker);

	status=serviceRegistration_unregister(activator->wiringEndpointListenerService);

	if(status==CELIX_SUCCESS){
		free(activator->wiringEndpointListener);
	}

	status = node_discovery_stop(activator->node_discovery);

	return status;
}

celix_status_t bundleActivator_destroy(void * userData, bundle_context_pt context) {
	celix_status_t status = CELIX_SUCCESS;
	struct activator *activator = userData;

	status=serviceTracker_destroy(activator->wiringEndpointListenerTracker);

	status = node_discovery_destroy(activator->node_discovery);

	activator->wiringEndpointListener=NULL;
	activator->wiringEndpointListenerService=NULL;
	activator->wiringEndpointListenerTracker=NULL;
	activator->node_discovery = NULL;
	activator->context = NULL;

	free(activator);

	return status;
}
