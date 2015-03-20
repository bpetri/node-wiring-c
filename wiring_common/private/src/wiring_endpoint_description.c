/**
 * Licensed under Apache License v2. See LICENSE for more information.
 */

#include <uuid/uuid.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "wiring_endpoint_description.h"
#include "remote_constants.h"

celix_status_t wiringEndpointDescription_create(char* wireId, properties_pt properties, wiring_endpoint_description_pt *wiringEndpointDescription) {
	celix_status_t status = CELIX_SUCCESS;

	*wiringEndpointDescription = calloc(1, sizeof(struct wiring_endpoint_description));

	if (*wiringEndpointDescription == NULL) {
		return CELIX_ENOMEM;
	}

	if (properties != NULL) {
		(*wiringEndpointDescription)->properties = properties;
	} else {
		properties_pt props = properties_create();
		if (props != NULL) {
			(*wiringEndpointDescription)->properties = props;
		} else {
			free(*wiringEndpointDescription);
			return CELIX_ENOMEM;
		}
	}

	if (wireId != NULL) {
		(*wiringEndpointDescription)->wireId = strdup(wireId);
	}
	else {
		char uuid[37];
		uuid_t uid;

		uuid_generate(uid);
		uuid_unparse(uid, &uuid[0]);

		(*wiringEndpointDescription)->wireId = strdup(&uuid[0]);
	}

	return status;
}

celix_status_t wiringEndpointDescription_destroy(wiring_endpoint_description_pt *description) {
	celix_status_t status = CELIX_SUCCESS;

	if ((*description)->wireId != NULL) {
		free((*description)->wireId);
	}

	if ((*description)->properties != NULL) {
		properties_destroy((*description)->properties);
	}

	free(*description);
	*description = NULL;

	return status;
}

unsigned int wiringEndpointDescription_hash(void* description) {

	wiring_endpoint_description_pt wepd = (wiring_endpoint_description_pt) description;

	if (wepd->wireId != NULL) {
		return (utils_stringHash(wepd->wireId));
	}

	return 0;

}

int wiringEndpointDescription_equals(void* description1, void* description2) {

	wiring_endpoint_description_pt wepd1 = (wiring_endpoint_description_pt) description1;
	wiring_endpoint_description_pt wepd2 = (wiring_endpoint_description_pt) description2;

	if (wepd1 == NULL || wepd2 == NULL) {
		return 1;
	}

	if ((wepd1->wireId == NULL) || (wepd2->wireId == NULL)) {
		return 1;
	}

	if (!strcmp(wepd1->wireId, wepd2->wireId)) {
		return 0;
	}

	return 1;

}

void wiringEndpointDescription_dump(wiring_endpoint_description_pt wep_desc) {
	printf("\t\t WEPD %s\n", wep_desc->wireId);

	hash_map_iterator_pt wep_desc_props_it = hashMapIterator_create(wep_desc->properties);

	while (hashMapIterator_hasNext(wep_desc_props_it)) {
		hash_map_entry_pt wep_desc_props_entry = hashMapIterator_nextEntry(wep_desc_props_it);
		char* key = (char*) hashMapEntry_getKey(wep_desc_props_entry);
		char* value = (char*) hashMapEntry_getValue(wep_desc_props_entry);
		printf("\t\t<%s=%s>\n", key, value);
	}

	hashMapIterator_destroy(wep_desc_props_it);
}
