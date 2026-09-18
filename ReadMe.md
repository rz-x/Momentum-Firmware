# Momentum Firmware for Garmin

**A Flipper Zero firmware fork that lets a Garmin watch control the Flipper over BLE.**

![A Flipper Sub-GHz Frequency Analyzer is visible on the Garmin at night while the Flipper stays in a bag](docs/pics/main_02.jpeg)

A fork of [Momentum Firmware](https://github.com/Next-Flip/Momentum-Firmware) with one feature on
top: a BLE central that can't bond (a Garmin watch, say) can drive the Flipper over RPC, and the
Flipper asks you first. Off by default, everything else is stock Momentum. Not affiliated with
the Momentum team.

### Where things live

| | |
|---|---|
| The code | this branch, [`feature/open-ble-pairing`](../../tree/feature/open-ble-pairing) |
| How it works, in detail | [`docs/OpenBlePairing.md`](docs/OpenBlePairing.md) |
| The watch app | [Flipper Watch Remote](https://github.com/rz-x/flipper-on-garmin) — the Connect IQ app that mirrors the Flipper screen and sends the buttons back |
| Unmodified upstream | the [`dev`](../../tree/dev) branch |

## Install

1. Grab the `.tgz` from [Releases](../../releases) (or build: `./fbt updater_package`).
2. Flash it like any Momentum update: qFlipper, or drop it on the SD card and pick it in the
   updater.
3. On the Flipper: **Momentum → Protocols → Open BLE Pairing → ON**, then reboot when asked.

That's it. Bluetooth stays on, the Flipper advertises as usual.

## Build it yourself

Same toolchain as Momentum, nothing extra. Linux or macOS, about 10 minutes the first time
(the toolchain download is most of it), under a minute after that.

```bash
git clone --recursive -b feature/open-ble-pairing https://github.com/rz-x/momentum-firmware-for-garmin.git
cd momentum-firmware-for-garmin
./fbt updater_package          # builds firmware + the .tgz update bundle into dist/f7-C/
```

`fbt` fetches its own ARM toolchain on first run, so no `apt install` beyond `git` and `python3`.
To flash straight over USB instead of copying the bundle by hand:

```bash
./fbt flash_usb_full           # Flipper connected, qFlipper closed
```

Editing the feature: the BLE side lives in `targets/f7/ble_glue/`, the prompt and allowlist in
`applications/services/bt/bt_service/`, the menu item in
`applications/main/momentum_app/scenes/momentum_app_scene_protocols.c`. `./fbt format` before
committing, `./fbt lint` to check.

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
bans any AI involvement in contributions, and this was built with AI help. So it lives here as a
fork instead of a PR.

No judgement on their rule, it's their project. But in 2026 a blanket AI ban reads to me like
shooting yourself in the foot: it doesn't filter out bad patches, it filters out the people who
bother to submit them. Code is either correct and reviewable, or it isn't, and that's readable
from the diff. Anyway, it's all here, take it or fork it.

## License

GPL-3.0, same as Momentum. See [LICENSE](LICENSE).
