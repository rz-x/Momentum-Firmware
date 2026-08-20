# Open BLE Pairing

An **opt-in** setting that lets BLE centrals **without bonding support** use the Flipper's serial
service and its RPC. Off by default; when off, the stock secure behaviour is unchanged.

## Motivation

The BLE serial service (`serial_service.c`) declares its RX/TX characteristics with
`ATTR_PERMISSION_AUTHEN_READ | ATTR_PERMISSION_AUTHEN_WRITE`, and the serial profile
(`serial_profile.c`) pairs with `GapPairingPinCodeShow` (MITM required). A central that cannot
perform SMP bonding therefore cannot read TX, write RX, or subscribe — it cannot use RPC at all.

Some hosts have no bonding stack. The concrete case that motivated this: **Garmin Connect IQ**,
whose `Toybox.BluetoothLowEnergy` has no SMP/bonding and is always the GATT central. This is not
Garmin-specific — any minimal embedded central hits the same wall.

This setting opens the existing, well-tested RPC path to those clients without adding a second
GATT service or a bridge app.

## What it does when enabled

**Momentum → Protocols → Open BLE Pairing → ON** (then restart the BT stack — the setting flags
`require_reboot`):

- **Pairing** drops to Just Works, bonding off (`serial_profile.c`): `GapPairingNone`,
  `bonding_mode = false`, which sets `MITM_PROTECTION_NOT_REQUIRED`.
- **Characteristic permissions** on the serial service relax to `ATTR_PERMISSION_NONE`
  (`serial_service.c`): the const characteristic table is copied and patched at
  `ble_svc_serial_start()`; the stock table is untouched.

When OFF, both paths are byte-for-byte the stock behaviour.

## Security implications

Enabling this **removes authentication and encryption from the serial link**: any nearby BLE
central can pair (Just Works) and drive RPC — read the screen, inject input, and use every RPC
command. That is why it is **off by default, gated behind an explicit toggle, and flagged with a
reboot**. Users should enable it only while actively using a non-bonding client and disable it
afterwards. It does not touch any other profile (HID, etc.).

## Files changed

| File | Change |
|------|--------|
| `lib/momentum/settings.h` | add `bool open_ble_pairing;` |
| `lib/momentum/settings.c` | default `false`; register in the settings serializer |
| `targets/f7/ble_glue/profiles/serial_profile.c` | when set: `GapPairingNone`, `bonding_mode = false` |
| `targets/f7/ble_glue/services/serial_service.c` | when set: RX/TX permissions → `ATTR_PERMISSION_NONE` |
| `applications/main/momentum_app/scenes/momentum_app_scene_protocols.c` | ON/OFF item under Protocols |

## Testing

- Builds clean: `./fbt` → `firmware.elf` + `firmware.bin`.
- OFF (default): existing bonded clients (qFlipper, mobile app) unaffected.
- ON: a non-bonding central connects, subscribes to TX (INDICATE), writes RX, and runs RPC
  (e.g. `Gui.StartScreenStreamRequest` + `Gui.SendInputEventRequest`).

## Origin

Developed for [Flipper-on-Garmin](https://github.com/rz-x/flipper-on-garmin), a Connect IQ app
that mirrors the Flipper screen on a Garmin watch and relays input. Proposed upstream as a general
capability for bonding-incapable BLE clients.
