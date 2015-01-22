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
 * remote_service_admin.h
 *
 *  \date       Sep 30, 2011
 *  \author    	<a href="mailto:celix-dev@incubator.apache.org">Apache Celix Project Team</a>
 *  \copyright	Apache License, Version 2.0
 */

#ifndef WIRING_ADMIN_H_
#define WIRING_ADMIN_H_

#include "wiring_endpoint_listener.h"

#define OSGI_WIRING_ADMIN "wiring_admin"

typedef struct wiring_export_registration *wiring_export_registration_pt;
typedef struct wiring_import_registration *wiring_import_registration_pt;

typedef struct wiring_export_reference *wiring_export_reference_pt;
typedef struct wiring_import_reference *wiring_import_reference_pt;

typedef struct wiring_admin *wiring_admin_pt;

typedef struct import_registration_factory *import_registration_factory_pt;


struct wiring_admin_service {
	wiring_admin_pt admin;

	celix_status_t (*exportWiringEndpoint)(wiring_admin_pt admin, char *serviceId, properties_pt properties, array_list_pt *registrations);
	celix_status_t (*importWiringEndpoint)(wiring_admin_pt admin, wiring_endpoint_description_pt endpoint, wiring_import_registration_pt *registration);

	celix_status_t (*wiringExportRegistration_close)(wiring_export_registration_pt registration);
	celix_status_t (*wiringImportRegistration_close)(wiring_admin_pt admin, wiring_import_registration_pt registration);

	celix_status_t (*wiringExportRegistration_getWiringExportReference)(wiring_export_registration_pt registration, wiring_export_reference_pt *reference);
	celix_status_t (*wiringExportReference_getExportedWiringEndpoint)(wiring_export_reference_pt reference, wiring_endpoint_description_pt *endpoint);

	/*
	celix_status_t (*removeExportedService)(export_registration_pt registration);
	celix_status_t (*getExportedServices)(wiring_admin_pt admin, array_list_pt *services);
	celix_status_t (*getImportedEndpoints)(wiring_admin_pt admin, array_list_pt *services);



	celix_status_t (*exportReference_getExportedService)(export_reference_pt reference);


	celix_status_t (*exportRegistration_getException)(export_registration_pt registration);

	celix_status_t (*exportRegistration_freeExportReference)(export_reference_pt *reference);
	celix_status_t (*exportRegistration_getEndpointDescription)(export_registration_pt registration, wiring_endpoint_description_pt endpointDescription);

	celix_status_t (*importReference_getImportedEndpoint)(import_reference_pt reference);
	celix_status_t (*importReference_getImportedService)(import_reference_pt reference);

	celix_status_t (*importRegistration_close)(wiring_admin_pt admin, import_registration_pt registration);
	celix_status_t (*importRegistration_getException)(import_registration_pt registration);
	celix_status_t (*importRegistration_getImportReference)(import_registration_pt registration, import_reference_pt *reference);
	*/

};

typedef struct wiring_admin_service *wiring_admin_service_pt;


#endif /* WIRING_ADMIN_H_ */
