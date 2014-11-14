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

#include "celix_log.h"
#include "node_discovery.h"
#include "node_discovery_impl.h"


struct activator {
	bundle_context_pt context;
	node_discovery_pt node_discovery;
};


celix_status_t bundleActivator_create(bundle_context_pt context, void **userData) {
	celix_status_t status = CELIX_SUCCESS;

	struct activator* activator = calloc(1, sizeof(*activator));

	if (!activator) {
		return CELIX_ENOMEM;
	}

	status = node_discovery_create(context, &activator->node_discovery);

	if (status == CELIX_SUCCESS) {
		activator->context = context;
		*userData = activator;
	}

	return status;
}

celix_status_t bundleActivator_start(void * userData, bundle_context_pt context) {
	celix_status_t status = CELIX_SUCCESS;
	struct activator *activator = userData;

	status = node_discovery_start(activator->node_discovery);

	return status;
}

celix_status_t bundleActivator_stop(void * userData, bundle_context_pt context) {
	celix_status_t status = CELIX_SUCCESS;
	struct activator *activator = userData;

	status = node_discovery_stop(activator->node_discovery);

	return status;
}

celix_status_t bundleActivator_destroy(void * userData, bundle_context_pt context) {
	celix_status_t status = CELIX_SUCCESS;
	struct activator *activator = userData;

	status = node_discovery_destroy(activator->node_discovery);

	activator->node_discovery = NULL;
	activator->context = NULL;

	free(activator);

	return status;
}
