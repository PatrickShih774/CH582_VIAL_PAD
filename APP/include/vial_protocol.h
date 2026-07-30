/*
 * vial_protocol.h
 *
 *  Standard VIA/Vial protocol constants for vial.rocks compatibility.
 *  Reference: https://github.com/the-via/firmware
 */

#ifndef VIAL_PROTOCOL_H_
#define VIAL_PROTOCOL_H_

#include <stdint.h>

/* ── VIA Protocol ─────────────────────────────────────────────────── */
#define VIA_PROTOCOL_VERSION    0x0009

enum via_command_id {
    VIA_GET_PROTOCOL_VERSION        = 0x01,
    VIA_GET_KEYBOARD_VALUE          = 0x02,
    VIA_SET_KEYBOARD_VALUE          = 0x03,
    VIA_DYNAMIC_KEYMAP_GET_KEYCODE  = 0x04,
    VIA_DYNAMIC_KEYMAP_SET_KEYCODE  = 0x05,
    VIA_DYNAMIC_KEYMAP_RESET        = 0x06,
    VIA_LIGHTING_SET_VALUE          = 0x07,
    VIA_LIGHTING_GET_VALUE          = 0x08,
    VIA_LIGHTING_SAVE               = 0x09,
    VIA_EEPROM_RESET                = 0x0A,
    VIA_BOOTLOADER_JUMP             = 0x0B,
    VIA_MACRO_GET_COUNT             = 0x0C,
    VIA_MACRO_GET_BUFFER            = 0x0D,
    VIA_MACRO_SET_BUFFER            = 0x0E,
    VIA_DYNAMIC_KEYMAP_GET_BUFFER   = 0x11,
    VIA_DYNAMIC_KEYMAP_SET_BUFFER   = 0x12,
};

enum via_keyboard_value_id {
    VIA_VALUE_UPTIME                = 0x00,
    VIA_VALUE_LAYOUT_OPTIONS        = 0x01,
    VIA_VALUE_SWITCH_MATRIX_STATE   = 0x02,
};

/* ── Vial Protocol ─────────────────────────────────────────────────── */
#define VIAL_PROTOCOL_VERSION   6
#define VIAL_CMD_PREFIX         0xFE

enum vial_command_id {
    VIAL_GET_KEYBOARD_ID    = 0x00,
    VIAL_GET_SIZE           = 0x01,
    VIAL_GET_DEFINITION     = 0x02,
    VIAL_GET_ENCODER        = 0x03,
    VIAL_SET_ENCODER        = 0x04,
    VIAL_GET_UNLOCK_STATUS  = 0x05,
    VIAL_UNLOCK             = 0x06,
    VIAL_GET_LAYER_OPTIONS  = 0x07,
    VIAL_SET_LAYER_OPTIONS  = 0x08,
    VIAL_QMK_SETTINGS_QUERY = 0x09,
    VIAL_QMK_SETTINGS_GET   = 0x0A,
    VIAL_QMK_SETTINGS_SET   = 0x0B,
    VIAL_QMK_SETTINGS_RESET = 0x0C,
    VIAL_DYNAMIC_ENTRY_OP   = 0x0D,
};

/* ── Keyboard UID (8 bytes, derived from VID/PID) ─────────────────── */
#define VIAL_KEYBOARD_UID  {0x73, 0x92, 0x57, 0x91, 0x00, 0x00, 0x00, 0x01}

/* ── Keymap dimensions ─────────────────────────────────────────────── */
#define VIAL_MATRIX_ROWS    6
#define VIAL_MATRIX_COLS    4
#define VIAL_MATRIX_SIZE    (VIAL_MATRIX_ROWS * VIAL_MATRIX_COLS)
#define VIAL_LAYER_COUNT    4

/* ── Unlock key (last 4 bytes of UID, big-endian → 0x0000_0001) ──── */
#define VIAL_UNLOCK_KEY     0x00000001

#endif /* VIAL_PROTOCOL_H_ */
