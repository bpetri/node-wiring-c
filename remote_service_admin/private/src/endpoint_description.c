/**
 * Licensed under Apache License v2. See LICENSE for more information.
 */

#include <stdlib.h>
#include <stdio.h>

#include "celix_errno.h"
#include "celix_log.h"

#include "endpoint_description.h"
#include "remote_constants.h"
#include "constants.h"

static celix_status_t endpointDescription_verifyLongProperty(properties_pt properties, char *propertyName, long *longProperty);

celix_status_t endpointDescription_create(properties_pt properties, endpoint_description_pt *endpointDescription) {
	celix_status_t status = CELIX_SUCCESS;

    *endpointDescription = malloc(sizeof(**endpointDescription));

    long serviceId = 0L;
    status = endpointDescription_verifyLongProperty(properties, (char *) OSGI_RSA_ENDPOINT_SERVICE_ID, &serviceId);
    if (status != CELIX_SUCCESS) {
    	return status;
    }

    (*endpointDescription)->properties = properties;
    (*endpointDescription)->frameworkUUID = properties_get(properties, (char *) OSGI_RSA_ENDPOINT_FRAMEWORK_UUID);
    (*endpointDescription)->id = properties_get(properties, (char *) OSGI_RSA_ENDPOINT_ID);
    (*endpointDescription)->service = properties_get(properties, (char *) OSGI_FRAMEWORK_OBJECTCLASS);
    (*endpointDescription)->serviceId = serviceId;

    if (!(*endpointDescription)->frameworkUUID || !(*endpointDescription)->id || !(*endpointDescription)->service) {
    	fw_log(logger, OSGI_FRAMEWORK_LOG_ERROR, "ENDPOINT_DESCRIPTION: incomplete description!.");
    	status = CELIX_BUNDLE_EXCEPTION;
    }

    return status;
}

celix_status_t endpointDescription_destroy(endpoint_description_pt description) {
    properties_destroy(description->properties);
    free(description);
    return CELIX_SUCCESS;
}

static celix_status_t endpointDescription_verifyLongProperty(properties_pt properties, char *propertyName, long *longProperty) {
    celix_status_t status = CELIX_SUCCESS;

    char *value = properties_get(properties, propertyName);
    if (value == NULL) {
        *longProperty = 0l;
    } else {
        *longProperty = atol(value);
    }

    return status;
}
