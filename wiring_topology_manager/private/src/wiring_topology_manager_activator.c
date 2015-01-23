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
 * wiring_topology_manager_activator.c
 *
 *  \date       Sep 29, 2011
 *  \author    	<a href="mailto:celix-dev@incubator.apache.org">Apache Celix Project Team</a>
 *  \copyright	Apache License, Version 2.0
 */

#include <stdio.h>
#include <stdlib.h>

#include "constants.h"
#include "bundle_activator.h"
#include "service_tracker.h"
#include "service_registration.h"

#include "wiring_topology_manager.h"
#include "wiring_endpoint_listener.h"
#include "wiring_admin.h"
#include "remote_constants.h"


struct activator {
	bundle_context_pt context;

	wiring_topology_manager_pt manager;

	service_tracker_pt inaeticsWiringAdminTracker;

	wiring_endpoint_listener_pt wiringEndpointListener;
	service_registration_pt wiringEndpointListenerService;
};


static celix_status_t bundleActivator_createInaeticsWATracker(struct activator *activator, service_tracker_pt *tracker);


celix_status_t bundleActivator_create(bundle_context_pt context, void **userData) {
	celix_status_t status = CELIX_SUCCESS;
	struct activator *activator = NULL;

	activator = malloc(sizeof(struct activator));

	if (!activator) {
		return CELIX_ENOMEM;
	}

	activator->context = context;
	activator->wiringEndpointListenerService = NULL;
	activator->manager = NULL;
	activator->inaeticsWiringAdminTracker = NULL;

	status = wiringTopologyManager_create(context, &activator->manager);
	if (status == CELIX_SUCCESS) {
		status = bundleActivator_createInaeticsWATracker(activator, &(activator->inaeticsWiringAdminTracker));

		if (status == CELIX_SUCCESS) {
			*userData = activator;
		}

	}

	return status;
}

static celix_status_t bundleActivator_createInaeticsWATracker(struct activator *activator, service_tracker_pt *tracker) {
	celix_status_t status = CELIX_SUCCESS;

	service_tracker_customizer_pt customizer = NULL;

	status = serviceTrackerCustomizer_create(activator->manager, wiringTopologyManager_waAdding,
			wiringTopologyManager_waAdded, wiringTopologyManager_waModified, wiringTopologyManager_waRemoved, &customizer);

	if (status == CELIX_SUCCESS) {
		status = serviceTracker_create(activator->context, OSGI_WIRING_ADMIN, customizer, tracker);
	}

	return status;
}


celix_status_t bundleActivator_start(void * userData, bundle_context_pt context) {
	celix_status_t status = CELIX_SUCCESS;
	struct activator *activator = userData;

	wiring_endpoint_listener_pt wEndpointListener = malloc(sizeof(*wEndpointListener));
	wEndpointListener->handle = activator->manager;
	wEndpointListener->wiringEndpointAdded = wiringTopologyManager_addImportedWiringEndpoint;
	wEndpointListener->wiringEndpointRemoved = wiringTopologyManager_removeImportedWiringEndpoint;
	activator->wiringEndpointListener = wEndpointListener;

	char *uuid = NULL;
	status = bundleContext_getProperty(activator->context, (char *)OSGI_FRAMEWORK_FRAMEWORK_UUID, &uuid);
	if (!uuid) {
		printf("WIRING_TOPOLOGY_MANAGER: no framework UUID defined?!\n");
		return CELIX_ILLEGAL_STATE;
	}

	size_t len = 14 + strlen(OSGI_FRAMEWORK_OBJECTCLASS) + strlen(OSGI_RSA_ENDPOINT_FRAMEWORK_UUID) + strlen(uuid);
	char *scope = calloc(len+1,sizeof(char));
	if (!scope) {
		return CELIX_ENOMEM;
	}

	snprintf(scope, len, "(&(%s=*)(!(%s=%s)))", OSGI_FRAMEWORK_OBJECTCLASS, OSGI_RSA_ENDPOINT_FRAMEWORK_UUID, uuid);

	printf("WIRING_TOPOLOGY_MANAGER: endpoint listener scope is %s\n", scope);

	properties_pt props = properties_create();
	properties_set(props, (char *) OSGI_WIRING_ENDPOINT_LISTENER_SCOPE, scope);

	// We can release the scope, as properties_set makes a copy of the key & value...
	free(scope);

	bundleContext_registerService(context, (char *) OSGI_WIRING_ENDPOINT_LISTENER_SERVICE, wEndpointListener, props, &activator->wiringEndpointListenerService);

	serviceTracker_open(activator->inaeticsWiringAdminTracker);

	return status;
}

celix_status_t bundleActivator_stop(void * userData, bundle_context_pt context) {
	celix_status_t status = CELIX_SUCCESS;
	struct activator *activator = userData;

	serviceTracker_close(activator->inaeticsWiringAdminTracker);
	serviceTracker_destroy(activator->inaeticsWiringAdminTracker);


	serviceRegistration_unregister(activator->wiringEndpointListenerService);
	free(activator->wiringEndpointListener);

	return status;
}

celix_status_t bundleActivator_destroy(void * userData, bundle_context_pt context) {
	celix_status_t status = CELIX_SUCCESS;

	struct activator *activator = userData;
	if (!activator || !activator->manager) {
		status = CELIX_BUNDLE_EXCEPTION;
	}
	else {
		status = wiringTopologyManager_destroy(activator->manager);
		free(activator);
	}

	return status;
}
