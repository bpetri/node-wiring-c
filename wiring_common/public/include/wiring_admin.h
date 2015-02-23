/**
 * Licensed under Apache License v2. See LICENSE for more information.
 */

#ifndef WIRING_ADMIN_H_
#define WIRING_ADMIN_H_

#include "wiring_endpoint_listener.h"
#include "celix_errno.h"

static const char * const INAETICS_WIRING_ADMIN = "wiring_admin";

static const char * const INAETICS_WIRING_ADMIN_SCOPE = "wiring.admin.scope";

#define DEFAULT_WA_ADDRESS 	"127.0.0.1"
#define DEFAULT_WA_PORT		"6789"

#define NODE_DISCOVERY_NODE_WA_ADDRESS	"NODE_DISCOVERY_NODE_WA_ADDRESS"
#define NODE_DISCOVERY_NODE_WA_PORT		"NODE_DISCOVERY_NODE_WA_PORT"
#define NODE_DISCOVERY_NODE_WA_ITF 		"NODE_DISCOVERY_NODE_WA_ITF"

#define WIRING_ENDPOINT_DESCRIPTION_PROTOCOL_KEY 		"ep_protocol"

typedef struct wiring_admin *wiring_admin_pt;

typedef celix_status_t (*rsa_inaetics_receive_cb)(char* data, char**response);
typedef celix_status_t (*rsa_inaetics_send)(wiring_admin_pt admin,void* handle, char *request, char **reply, int* replyStatus);

typedef void* wiring_handle;

struct wiring_admin_service {
	wiring_admin_pt admin;

	celix_status_t (*exportWiringEndpoint)(wiring_admin_pt admin, rsa_inaetics_receive_cb rsa_inaetics_cb);
	celix_status_t (*removeExportedWiringEndpoint)(wiring_admin_pt admin, rsa_inaetics_receive_cb rsa_inaetics_cb);
	celix_status_t (*getWiringEndpoint)(wiring_admin_pt admin,wiring_endpoint_description_pt* wEndpoint);

	celix_status_t (*importWiringEndpoint)(wiring_admin_pt admin, wiring_endpoint_description_pt wEndpoint,rsa_inaetics_send* sendFunc,wiring_handle* handle);
	celix_status_t (*removeImportedWiringEndpoint)(wiring_admin_pt admin, wiring_handle handle);

};

typedef struct wiring_admin_service *wiring_admin_service_pt;


#endif /* WIRING_ADMIN_H_ */
