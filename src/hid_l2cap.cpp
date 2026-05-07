#include <Arduino.h>
#include <esp_bt_main.h>
#include "esp32-hal-bt.h"
#include "esp_gap_bt_api.h"

#include "stack/l2c_api.h"
#include "osi/allocator.h"
extern "C"{
  #include "stack/btm_api.h"
}
#include "hid_l2cap.h"

extern bool isUsbBinaryTransferActive(void);

#define HID_LOGF(...) do { if (!isUsbBinaryTransferActive()) Serial.printf(__VA_ARGS__); } while (0)
#define HID_LOGLN(msg) do { if (!isUsbBinaryTransferActive()) Serial.println(msg); } while (0)

#define HID_L2CAP_ID_HIDC 0x40
#define HID_L2CAP_ID_HIDI 0x41

static long hid_l2cap_init_service( const char *name, uint16_t psm, uint8_t security_id);

static void hid_l2cap_connect_cfm_cback (uint16_t l2cap_cid, uint16_t result);
static void hid_l2cap_config_ind_cback (uint16_t l2cap_cid, tL2CAP_CFG_INFO *p_cfg);
static void hid_l2cap_config_cfm_cback (uint16_t l2cap_cid, tL2CAP_CFG_INFO *p_cfg);
static void hid_l2cap_disconnect_ind_cback (uint16_t l2cap_cid, bool ack_needed);
static void hid_l2cap_disconnect_cfm_cback (uint16_t l2cap_cid, uint16_t result);
static void hid_l2cap_data_ind_cback (uint16_t l2cap_cid, BT_HDR *p_msg);

static void dump_bin(const char *p_message, const uint8_t *p_bin, int len);

static BT_STATUS is_connected = BT_UNINITIALIZED;
static BD_ADDR g_bd_addr;
static HID_L2CAP_CALLBACK g_callback;
static volatile bool g_auth_completed = false;
static volatile bool g_stale_bond_detected = false;

// Drop the NVS link key for g_bd_addr and raise the stale-bond flag for
// the .ino layer. Used when the stack tells us the stored bond no longer
// authenticates with the peer (HCI 0x05 / 0x06, or AUTH_CMPL stat != 0).
// Without this, retrying L2CAP connect just keeps re-presenting the same
// bad key forever.
static void hid_l2cap_drop_stale_bond(const uint8_t *peer)
{
  esp_bd_addr_t addr;
  memcpy(addr, peer, sizeof(esp_bd_addr_t));
  esp_err_t err = esp_bt_gap_remove_bond_device(addr);
  if (err == ESP_OK) {
    HID_LOGLN("[BT_GAP] stale NVS bond removed; next attempt will pair fresh");
  } else {
    HID_LOGF("[BT_GAP] esp_bt_gap_remove_bond_device failed: 0x%x\n", err);
  }
  g_stale_bond_detected = true;
}

// GAP callback for handling SSP (Secure Simple Pairing) events
static void bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
  switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
      HID_LOGF("[BT_GAP] AUTH_CMPL: stat=%d, name=%s\n",
               param->auth_cmpl.stat, param->auth_cmpl.device_name);
      if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
        g_auth_completed = true;
        HID_LOGLN("[BT_GAP] Authentication successful - bond info should be in NVS");
      } else {
        HID_LOGLN("[BT_GAP] Authentication failed - dropping stale bond");
        hid_l2cap_drop_stale_bond(param->auth_cmpl.bda);
      }
      break;

    case ESP_BT_GAP_PIN_REQ_EVT:
      HID_LOGLN("[BT_GAP] PIN_REQ - replying with 0000");
      {
        esp_bt_pin_code_t pin = {'0', '0', '0', '0'};
        esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin);
      }
      break;

    case ESP_BT_GAP_CFM_REQ_EVT:
      HID_LOGF("[BT_GAP] CFM_REQ: num_val=%d - auto confirming\n",
               param->cfm_req.num_val);
      esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
      break;

    case ESP_BT_GAP_KEY_NOTIF_EVT:
      HID_LOGF("[BT_GAP] KEY_NOTIF: passkey=%d\n", param->key_notif.passkey);
      break;

    case ESP_BT_GAP_KEY_REQ_EVT:
      HID_LOGLN("[BT_GAP] KEY_REQ");
      break;

    case ESP_BT_GAP_MODE_CHG_EVT:
      HID_LOGF("[BT_GAP] MODE_CHG: mode=%d\n", param->mode_chg.mode);
      break;

    default:
      HID_LOGF("[BT_GAP] event=%d\n", event);
      break;
  }
}

