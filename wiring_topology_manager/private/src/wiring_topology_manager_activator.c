/**
 * Licensed under Apache License v2. See LICENSE for more information.
 */

#include <stdio.h>
#include <stdlib.h>

#include "constants.h"
#include "bundle_activator.h"
#include "service_tracker.h"
#include "service_registration.h"

#include "wiring_topology_manager_impl.h"
#include "wiring_endpoint_listener.h"
#include "wiring_admin.h"
#include "remote_constants.h"

struct activator {
    bundle_context_pt context;

    wiring_topology_manager_pt manager;

    service_tracker_pt inaeticsWiringAdminTracker;
    service_tracker_pt endpointListenerTracker;

    wiring_endpoint_listener_pt wiringEndpointListener;
    service_registration_pt wiringEndpointListenerService;

    wiring_topology_manager_service_pt wiringTopologyManagerService;
    service_registration_pt wiringTopologyManagerServiceRegistration;
};

static celix_status_t bundleActivator_createEPLTracker(struct activator *activator, service_tracker_pt *tracker);
static celix_status_t bundleActivator_createInaeticsWATracker(struct activator *activator, service_tracker_pt *tracker);

celix_status_t bundleActivator_create(bundle_context_pt context, void **userData) {
    celix_status_t status = CELIX_SUCCESS;
    struct activator *activator = NULL;

    activator = calloc(1, sizeof(struct activator));

    if (!activator) {
        return CELIX_ENOMEM;
    }
    activator->context = context;
    activator->manager = NULL;
    activator->inaeticsWiringAdminTracker = NULL;
    activator->wiringEndpointListener = NULL;
    activator->wiringEndpointListenerService = NULL;
    activator->wiringTopologyManagerService = NULL;
    activator->wiringTopologyManagerServiceRegistration = NULL;

    status = wiringTopologyManager_create(context, &activator->manager);
    if (status == CELIX_SUCCESS) {
        status = bundleActivator_createEPLTracker(activator, &activator->endpointListenerTracker);
        if (status == CELIX_SUCCESS) {
            status = bundleActivator_createInaeticsWATracker(activator, &(activator->inaeticsWiringAdminTracker));

            if (status == CELIX_SUCCESS) {
                *userData = activator;
            }

        }
    }

    return status;
}

static celix_status_t bundleActivator_createEPLTracker(struct activator *activator, service_tracker_pt *tracker) {
    celix_status_t status;

    service_tracker_customizer_pt customizer = NULL;

    status = serviceTrackerCustomizer_create(activator->manager, wiringTopologyManager_wiringEndpointListenerAdding, wiringTopologyManager_wiringEndpointListenerAdded,
            wiringTopologyManager_wiringEndpointListenerModified, wiringTopologyManager_wiringEndpointListenerRemoved, &customizer);

    if (status == CELIX_SUCCESS) {
        status = serviceTracker_create(activator->context, (char *) INAETICS_WIRING_ENDPOINT_LISTENER_SERVICE, customizer, tracker);
    }

    return status;
}

static celix_status_t bundleActivator_createInaeticsWATracker(struct activator *activator, service_tracker_pt *tracker) {
    celix_status_t status = CELIX_SUCCESS;

    service_tracker_customizer_pt customizer = NULL;

    status = serviceTrackerCustomizer_create(activator->manager, wiringTopologyManager_waAdding, wiringTopologyManager_waAdded, wiringTopologyManager_waModified, wiringTopologyManager_waRemoved,
            &customizer);

    if (status == CELIX_SUCCESS) {
        status = serviceTracker_create(activator->context, (char*) INAETICS_WIRING_ADMIN, customizer, tracker);
    }

    return status;
}

