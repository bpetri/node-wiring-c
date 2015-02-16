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
 * wiring_endpoint_listener.h
 *
 *  \date       Sep 29, 2011
 *  \author    	<a href="mailto:celix-dev@incubator.apache.org">Apache Celix Project Team</a>
 *  \copyright	Apache License, Version 2.0
 */

#ifndef WIRING_ENDPOINT_LISTENER_H_
#define WIRING_ENDPOINT_LISTENER_H_

#include "wiring_endpoint_description.h"

static const char * const INAETICS_WIRING_ENDPOINT_LISTENER_SERVICE = "wiring_endpoint_listener";

static const char * const INAETICS_WIRING_ENDPOINT_LISTENER_SCOPE = "wiring.endpoint.listener.scope";

struct wiring_endpoint_listener {
	void *handle;
	celix_status_t (*wiringEndpointAdded)(void *handle, wiring_endpoint_description_pt wEndpoint, char *matchedFilter);
	celix_status_t (*wiringEndpointRemoved)(void *handle, wiring_endpoint_description_pt wEndpoint, char *matchedFilter);
};

typedef struct wiring_endpoint_listener *wiring_endpoint_listener_pt;


#endif /* WIRING_ENDPOINT_LISTENER_H_ */