static uint16_t l2cap_cid_hidc;
static uint16_t l2cap_cid_hidi;

static tL2CAP_CFG_INFO hid_cfg_info;
static const tL2CAP_APPL_INFO dyn_info = {
    NULL,
    hid_l2cap_connect_cfm_cback,
    NULL,
    hid_l2cap_config_ind_cback,
    hid_l2cap_config_cfm_cback,
    hid_l2cap_disconnect_ind_cback,
    hid_l2cap_disconnect_cfm_cback,
    NULL,
    hid_l2cap_data_ind_cback,
    NULL,
    NULL
} ;

static long hid_l2cap_init_services(void)
{  
  long ret;
  ret = hid_l2cap_init_service( "HIDC", BT_PSM_HIDC, BTM_SEC_SERVICE_FIRST_EMPTY   );
  if( ret != 0 )
    return ret;
  ret = hid_l2cap_init_service( "HIDI", BT_PSM_HIDI, BTM_SEC_SERVICE_FIRST_EMPTY + 1 );
  if( ret != 0 )
    return ret;

  return 0;
}

static long hid_l2cap_init_service( const char *name, uint16_t psm, uint8_t security_id)
{
    /* Register the PSM for incoming connections */
    if (!L2CA_Register(psm, (tL2CAP_APPL_INFO *) &dyn_info)) {
        HID_LOGF("%s Registering service %s failed\n", __func__, name);
        return -1;
    }

    /* Register with the Security Manager for our specific security level (none) */
    if (!BTM_SetSecurityLevel (false, name, security_id, 0, psm, 0, 0)) {
        HID_LOGF("%s Registering security service %s failed\n", __func__, name );
        return -1;
    }

    HID_LOGF("[%s] Service %s Initialized\n", __func__, name);

    return 0;
}

BT_STATUS hid_l2cap_is_connected(void)
{
  return is_connected;
}

bool hid_l2cap_auth_completed(void)
{
  if (g_auth_completed) {
    g_auth_completed = false;
    return true;
  }
  return false;
}

bool hid_l2cap_consume_stale_bond_flag(void)
{
  if (g_stale_bond_detected) {
    g_stale_bond_detected = false;
    return true;
  }
  return false;
}

long hid_l2cap_reconnect(void)
{
  long ret;
  ret = L2CA_CONNECT_REQ(BT_PSM_HIDC, g_bd_addr, NULL, NULL);
  HID_LOGF("L2CA_CONNECT_REQ ret=%ld\\n", static_cast<long>(ret));
  if( ret == 0 ){
    return -1;
  }
  l2cap_cid_hidc = ret;

  is_connected = BT_CONNECTING;

  return ret;
}

long hid_l2cap_connect(BD_ADDR addr)
{
  memmove(g_bd_addr, addr, sizeof(BD_ADDR));

  return hid_l2cap_reconnect();
}


long hid_l2cap_initialize(HID_L2CAP_CALLBACK callback)
{
  if(!btStarted() && !btStart()){
    HID_LOGLN("btStart failed");
    return -1;
  }

  esp_bluedroid_status_t bt_state = esp_bluedroid_get_status();
  if(bt_state == ESP_BLUEDROID_STATUS_UNINITIALIZED){
      if (esp_bluedroid_init()) {
        HID_LOGLN("esp_bluedroid_init failed");
        return -1;
      }
  }

  if(bt_state != ESP_BLUEDROID_STATUS_ENABLED){
      if (esp_bluedroid_enable()) {
        HID_LOGLN("esp_bluedroid_enable failed");
        return -1;
      }
  }

  // Register GAP callback for SSP (Secure Simple Pairing) handling
  if (esp_bt_gap_register_callback(bt_gap_cb) != ESP_OK) {
    HID_LOGLN("esp_bt_gap_register_callback failed");
    return -1;
  }

  // Set IO capability to NoInputNoOutput for "Just Works" pairing
  esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
  esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &iocap, sizeof(uint8_t));

  // Set fixed PIN for legacy pairing compatibility
  esp_bt_pin_code_t pin_code = {'0', '0', '0', '0'};
  esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_FIXED, 4, pin_code);

  HID_LOGLN("[BT] GAP callback registered, SSP configured");

  if( hid_l2cap_init_services() != 0 ){
    HID_LOGLN("hid_l2cap_init_services failed");
    return -1;
  }

  g_callback = callback;

  is_connected = BT_DISCONNECTED;

  return 0;
}