celix_status_t bundleActivator_start(void * userData, bundle_context_pt context) {
    celix_status_t status = CELIX_SUCCESS;
    struct activator *activator = userData;

    /* Wiring Endpoint Listener Service Creation and Registration */
    wiring_endpoint_listener_pt wEndpointListener = calloc(1, sizeof(*wEndpointListener));
    if (wEndpointListener == NULL) {
        serviceTracker_destroy(activator->inaeticsWiringAdminTracker);
        bundleActivator_destroy(userData, context);
        return CELIX_ENOMEM;
    }
    wEndpointListener->handle = activator->manager;
    wEndpointListener->wiringEndpointAdded = wiringTopologyManager_WiringEndpointAdded;
    wEndpointListener->wiringEndpointRemoved = wiringTopologyManager_WiringEndpointRemoved;
    activator->wiringEndpointListener = wEndpointListener;

    char *uuid = NULL;
    status = bundleContext_getProperty(activator->context, (char *) OSGI_FRAMEWORK_FRAMEWORK_UUID, &uuid);
    if (!uuid) {
        printf("WTM: no framework UUID defined?!\n");
        return CELIX_ILLEGAL_STATE;
    }

    size_t len = 14 + strlen(OSGI_FRAMEWORK_OBJECTCLASS) + strlen(OSGI_RSA_ENDPOINT_FRAMEWORK_UUID) + strlen(uuid);
    char scope[len + 1];

    snprintf(scope, len, "(!(%s=%s))", OSGI_RSA_ENDPOINT_FRAMEWORK_UUID, uuid);

    properties_pt props = properties_create();
    properties_set(props, (char *) INAETICS_WIRING_ENDPOINT_LISTENER_SCOPE, scope);

    bundleContext_registerService(context, (char *) INAETICS_WIRING_ENDPOINT_LISTENER_SERVICE, wEndpointListener, props, &activator->wiringEndpointListenerService);

    /* Wiring Topology Manager Service Creation and Registration */
    wiring_topology_manager_service_pt wiringTopologyManagerService = calloc(1, sizeof(*wiringTopologyManagerService));
    wiringTopologyManagerService->manager = activator->manager;
    wiringTopologyManagerService->exportWiringEndpoint = wiringTopologyManager_exportWiringEndpoint;
    wiringTopologyManagerService->removeExportedWiringEndpoint = wiringTopologyManager_removeExportedWiringEndpoint;
    wiringTopologyManagerService->importWiringEndpoint = wiringTopologyManager_importWiringEndpoint;
    wiringTopologyManagerService->removeImportedWiringEndpoint = wiringTopologyManager_removeImportedWiringEndpoint;

    activator->wiringTopologyManagerService = wiringTopologyManagerService;

    snprintf(scope, len, "(%s=%s)", OSGI_RSA_ENDPOINT_FRAMEWORK_UUID, uuid);

    properties_pt wtm_props = properties_create();
    properties_set(wtm_props, (char *) INAETICS_WIRING_TOPOLOGY_MANAGER_SCOPE, scope);

    bundleContext_registerService(context, (char *) INAETICS_WIRING_TOPOLOGY_MANAGER_SERVICE, wiringTopologyManagerService, wtm_props, &activator->wiringTopologyManagerServiceRegistration);


    if (status == CELIX_SUCCESS) {
        serviceTracker_open(activator->inaeticsWiringAdminTracker);
    }

    if (status == CELIX_SUCCESS) {
        printf("WTM: endpointListenerTracker initiated.\n");
        status = serviceTracker_open(activator->endpointListenerTracker);
    }

    return status;
}

celix_status_t bundleActivator_stop(void * userData, bundle_context_pt context) {
    celix_status_t status = CELIX_SUCCESS;
    struct activator *activator = userData;

    if (serviceTracker_close(activator->endpointListenerTracker) == CELIX_SUCCESS) {
        serviceTracker_destroy(activator->endpointListenerTracker);
    }

    if (serviceTracker_close(activator->inaeticsWiringAdminTracker) == CELIX_SUCCESS) {
        serviceTracker_destroy(activator->inaeticsWiringAdminTracker);
    }

    serviceRegistration_unregister(activator->wiringEndpointListenerService);
    free(activator->wiringEndpointListener);

    serviceRegistration_unregister(activator->wiringTopologyManagerServiceRegistration);
    free(activator->wiringTopologyManagerService);

    return status;
}

celix_status_t bundleActivator_destroy(void * userData, bundle_context_pt context) {
    celix_status_t status = CELIX_SUCCESS;

    struct activator *activator = userData;
    if (!activator || !activator->manager) {
        status = CELIX_BUNDLE_EXCEPTION;
    } else {
        status = wiringTopologyManager_destroy(activator->manager);
        free(activator);
    }

    return status;
}
