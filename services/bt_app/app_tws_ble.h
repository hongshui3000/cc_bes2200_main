#ifndef __APP_TWS_BLE_H
#define __APP_TWS_BLE_H

void tws_ble_init(void);

void tws_ble_listen(BOOL listen);

void customer_ble_adv_enable(BOOL listen);

void customer_tws_ble_set_adv_para(uint16_t interval_min, uint16_t interval_max, 
                           uint8_t adv_type, uint8_t own_addr_type, 
                           uint8_t peer_addr_type,
                           uint8_t adv_chanmap, uint8_t adv_filter_policy);

void customer_ble_set_adv_data(bool set_scan_rsp, bool include_name,
                           bool include_txpower, int min_interval, 
                           int max_interval, int appearance,
                           uint16_t manufacturer_len, char* manufacturer_data,
                           uint16_t service_data_len, char* service_data,
                           uint16_t service_uuid_len, char* service_uuid);

void tws_reconnect_ble_set_adv_para(uint8_t isConnectedPhone);
void tws_reconnect_ble_set_adv_data(uint8_t isConnectedPhone);
#endif
