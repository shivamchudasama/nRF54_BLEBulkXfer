# BulkXfer: this device is the receiver (Server role) only. The Client role
# would need BLK_CLI_WRITE_INFLIGHT_MAX + BLK_SRV_NOTIFY_INFLIGHT_MAX ATT
# credits, which is more than the RX-priority CONFIG_BT_ATT_TX_COUNT in prj.conf.
zephyr_compile_definitions(BLK_ENABLE_SERVER=1 BLK_ENABLE_CLIENT=0)

# GATT_CB first: the BulkXfer Server sits on top of the generic GATT callbacks
add_subdirectory(GATT_CB)
add_subdirectory(BulkXfer)
