
#include <stdbool.h>
#include <stdlib.h>

#include "celix_log.h"
#include "constants.h"
#include "node_discovery.h"
#include "node_discovery_impl.h"
#include "node_description_writer.h"

#include "etcd.h"
#include "etcd_watcher.h"

struct etcd_watcher {
    node_discovery_pt node_discovery;

	celix_thread_mutex_t watcherLock;
	celix_thread_t watcherThread;

	volatile bool running;
};

#define MAX_ROOTNODE_LENGTH		 64
#define MAX_LOCALNODE_LENGTH	256

#define CFG_ETCD_ROOT_PATH		"NODE_DISCOVERY_ETCD_ROOT_PATH"
#define DEFAULT_ETCD_ROOTPATH	"node_discovery"

#define CFG_ETCD_SERVER_IP		"NODE_DISCOVERY_ETCD_SERVER_IP"
#define DEFAULT_ETCD_SERVER_IP	"127.0.0.1"

#define CFG_ETCD_SERVER_PORT	"NODE_DISCOVERY_ETCD_SERVER_PORT"
#define DEFAULT_ETCD_SERVER_PORT 4001

// be careful - this should be higher than the curl timeout
#define CFG_ETCD_TTL   "DISCOVERY_ETCD_TTL"
#define DEFAULT_ETCD_TTL 30


// note that the rootNode shouldn't have a leading slash
static celix_status_t etcdWatcher_getRootPath(bundle_context_pt context, char* rootNode) {
	celix_status_t status = CELIX_SUCCESS;
	char* rootPath = NULL;

	if (((bundleContext_getProperty(context, CFG_ETCD_ROOT_PATH, &rootPath)) != CELIX_SUCCESS) || (!rootPath)) {
		strcpy(rootNode, DEFAULT_ETCD_ROOTPATH);
	}
	else {
		strcpy(rootNode, rootPath);
	}

	return status;
}


static celix_status_t etcdWatcher_getLocalNodePath(bundle_context_pt context, char* localNodePath) {
	celix_status_t status = CELIX_SUCCESS;
	char rootPath[MAX_ROOTNODE_LENGTH];
    char* uuid = NULL;

    if ((etcdWatcher_getRootPath(context, &rootPath[0]) != CELIX_SUCCESS)) {
		status = CELIX_ILLEGAL_STATE;
    }
	else if (((bundleContext_getProperty(context, OSGI_FRAMEWORK_FRAMEWORK_UUID, &uuid)) != CELIX_SUCCESS) || (!uuid)) {
		status = CELIX_ILLEGAL_STATE;
	}
	else if (rootPath[strlen(&rootPath[0]) - 1] == '/') {
    	snprintf(localNodePath, MAX_LOCALNODE_LENGTH, "%s%s", &rootPath[0], uuid);
    }
    else {
    	snprintf(localNodePath, MAX_LOCALNODE_LENGTH, "%s/%s", &rootPath[0], uuid);
    }

    return status;
}


static celix_status_t etcdWatcher_addAlreadyExistingNodes(node_discovery_pt node_discovery, int* highestModified) {
	celix_status_t status = CELIX_SUCCESS;
	char** nodeArr = calloc(MAX_NODES, sizeof(*nodeArr));
	char rootPath[MAX_ROOTNODE_LENGTH];
	int i, size;

	*highestModified = -1;

	for (i = 0; i < MAX_NODES; i++) {
		nodeArr[i] = calloc(MAX_KEY_LENGTH, sizeof(*nodeArr[i]));
	}

	// we need to go though all nodes and get the highest modifiedIndex
	if (((status = etcdWatcher_getRootPath(node_discovery->context, &rootPath[0])) == CELIX_SUCCESS) &&
		 (etcd_getNodes(rootPath, nodeArr, &size) == true)) {
		for (i = 0; i < size; i++) {
			char* key = nodeArr[i];
			char value[MAX_VALUE_LENGTH];
			char action[MAX_VALUE_LENGTH];
			int modIndex;

			if (etcd_get(key, &value[0], &action[0], &modIndex) == true) {
				node_discovery_addNode(node_discovery, key, &value[0]);

				if (modIndex > *highestModified) {
					*highestModified = modIndex;
				}
			}
		}
	}

	for (i = 0; i < MAX_NODES; i++) {
		free(nodeArr[i]);
	}

	free(nodeArr);

	return status;
}


static celix_status_t etcdWatcher_addOwnNode(etcd_watcher_pt watcher)
{
    celix_status_t status = CELIX_BUNDLE_EXCEPTION;
    char localNodePath[MAX_LOCALNODE_LENGTH];
    char value[MAX_VALUE_LENGTH];
    char action[MAX_VALUE_LENGTH];
    int modIndex;
    char* ttlStr = NULL;
    int ttl;

    node_discovery_pt node_discovery = watcher->node_discovery;
	bundle_context_pt context = node_discovery->context;
	node_description_pt ownNodeDescription = node_discovery->ownNode;
	char* nodeDescStr = NULL;

	// register own framework
    if ((status = etcdWatcher_getLocalNodePath(context, &localNodePath[0])) != CELIX_SUCCESS) {
        return status;
    }

    if (node_description_writer_nodeDescToString(ownNodeDescription, &nodeDescStr) != CELIX_SUCCESS) {
        fw_log(logger, OSGI_FRAMEWORK_LOG_WARNING, "Error while converting node description");
    }

    if ((bundleContext_getProperty(context, CFG_ETCD_TTL, &ttlStr) != CELIX_SUCCESS) || !ttlStr) {
        ttl = DEFAULT_ETCD_TTL;
    }
    else
    {
        char* endptr = ttlStr;
        errno = 0;
        ttl =  strtol(ttlStr, &endptr, 10);
        if (*endptr || errno != 0) {
            ttl = DEFAULT_ETCD_TTL;
        }
    }

	if (etcd_get(localNodePath, &value[0], &action[0], &modIndex) != true) {
		etcd_set(localNodePath, nodeDescStr, ttl, false);
	}
	else if (etcd_set(localNodePath, nodeDescStr, ttl, true) == false)  {
        fw_log(logger, OSGI_FRAMEWORK_LOG_WARNING, "Cannot register local discovery");
    }
    else {
        status = CELIX_SUCCESS;
    }

	free(nodeDescStr);

    return status;
}

