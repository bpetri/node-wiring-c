
#ifndef ETCD_WATCHER_H_
#define ETCD_WATCHER_H_

#include "bundle_context.h"
#include "celix_errno.h"
#include "node_discovery.h"

typedef struct etcd_watcher *etcd_watcher_pt;

celix_status_t etcdWatcher_create(node_discovery_pt discovery,  bundle_context_pt context, etcd_watcher_pt *watcher);
celix_status_t etcdWatcher_destroy(etcd_watcher_pt watcher);


#endif /* ETCD_WATCHER_H_ */
