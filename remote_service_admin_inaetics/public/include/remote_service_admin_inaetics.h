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
 * remote_service_admin_inaetics.h
 *
 *  \date       Apr 08, 2015
 *  \author    	<a href="mailto:dev@celix.apache.org">Apache Celix Project Team</a>
 *  \copyright	Apache License, Version 2.0
 */

#ifndef REMOTE_SERVICE_ADMIN_INAETICS_H_
#define REMOTE_SERVICE_ADMIN_INAETICS_H_

#include "celix_errno.h"

static const char * const INAETICS_WIRING_RECEIVE_SERVICE = "wiring_receive";

struct wiring_receive_service {
	char* wireId;
	void* handle;
	celix_status_t (*receive)(void* handle, char* data, char** response);
};

typedef struct wiring_receive_service *wiring_receive_service_pt;


#endif /* REMOTE_SERVICE_ADMIN_HTTP_IMPL_H_ */