/*
 * performs (blocking) etcd_watch calls to check for
 * changing discovery endpoint information within etcd.
 */
static void* etcdWatcher_run(void* data) {
	etcd_watcher_pt watcher = (etcd_watcher_pt) data;
	time_t timeBeforeWatch = time(NULL);
	static char rootPath[MAX_ROOTNODE_LENGTH];
	int highestModified = 0;

	node_discovery_pt node_discovery = watcher->node_discovery;
	bundle_context_pt context = node_discovery->context;

	etcdWatcher_addAlreadyExistingNodes(node_discovery, &highestModified);
	etcdWatcher_getRootPath(context, &rootPath[0]);

	while (watcher->running) {

		char rkey[MAX_KEY_LENGTH];
		char value[MAX_VALUE_LENGTH];
		char preValue[MAX_VALUE_LENGTH];
		char action[MAX_ACTION_LENGTH];

		if (etcd_watch(rootPath, highestModified+1, &action[0], &preValue[0], &value[0], &rkey[0]) == true) {
			if (strcmp(action, "set") == 0) {
				node_discovery_addNode(node_discovery, &rkey[0], &value[0]);
			} else if (strcmp(action, "delete") == 0) {
				node_discovery_removeNode(node_discovery, &rkey[0]);
			} else if (strcmp(action, "expire") == 0) {
				node_discovery_removeNode(node_discovery, &rkey[0]);
			} else if (strcmp(action, "update") == 0) {
				// TODO
			} else {
				fw_log(logger, OSGI_FRAMEWORK_LOG_INFO, "Unexpected action: %s", action);
			}
			highestModified++;
		}

		// update own framework uuid
		if (time(NULL) - timeBeforeWatch > (DEFAULT_ETCD_TTL/2)) {
			etcdWatcher_addOwnNode(watcher);
			timeBeforeWatch = time(NULL);
		}
	}

	return NULL;
}

celix_status_t etcdWatcher_create(node_discovery_pt node_discovery, bundle_context_pt context,
		etcd_watcher_pt *watcher)
{
	celix_status_t status = CELIX_SUCCESS;

	char* etcd_server = NULL;
	char* etcd_port_string = NULL;
	int etcd_port = 0;

	if (node_discovery == NULL) {
		return CELIX_BUNDLE_EXCEPTION;
	}

	(*watcher) = calloc(1, sizeof(struct etcd_watcher));
	if (!watcher) {
		return CELIX_ENOMEM;
	}
	else
	{
		(*watcher)->node_discovery = node_discovery;
	}

	if ((bundleContext_getProperty(context, CFG_ETCD_SERVER_IP, &etcd_server) != CELIX_SUCCESS) || !etcd_server) {
		etcd_server = DEFAULT_ETCD_SERVER_IP;
	}

	if ((bundleContext_getProperty(context, CFG_ETCD_SERVER_PORT, &etcd_port_string) != CELIX_SUCCESS) || !etcd_port_string) {
		etcd_port = DEFAULT_ETCD_SERVER_PORT;
	}
	else
	{
		char* endptr = etcd_port_string;
		errno = 0;
		etcd_port =  strtol(etcd_port_string, &endptr, 10);
		if (*endptr || errno != 0) {
			etcd_port = DEFAULT_ETCD_SERVER_PORT;
		}
	}

	if (etcd_init(etcd_server, etcd_port) == false)
	{
		return CELIX_BUNDLE_EXCEPTION;
	}

	etcdWatcher_addOwnNode(*watcher);

	if ((status = celixThreadMutex_create(&(*watcher)->watcherLock, NULL)) != CELIX_SUCCESS) {
		return status;
	}

	if ((status = celixThreadMutex_lock(&(*watcher)->watcherLock)) != CELIX_SUCCESS) {
		return status;
	}

	if ((status = celixThread_create(&(*watcher)->watcherThread, NULL, etcdWatcher_run, *watcher)) != CELIX_SUCCESS) {
		return status;
	}

	(*watcher)->running = true;

	if ((status = celixThreadMutex_unlock(&(*watcher)->watcherLock)) != CELIX_SUCCESS) {
		return status;
	}

	return status;
}

celix_status_t etcdWatcher_destroy(etcd_watcher_pt watcher) {
	celix_status_t status = CELIX_SUCCESS;
	char localNodePath[MAX_LOCALNODE_LENGTH];

	watcher->running = false;

	celixThread_join(watcher->watcherThread, NULL);

	// register own framework
	status = etcdWatcher_getLocalNodePath(watcher->node_discovery->context, &localNodePath[0]);

	if (status != CELIX_SUCCESS || etcd_del(localNodePath) == false)
	{
		fw_log(logger, OSGI_FRAMEWORK_LOG_WARNING, "Cannot remove local discovery registration.");
	}

	free(watcher);

	return status;
}

