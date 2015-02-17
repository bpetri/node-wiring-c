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
 * remote_service_admin_activator.c
 *
 *  \date       Sep 30, 2011
 *  \author    	<a href="mailto:celix-dev@incubator.apache.org">Apache Celix Project Team</a>
 *  \copyright	Apache License, Version 2.0
 */
#include <stdlib.h>

#include "bundle_activator.h"
#include "service_registration.h"

#include "wiring_admin_impl.h"

struct activator {
	bundle_context_pt context;
	wiring_admin_pt admin;
	wiring_admin_service_pt wiringAdminService;
	service_registration_pt registration;
};

celix_status_t bundleActivator_create(bundle_context_pt context, void **userData) {
	celix_status_t status = CELIX_SUCCESS;
	struct activator *activator;

	activator = calloc(1, sizeof(*activator));
	if (!activator) {
		status = CELIX_ENOMEM;
	} else {
		activator->context = context;
		activator->admin = NULL;
		activator->registration = NULL;
		activator->wiringAdminService = NULL;

		*userData = activator;
	}

	return status;
}

celix_status_t bundleActivator_start(void * userData, bundle_context_pt context) {
	celix_status_t status = CELIX_SUCCESS;
	struct activator *activator = userData;

	status = wiringAdmin_create(context, &activator->admin);
	if (status == CELIX_SUCCESS) {

		activator->wiringAdminService = calloc(1, sizeof(struct wiring_admin_service));
		if (!activator->wiringAdminService) {
			status = CELIX_ENOMEM;
		} else {
			activator->wiringAdminService->admin = activator->admin;

			activator->wiringAdminService->exportWiringEndpoint = wiringAdmin_exportWiringEndpoint;
			activator->wiringAdminService->removeExportedWiringEndpoint = wiringAdmin_removeExportedWiringEndpoint;
			activator->wiringAdminService->getWiringEndpoint = wiringAdmin_getWiringEndpoint;

			activator->wiringAdminService->importWiringEndpoint = wiringAdmin_importWiringEndpoint;
			activator->wiringAdminService->removeImportedWiringEndpoint = wiringAdmin_removeImportedWiringEndpoint;

			char *uuid = NULL;
			status = bundleContext_getProperty(activator->context, (char *)OSGI_FRAMEWORK_FRAMEWORK_UUID, &uuid);
			if (!uuid) {
				printf("WA: no framework UUID defined?!\n");
				return CELIX_ILLEGAL_STATE;
			}

			size_t len = 14 + strlen(OSGI_FRAMEWORK_OBJECTCLASS) + strlen(OSGI_RSA_ENDPOINT_FRAMEWORK_UUID) + strlen(uuid);
			char *scope = calloc(len+1,sizeof(char));
			if (!scope) {
				return CELIX_ENOMEM;
			}

			snprintf(scope, len, "(%s=%s)", OSGI_RSA_ENDPOINT_FRAMEWORK_UUID, uuid);

			//printf("WA: Wiring Endpoint Listener scope is %s\n", scope);

			properties_pt props = properties_create();
			properties_set(props, (char *) INAETICS_WIRING_ADMIN_SCOPE, scope);

			free(scope);


			status = bundleContext_registerService(context, (char*)INAETICS_WIRING_ADMIN, activator->wiringAdminService, props, &activator->registration);
		}
	}

	return status;
}

celix_status_t bundleActivator_stop(void * userData, bundle_context_pt context) {
	celix_status_t status = CELIX_SUCCESS;
	struct activator *activator = userData;

	wiringAdmin_stop(activator->admin);
	serviceRegistration_unregister(activator->registration);
	activator->registration = NULL;

	free(activator->wiringAdminService);
	activator->wiringAdminService = NULL;


	return status;
}

celix_status_t bundleActivator_destroy(void * userData, bundle_context_pt context) {
	celix_status_t status = CELIX_SUCCESS;
	struct activator *activator = userData;

	status = wiringAdmin_destroy(&activator->admin);

	free(activator);

	return status;
}


