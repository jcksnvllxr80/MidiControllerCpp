# Handoff: add WiFi support to the controller app

**Audience:** an agent working in `MidiControllerControllerApp` (Tauri + Svelte UI,
Rust backend). The **firmware** WiFi side is done (see this repo); the app needs a
matching **WiFi transport** + a **"Set WiFi" UI**. The wire protocol is unchanged —
WiFi carries the exact same newline-JSON the serial transport already speaks, so
you reuse `protocol/codec.rs` verbatim.

## What the firmware already does

- Stores WiFi credentials in flash; on boot, if enabled, joins the saved network.
- Runs the **same** request/response protocol over **TCP on port 8080** (in
  addition to USB serial).
- Advertises via **mDNS**: hostname `midicontroller.local`, service
  `_midicontroller._tcp` on port 8080, TXT `version=1`.
- Adds three ops (work over USB *and* TCP):

  ```jsonc
  // set credentials (first-time setup, done over USB) — enables WiFi + connects
  -> {"id":N,"op":"wifi_set","ssid":"MyNet","password":"secret"}   // password optional (open net)
  <- {"id":N,"ok":true,"data":{"enabled":true,"connected":true,"ssid":"MyNet","ip":"192.168.1.50"}}

  // query status
  -> {"id":N,"op":"wifi_status"}
  <- {"id":N,"ok":true,"data":{"enabled":bool,"connected":bool,"ssid":"...","ip":"..."}}

  // turn WiFi on/off (persisted)
  -> {"id":N,"op":"wifi_enable","on":false}
  <- {"id":N,"ok":true,"data":{...status...}}
  ```

## First-time setup flow (UX)

1. User connects over **USB/Serial** (already works).
2. App shows a **"WiFi" screen**: SSID + password fields → on save, send `wifi_set`.
3. Show the returned `ip` (the OLED shows it too). Device is now on the network.
4. Next launch (or after a rescan), the **WiFi transport discovers it via mDNS**
   and the user can connect wirelessly — no cable.

## Task 1 — WiFi transport (`src-tauri/src/transport/wifi.rs`)

Implement `Transport` (see `transport/mod.rs`) for `Protocol::Wifi`, mirroring
`serial.rs` but over TCP:

- `discover()` — browse mDNS for `_midicontroller._tcp` (crate suggestion:
  `mdns-sd`). For each hit, build a `DeviceInfo { protocol: Wifi, name, address:
  Address::Net { host, port }, .. }` (resolve to the IP or use `midicontroller.local`).
- `connect(device)` — `TcpStream::connect((host, port))`, set read timeout
  (~1500 ms), then run the **identify** handshake using the existing
  `codec::roundtrip` (TcpStream is `Read + Write`, so `codec` works unchanged).
  Return the `DeviceIdentity` from `data` exactly like serial.
- `request(req)` — `codec::roundtrip` over the stream.
- `disconnect()` / `is_connected()` — as in serial.
- Register it in `transport/registry.rs` alongside Serial/USB.

Because `codec.rs` is transport-agnostic (`Read`/`Write`), this is essentially
`serial.rs` with `TcpStream` swapped in for the serial port + mDNS discovery.

## Task 2 — "Set WiFi" UI (Svelte)

- A small form (SSID, password) reachable once connected over any transport.
- On submit: `request({op:"wifi_set", ssid, password})`; show
  `data.ip` / `data.connected`. Add a toggle that sends `wifi_enable {on}`.
- Optional: a `wifi_status` poll to reflect connection state.

## Acceptance

- Set credentials over USB → firmware reports an IP.
- The app's WiFi scan lists the device (via mDNS) and connects to it over TCP.
- All existing ops (`list_*`, `get_*`, `dpad`, `short`, …) work identically over
  WiFi — same `codec`, just a different socket.

## Gotchas

- Same framing/codec as serial — do **not** invent a new protocol.
- Port is **8080**, service `_midicontroller._tcp`, host `midicontroller.local`.
- The firmware also answers `identify`/`ping` over TCP, so reuse the serial
  connect/heartbeat logic.
- `Protocol::Wifi` already exists in `transport/mod.rs`; `usb.rs` is a stub — model
  the new `wifi.rs` on `serial.rs`, not `usb.rs`.
