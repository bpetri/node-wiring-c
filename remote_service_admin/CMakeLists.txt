#
# Licensed under Apache License v2. See LICENSE for more information.
#

CELIX_ADD_COMPONENT(remote_service_admin
    DISPLAY_NAME remote_service_admin
    DESCRIPTION Public files needed for RSA implementations
    GROUP bundles
)
install (FILES 
        public/include/remote_endpoint_impl.h
        public/include/remote_endpoint.h
        public/include/remote_proxy.h
        public/include/remote_service_admin.h
        public/include/endpoint_description.h
        ../endpoint_listener/public/include/endpoint_listener.h
        ../utils/public/include/remote_constants.h
    DESTINATION 
        include/celix/remote_service_admin 
    COMPONENT 
        remote_service_admin
)
install (FILES
        private/src/remote_proxy_factory_impl.c
    DESTINATION
        share/celix/remote_service_admin
    COMPONENT
        remote_service_admin
)
install (FILES 
        ${PROJECT_SOURCE_DIR}/remote_services/endpoint_listener/public/include/endpoint_listener.h
    DESTINATION 
        include/celix/endpoint_listener 
    COMPONENT 
        remote_service_admin
)
