# Linux Impulse Triggers

> [!IMPORTANT]
> **AI-assisted project:** OpenAI Codex materially assisted with the
> investigation, code, scripts, and documentation. All physical controller
> tests and confirmed in-game results were performed by a human. See
> [AI_DISCLOSURE.md](AI_DISCLOSURE.md).

Genuine, game-controlled Xbox-style impulse-trigger rumble on Linux through
SDL's direct USB/GIP backend and Wine/Proton's existing four-motor path.

This project is about **four independent rumble channels**:

- low-frequency main motor
- high-frequency main motor
- left-trigger motor
- right-trigger motor

It does not synthesize trigger vibration from ordinary rumble or trigger
pressure.

## Status

Confirmed on the **8BitDo Ultimate Wired Controller for Xbox**, USB
`2dc8:2015`, on CachyOS, with Steam Input disabled:

| Game | Proton path | Result |
|---|---|---|
| Forza Horizon 6 (`2483190`) | `proton-cachyos-native` 11.0-20260703; SDL 3.4.14 / sdl2-compat 2.32.70 | Human-confirmed in-game impulse triggers |
| Forza Motorsport (2023) (`2440510`) | `GE-Proton11-3-FM` inside Steam Linux Runtime 4; private SDL 3.4.16 / sdl2-compat 2.32.72 / libusb 1.0.30; controller-ID workaround | Human-confirmed controls and in-game impulse triggers, 2026-09-06 |

**Native CachyOS Proton is not a universal requirement.** Motorsport works
inside the Steam runtime with a suitable SDL/libusb stack. FH6 using this
container approach, and use on other distributions, remain untested here.
See the [Motorsport findings and reproduction notes](docs/forza-motorsport-2023.md).

Official Xbox One/Series and other licensed USB controllers using Microsoft's
GIP protocol are strong candidates, but should be listed as confirmed only
after all four physical motors have been tested. “Xbox licensed” does not
guarantee that a controller uses the same USB, Bluetooth, or manufacturer HID
protocol.

## Why this is needed

The tested pipeline was:

```text
game
  -> Windows rumble API
  -> Wine winebus.sys
  -> SDL2 compatibility layer
  -> SDL3 direct Xbox/GIP backend
  -> libusb
  -> controller
```

Wine already preserved four independent intensities and called both ordinary
and trigger-rumble SDL functions. SDL already knew how to send the appropriate
USB/GIP packet. The blockers were:

1. Linux's ordinary `xpad` evdev force-feedback path exposed only the two main
   motors.
2. SDL needed write access to the raw USB device to select its direct GIP
   backend.
3. The initial FH6 runtime test could not open direct USB; native Proton
   provided a working host-library route. The later Motorsport investigation
   verified that Runtime 4 exposed raw USB, but its supplied SDL still chose
   evdev. Loading private SDL/libusb copies inside that runtime restored GIP.
4. Motorsport additionally rejected physical controllers because its Wine
   `NonRoamableId` implementation returned `E_NOTIMPL`. An exact-build WGI DLL
   workaround restored controller enumeration in the game.

FH6 required no source or binary patch. Motorsport required a four-byte
controller-ID DLL workaround, but no kernel, xpad, or SDL source changes.
The earlier blanket claim that Steam's container necessarily hides raw USB
was too broad; test both device visibility and the actual library/backend.

## Safety and scope

The included installer creates a udev rule for exactly one VID:PID supplied on
the command line. It grants raw USB access only to the active local desktop user
through `TAG+="uaccess"`; it does not make every USB device world-writable.

Do not add a broad vendor-wide or all-controller rule. Confirm the exact USB
ID with `lsusb` first.

## 1. Install build dependencies

On CachyOS/Arch:

```bash
sudo pacman -S --needed base-devel pkgconf sdl3 sdl2-compat libusb
```

Build the diagnostics:

```bash
make
```

## 2. Probe the current path

Connect the controller by USB and run:

```bash
./build/sdl3-four-motor-test --probe
```

If the path looks like `/dev/input/event...` and trigger-rumble capability is
false, SDL is probably using the kernel evdev/xpad path.

Find the controller's exact USB ID:

```bash
lsusb
```

For the confirmed 8BitDo controller it is `2dc8:2015`. Do not reuse that ID for
a different controller.

## 3. Grant narrowly scoped raw-USB access

Pass the hexadecimal vendor and product IDs separately:

```bash
sudo ./scripts/install-udev-rule.sh 2dc8 2015
```

Unplug and reconnect the controller once. Then probe SDL's direct backend:

