# Linux Impulse Triggers

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

Confirmed working end to end with:

- 8BitDo Ultimate Wired Controller for Xbox, USB `2dc8:2015`
- CachyOS with SDL 3.4.14 and `sdl2-compat` 2.32.70
- `proton-cachyos-native` 11.0-20260703
- Forza Horizon 6, Steam AppID `2483190`
- Steam Input disabled

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
3. Steam Linux Runtime's container did not expose `/dev/bus/usb`, so the game
   needed a native Proton build using the host SDL/libusb stack.

No kernel, xpad, SDL, Wine, or Proton source patch was needed for the confirmed
controller.

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

The standard Steam Linux Runtime/Pressure Vessel path did not expose raw USB to
SDL in the investigation. A controller can therefore pass the standalone host
test but fall back to evdev when the game runs in the container.

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

Not yet supported by this repository. Motorsport has an independent Xbox Game
Runtime/Gaming Services launch blocker under ordinary Proton. Experimental GDK
Wine work exists, but it should not be mixed into the confirmed FH6 instructions
until game startup and the four-motor path have both been verified.

## Technical notes and upstream references

- [`docs/investigation.md`](docs/investigation.md)
- [SDL Xbox One HIDAPI backend](https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/SDL_hidapi_xboxone.c)
- [SDL 8BitDo HIDAPI backend](https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/SDL_hidapi_8bitdo.c)
- [Wine SDL winebus backend](https://gitlab.winehq.org/wine/wine/-/blob/master/dlls/winebus.sys/bus_sdl.c)
- [Linux xpad driver](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c)

## License

MIT. See [`LICENSE`](LICENSE).
