#ifndef _HID_L2CAP_H_
#define _HID_L2CAP_H_

#include "stack/bt_types.h"

enum BT_STATUS {
  BT_UNINITIALIZED,
  BT_DISCONNECTED,
  BT_CONNECTING,
  BT_CONNECTED
};

enum KEY_MODIFIER_MASK {
  KEY_MASK_CTRL = 0x01,
  KEY_MASK_SHIFT = 0x02,
  KEY_MASK_ALT = 0x04,
  KEY_MASK_WIN = 0x08
};

#define HID_L2CAP_MESSAGE_SIZE  8
typedef void (*HID_L2CAP_CALLBACK)(uint8_t *p_msg); 

long hid_l2cap_initialize(HID_L2CAP_CALLBACK callback);
long hid_l2cap_connect(BD_ADDR addr);
long hid_l2cap_reconnect(void);
BT_STATUS hid_l2cap_is_connected(void);
bool hid_l2cap_auth_completed(void);

// Returns true (once) if the stack detected a stale stored link key —
// either AUTH_CMPL fired with stat != SUCCESS, or L2CAP connect_cfm came
// back with result 5 (HCI_ERR_AUTH_FAILURE) or 6 (HCI_ERR_KEY_MISSING).
// In that case the per-peer NVS bond was already removed via
// esp_bt_gap_remove_bond_device(); the .ino is expected to also delete
// the SD-side copy (/bt_bond.dat) so the next boot does not restore it.
// Idempotent: subsequent calls return false until the next failure.
bool hid_l2cap_consume_stale_bond_flag(void);

#endif