```bash
SDL_HIDAPI_LIBUSB=1 \
SDL_HIDAPI_LIBUSB_WHITELIST=0 \
SDL_JOYSTICK_HIDAPI=1 \
SDL_JOYSTICK_HIDAPI_XBOX=1 \
SDL_JOYSTICK_HIDAPI_XBOX_ONE=1 \
SDL_JOYSTICK_HIDAPI_GIP=1 \
SDL_JOYSTICK_HIDAPI_GIP_RESET_FOR_METADATA=0 \
./build/sdl3-four-motor-test --probe
```

A direct USB/GIP controller normally has a path such as `3-5:1.0`, rather than
an evdev path, and reports both ordinary and trigger rumble capability.

## 4. Physically verify all four motors

The probe prints an index for every detected joystick. Run the interactive test
with the desired index, for example index 0:

```bash
SDL_HIDAPI_LIBUSB=1 \
SDL_HIDAPI_LIBUSB_WHITELIST=0 \
SDL_JOYSTICK_HIDAPI=1 \
SDL_JOYSTICK_HIDAPI_XBOX=1 \
SDL_JOYSTICK_HIDAPI_XBOX_ONE=1 \
SDL_JOYSTICK_HIDAPI_GIP=1 \
SDL_JOYSTICK_HIDAPI_GIP_RESET_FOR_METADATA=0 \
./build/sdl3-four-motor-test --rumble 0
```

The program asks before each pattern:

1. main low-frequency motor only
2. main high-frequency motor only
3. left-trigger motor only
4. right-trigger motor only
5. both trigger motors at 50%
6. everything off

Do not report a controller as confirmed based only on SDL success codes; a
person holding the controller must verify which physical motor moved.

## 5. Configure Steam/Proton

For the confirmed FH6 setup:

1. Disable Steam Input for the game.
2. Select a native Proton build that uses the host SDL and libusb. The tested
   tool was `proton-cachyos-native`.
3. Add the launch options from
   [`steam/forza-horizon-6.txt`](steam/forza-horizon-6.txt).

A successful host probe does not prove the game uses the same SDL backend.
Probe inside its exact runtime too. The confirmed Motorsport configuration
uses the container successfully; see its separate guide below.

## Revert

Remove only the rule for a particular controller:

```bash
sudo ./scripts/uninstall-udev-rule.sh 2dc8 2015
```

Then unplug and reconnect it. You can also switch the game back to its previous
Proton build and remove its launch options.

## Compatibility

| Controller | Connection | VID:PID | Result |
|---|---|---:|---|
| 8BitDo Ultimate Wired Controller for Xbox | USB GIP | `2dc8:2015` | Confirmed: four independent motors |
| Official Xbox Series controller | USB GIP | varies | Expected; physical test needed |
| Xbox/third-party controller over Bluetooth | Bluetooth HID | varies | Unverified; different transport |
| Other Xbox-licensed USB controllers | varies | varies | Test individually |

Please include the exact name, VID:PID, connection type, SDL version, backend
path, and physical results when reporting another controller.

## Forza Motorsport (2023)

**Confirmed working on the tested system**, including genuine independent
left/right trigger feedback. The solution retains GE-Proton11-3-FM and Xodus,
loads a working SDL/libusb stack inside Runtime 4, and fixes the game's failing
physical-controller ID request. It does not synthesize rumble.

Read [the full Motorsport report](docs/forza-motorsport-2023.md) for exact
versions, evidence, the credited controller-ID workaround, reproduction
constraints, and rollback. This is a documented experimental setup, not an
automatic installer or a claim that every Proton build/controller works.

## Technical notes and upstream references

- [`docs/investigation.md`](docs/investigation.md)
- [`AI_DISCLOSURE.md`](AI_DISCLOSURE.md)
- [SDL Xbox One HIDAPI backend](https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/SDL_hidapi_xboxone.c)
- [SDL 8BitDo HIDAPI backend](https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/SDL_hidapi_8bitdo.c)
- [Wine SDL winebus backend](https://gitlab.winehq.org/wine/wine/-/blob/master/dlls/winebus.sys/bus_sdl.c)
- [Linux xpad driver](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c)

## AI disclosure

OpenAI Codex materially assisted with this project's investigation, test code,
scripts, and documentation. The physical four-motor tests and final in-game
behavior were performed and confirmed by a human. See
[`AI_DISCLOSURE.md`](AI_DISCLOSURE.md) for details.

## License

MIT. See [`LICENSE`](LICENSE).
