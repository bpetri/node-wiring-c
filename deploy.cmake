deploy("wiring" BUNDLES
		${CELIX_BUNDLES_DIR}/shell.zip
		${CELIX_BUNDLES_DIR}/shell_tui.zip
        org.inaetics.node_discovery.etcd.NodeDiscovery
        org.inaetics.wiring_topology_manager.WiringTopologyManager
      )

deploy("node_discovery_etcd" BUNDLES
		${CELIX_BUNDLES_DIR}/shell.zip
		${CELIX_BUNDLES_DIR}/shell_tui.zip
        org.inaetics.node_discovery.etcd.NodeDiscovery
      )

deploy("node_discovery_etcd_2" BUNDLES
		${CELIX_BUNDLES_DIR}/shell.zip
		${CELIX_BUNDLES_DIR}/shell_tui.zip
        org.inaetics.node_discovery.etcd.NodeDiscovery
      )

deploy("wiring_topology_manager" BUNDLES
		${CELIX_BUNDLES_DIR}/shell.zip
		${CELIX_BUNDLES_DIR}/shell_tui.zip
        org.inaetics.wiring_topology_manager.WiringTopologyManager
      )
