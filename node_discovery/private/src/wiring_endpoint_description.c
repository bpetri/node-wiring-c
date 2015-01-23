/*
 * wiring_endpoint_description.c

 *
 *  Created on: Jan 23, 2015
 *      Author: dn234
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wiring_endpoint_description.h"

celix_status_t wiringEndpointDescription_create(char* uuid,properties_pt properties, wiring_endpoint_description_pt *wiringEndpointDescription){
	celix_status_t status = CELIX_SUCCESS;

	*wiringEndpointDescription = calloc(1,sizeof(struct wiring_endpoint_description));

	if(properties!=NULL){
		(*wiringEndpointDescription)->properties=properties;
	}
	else{
		(*wiringEndpointDescription)->properties=properties_create();
	}

	if(uuid!=NULL){
		(*wiringEndpointDescription)->frameworkUUID=strdup(uuid);
	}
	else{
		(*wiringEndpointDescription)->frameworkUUID=NULL;
	}

	(*wiringEndpointDescription)->url=NULL;
	(*wiringEndpointDescription)->port=0;


	return status;
}

celix_status_t wiringEndpointDescription_destroy(wiring_endpoint_description_pt description){
	celix_status_t status = CELIX_SUCCESS;

	if(description->frameworkUUID!=NULL){
		free(description->frameworkUUID);
	}

	if(description->properties!=NULL){
		properties_destroy(description->properties);
	}

	if(description->url!=NULL){
		free(description->url);
	}

	free(description);

	return status;
}

