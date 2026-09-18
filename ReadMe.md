# Momentum Firmware, Open BLE Pairing edition

A fork of [Momentum Firmware](https://github.com/Next-Flip/Momentum-Firmware) with one feature on
top: a BLE central that can't bond (a Garmin watch, say) can drive the Flipper over RPC, and the
Flipper asks you first. Everything else is stock Momentum. Not affiliated with the Momentum team.

Built for [Flipper Watch Remote](https://github.com/rz-x/flipper-on-garmin), a Connect IQ app that
mirrors the Flipper screen on a Garmin and sends the buttons back. Works with any other
non-bonding central just as well.

## Install

1. Grab the `.tgz` from [Releases](../../releases) (or build: `./fbt updater_package`).
2. Flash it like any Momentum update: qFlipper, or drop it on the SD card and pick it in the
   updater.
3. On the Flipper: **Momentum → Protocols → Open BLE Pairing → ON**, then reboot when asked.

That's it. Bluetooth stays on, the Flipper advertises as usual.

## How it behaves

The first time a new device connects, the Flipper shows:

```
      Allow BLE remote?
   Watch C45B:0E12:AA1E
  wants to control Flipper
 [Deny]              [Allow]
```

Allow remembers the device (up to 8) and opens RPC. Deny drops the link before any service is
reachable, and the remote side sees a plain disconnect. Remembered devices don't get asked again.
To wipe the list: **Momentum → Protocols → Forget BLE Remotes**.

Toggle OFF and the firmware is byte-for-byte stock Momentum on the BLE side. Off is the default.

## What actually changed, and why

Stock Momentum wants a bonded, MITM-protected link before it lets anyone near the serial/RPC
service. Connect IQ has no SMP at all, so a watch could see the Flipper and do nothing with it.
Getting from "nothing" to "working" took five separate fixes, each of which looked like the last
one until it wasn't:

1. Serial characteristics drop to `ATTR_PERMISSION_NONE` and the profile to Just Works with
   bonding off. This is the actual toggle.
2. `GapEventTypeConnected` is emitted on connection complete. Stock only emits it after pairing
   completes, and with Just Works that never happens, so the RPC session never opened.
3. `max_packet_size` starts at 20, not 486. With the default 23-byte MTU the old code chunked by
   486, sent 20, and advanced the pointer by 486. Frames arrived with holes. This one cost a
   week.
4. The connection-interval check now runs on unbonded links too. It was gated on `is_secure`,
   which only pairing sets, so a watch proposing 997 ms got 997 ms. Now it negotiates down to
   7 ms. That's the difference between one frame a minute and two a second.
5. `aci_gatt_exchange_config()` is requested at connect and retried once the parameters settle.
   Whether Garmin ever accepts a bigger MTU is still an open question (see below).

Then the security bit, because 1 plus 2 meant anyone in radio range could open RPC without a
sound: a `GapEventTypeConnectionRequest` fires before `Connected`, the bt service checks an
allowlist in internal flash (`/int/.bt_open_allow`) or asks on screen, and a refusal terminates
the link. The handler blocks the GAP thread on the dialog, which is exactly what the stock
numeric-comparison handler already does, so no new pattern was invented.

Full write-up: [`docs/OpenBlePairing.md`](docs/OpenBlePairing.md).

Files touched: `targets/f7/ble_glue/gap.{c,h}`, `services/serial_service.c`,
`profiles/serial_profile.c`, `applications/services/bt/bt_service/bt.c`,
`bt_open_pairing_allowlist.{c,h}` (new), `lib/momentum/settings.{c,h}`,
`applications/main/momentum_app/scenes/momentum_app_scene_protocols.c`.

## Limitations, read before you rely on it

- **The link is not encrypted.** Just Works, no bonding, no keys. Screen contents and keystrokes
  go over the air in the clear. The prompt stops strangers from taking control; it does not stop
  anyone from listening. Turn the toggle off when you're done.
- **Approval is by BLE address.** A central that rotates its address (resolvable private
  addresses) gets asked again after every rotation. Whether a Garmin does this is not yet known;
  it's the first thing to check on new hardware.
- **MTU stays at 23 with Connect IQ so far.** A 1 KB screen frame is 52 packets, each waiting for
  its own INDICATE confirmation. That caps the mirror at roughly 0.5 fps. The MTU retry is in,
  but not confirmed to help.
- **INDICATE, not NOTIFY, on purpose.** Notify would be much faster and would also desync RPC's
  varint framing for good on the first lost packet. Not touching it without a resync strategy.
- **Tested on one pair of devices**: Descent Mk2 plus one Flipper Zero. The approval prompt,
  allowlist and Forget menu have been exercised on that pair once. Treat the rest as
  build-verified.
- **Shallow clone.** This fork has no common git history with upstream yet, so rebasing onto
  `upstream/dev` needs `git fetch --unshallow` first. Known, not yet done.

## Known issues

- One unexplained Flipper hang (spinning hourglass, hard reset) during an early session. Not
  reproduced since the approval prompt landed, no log captured at the time. If you hit it, `log`
  over USB CLI and send the tail.
- `Forget BLE Remotes` is locked while the toggle is off. Intentional, but easy to miss.

## Why not upstream

Momentum's [CONTRIBUTING.md](https://github.com/Next-Flip/Momentum-Firmware/blob/dev/CONTRIBUTING.md)
bans any AI involvement in contributions, and this feature was built with an AI coding assistant
(the commit trailers say so). So it lives here as a fork rather than a PR. If someone wants to
re-implement it by hand and submit, `docs/OpenBlePairing.md` and the commit messages describe
every change and the reason for it.

## License

GPL-3.0, same as Momentum. See [LICENSE](LICENSE).
