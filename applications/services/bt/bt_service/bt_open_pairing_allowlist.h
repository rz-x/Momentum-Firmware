#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Allowlist of centrals approved for Open BLE Pairing.
 *
 * Open BLE Pairing (Momentum, opt-in) drops the serial/RPC profile to Just Works with no bonding so
 * that a central without SMP support — a Garmin Connect IQ watch — can use RPC at all. Without any
 * further check that means *every* BLE central in radio range can open an RPC session, which is
 * full control of the device. This module is the check: the first time an unknown central
 * connects the user is asked on the Flipper's screen, and an approved address is remembered here
 * so the question is only asked once per device.
 *
 * Addresses are stored with their type. A central using resolvable private addresses will change
 * its address and be asked again; that is a limitation of matching on address, not a bug.
 */

#define BT_OPEN_PAIRING_ADDR_SIZE 6
#define BT_OPEN_PAIRING_ALLOWLIST_MAX 8

bool bt_open_pairing_allowlist_contains(
    uint8_t addr_type,
    const uint8_t addr[BT_OPEN_PAIRING_ADDR_SIZE]);

void bt_open_pairing_allowlist_add(uint8_t addr_type, const uint8_t addr[BT_OPEN_PAIRING_ADDR_SIZE]);

/** Forget every approved central. The next connection from each will prompt again. */
void bt_open_pairing_allowlist_clear(void);

/** Number of approved centrals currently stored. */
uint8_t bt_open_pairing_allowlist_count(void);

#ifdef __cplusplus
}
#endif
