# Open BLE Pairing

An **opt-in** setting that lets BLE centrals **without bonding support** use the Flipper's serial
service and its RPC. Off by default; when off, the stock secure behaviour is unchanged.

## Motivation

The BLE serial service (`serial_service.c`) declares its RX/TX characteristics with
`ATTR_PERMISSION_AUTHEN_READ | ATTR_PERMISSION_AUTHEN_WRITE`, and the serial profile
(`serial_profile.c`) pairs with `GapPairingPinCodeShow` (MITM required). A central that cannot
perform SMP bonding therefore cannot read TX, write RX, or subscribe - it cannot use RPC at all.

Some hosts have no bonding stack. The concrete case that motivated this: **Garmin Connect IQ**,
whose `Toybox.BluetoothLowEnergy` has no SMP/bonding and is always the GATT central. This is not
Garmin-specific - any minimal embedded central hits the same wall.

This setting opens the existing, well-tested RPC path to those clients without adding a second
GATT service or a bridge app.

## What it does when enabled

**Momentum → Protocols → Open BLE Pairing → ON** (then restart the BT stack - the setting flags
`require_reboot`):

- **Pairing** drops to Just Works, bonding off (`serial_profile.c`): `GapPairingNone`,
  `bonding_mode = false`, which sets `MITM_PROTECTION_NOT_REQUIRED`.
- **Characteristic permissions** on the serial service relax to `ATTR_PERMISSION_NONE`
  (`serial_service.c`): the const characteristic table is copied and patched at
  `ble_svc_serial_start()`; the stock table is untouched.

When OFF, both paths are byte-for-byte the stock behaviour.

## Approval prompt and allowlist (2026-09-18)

Enabling this removes authentication and encryption from the serial link. On its own that would
let any nearby central drive RPC silently, so a check sits in front of it:

- `gap.c` emits `GapEventTypeConnectionRequest` (peer address in `data.peer`) before
  `GapEventTypeConnected` on a Just Works link. The handler's return value decides.
- `bt.c` answers from `bt_open_pairing_allowlist.{c,h}` (up to 8 addresses in
  `/int/.bt_open_allow`) or shows a Deny/Allow dialog with the address. Allow stores the address;
  Deny makes GAP call `aci_gap_terminate()` before `Connected` is ever emitted.
- **Momentum → Protocols → Forget BLE Remotes** clears the list (locked while the toggle is off).

Matching is by address, so a central using resolvable private addresses is asked again after
each rotation. The link itself remains unencrypted; the prompt controls access, not
confidentiality. Off by default, reboot-flagged, and it does not touch any other profile.

## Files changed

| File | Change |
|------|--------|
| `lib/momentum/settings.h` | add `bool open_ble_pairing;` |
| `lib/momentum/settings.c` | default `false`; register in the settings serializer |
| `targets/f7/ble_glue/profiles/serial_profile.c` | when set: `GapPairingNone`, `bonding_mode = false` |
| `targets/f7/ble_glue/services/serial_service.c` | when set: RX/TX permissions → `ATTR_PERMISSION_NONE` |
| `applications/main/momentum_app/scenes/momentum_app_scene_protocols.c` | ON/OFF item and "Forget BLE Remotes" under Protocols |
| `targets/f7/ble_glue/gap.c`, `gap.h` | Connected on Just Works; interval check on unbonded links; MTU exchange + retry; `GapEventTypeConnectionRequest` |
| `applications/services/bt/bt_service/bt.c` | `max_packet_size` from real MTU; connection-request handler with dialog |
| `applications/services/bt/bt_service/bt_open_pairing_allowlist.{c,h}` | new: approved-address store in internal flash |

## Testing

- Builds clean: `./fbt` → `firmware.elf` + `firmware.bin`.
- OFF (default): existing bonded clients (qFlipper, mobile app) unaffected.
- ON: a non-bonding central connects, subscribes to TX (INDICATE), writes RX, and runs RPC
  (e.g. `Gui.StartScreenStreamRequest` + `Gui.SendInputEventRequest`).

## Origin

Developed for [Flipper-on-Garmin](https://github.com/rz-x/flipper-on-garmin), a Connect IQ app
that mirrors the Flipper screen on a Garmin watch and relays input. Proposed upstream as a general
capability for bonding-incapable BLE clients.