static void hid_l2cap_connect_cfm_cback(uint16_t l2cap_cid, uint16_t result)
{
  HID_LOGF("[%s] l2cap_cid: 0x%02x\n  result: %d\n", __func__, l2cap_cid, result );

  // result == 5 (HCI_ERR_AUTH_FAILURE) or 6 (HCI_ERR_KEY_MISSING) means
  // the stored link key for g_bd_addr no longer matches what the peer
  // expects (peer was re-paired with another host, or its bond store was
  // wiped). Retrying with the same NVS key fails forever; drop it so the
  // next L2CA_CONNECT_REQ falls back to fresh Just-Works SSP. The .ino
  // side is expected to also delete /bt_bond.dat via the
  // hid_l2cap_consume_stale_bond_flag() poll so the SD copy doesn't
  // resurrect the bad key on the next boot.
  if (result == 5 || result == 6) {
    HID_LOGF("[%s] stale link key (result=%d) — auto-forgetting bond\n", __func__, result);
    hid_l2cap_drop_stale_bond(g_bd_addr);
  }
}

static void hid_l2cap_config_cfm_cback(uint16_t l2cap_cid, tL2CAP_CFG_INFO *p_cfg)
{
  HID_LOGF("[%s] l2cap_cid: 0x%02x\n  p_cfg->result: %d\n", __func__, l2cap_cid, p_cfg->result );
    
  if( l2cap_cid == l2cap_cid_hidc ){
    long ret;
    ret = L2CA_CONNECT_REQ(BT_PSM_HIDI, g_bd_addr, NULL, NULL);
    HID_LOGF("L2CA_CONNECT_REQ ret=%ld\\n", static_cast<long>(ret));
    if( ret == 0 )
      return;
    l2cap_cid_hidi = ret;
  }else if( l2cap_cid == l2cap_cid_hidi ){
    is_connected = BT_CONNECTED;

    HID_LOGLN("Hid Connected");
  }
}

static void hid_l2cap_config_ind_cback(uint16_t l2cap_cid, tL2CAP_CFG_INFO *p_cfg)
{
    HID_LOGF("[%s] l2cap_cid: 0x%02x\n  p_cfg->result: %d\n  p_cfg->mtu_present: %d\n  p_cfg->mtu: %d\n", __func__, l2cap_cid, p_cfg->result, p_cfg->mtu_present, p_cfg->mtu );

    p_cfg->result = L2CAP_CFG_OK;

    L2CA_ConfigRsp(l2cap_cid, p_cfg);

    /* Send a Configuration Request. */
    L2CA_CONFIG_REQ(l2cap_cid, &hid_cfg_info);
}

static void hid_l2cap_disconnect_ind_cback(uint16_t l2cap_cid, bool ack_needed)
{
    HID_LOGF("[%s] l2cap_cid: 0x%02x\n  ack_needed: %d\n", __func__, l2cap_cid, ack_needed );
    is_connected = BT_DISCONNECTED;
    // Note: g_callback is intentionally NOT cleared here.
    // Clearing it would cause key presses to be ignored after reconnection.
}

static void hid_l2cap_disconnect_cfm_cback(uint16_t l2cap_cid, uint16_t result)
{
    HID_LOGF("[%s] l2cap_cid: 0x%02x\n  result: %d\n", __func__, l2cap_cid, result );
}

static void hid_l2cap_data_ind_cback(uint16_t l2cap_cid, BT_HDR *p_buf)
{
    HID_LOGF("[%s] l2cap_cid: 0x%02x\n", __func__, l2cap_cid );
    HID_LOGF("event=%d len=%d offset=%d layer_specific=%d\n", p_buf->event, p_buf->len, p_buf->offset, p_buf->layer_specific);
    dump_bin("\tdata=", &p_buf->data[p_buf->offset], p_buf->len);

    if( p_buf->len == (HID_L2CAP_MESSAGE_SIZE + 2) && p_buf->data[p_buf->offset] == 0xa1 && p_buf->data[p_buf->offset + 1] == 0x01){
      if( g_callback != NULL )
          g_callback(&p_buf->data[p_buf->offset + 2]);
    }

    osi_free( p_buf );
}

static void dump_bin(const char *p_message, const uint8_t *p_bin, int len)
{
  if (isUsbBinaryTransferActive()) {
    return;
  }
  Serial.printf("%s", p_message);
  for( int i = 0 ; i < len ; i++ ){
    Serial.printf("%02x ", p_bin[i]);
  }
  Serial.printf("\n");
}




