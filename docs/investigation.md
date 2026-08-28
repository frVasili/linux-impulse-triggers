# Investigation notes

## Confirmed hardware and software

- Controller: 8BitDo Ultimate Wired Controller for Xbox
- USB ID: `2dc8:2015`
- USB class/subclass/protocol: `ff/47/d0` (Xbox GIP)
- Normal Linux driver: `xpad`
- Kernel during final verification: CachyOS `7.2.2-1-cachyos`
- SDL: 3.4.14
- SDL2 compatibility layer: 2.32.70
- Proton: `proton-cachyos-native` 11.0-20260703
- Game: Forza Horizon 6, Steam AppID `2483190`

## Baseline

Through the stock `xpad` evdev path, SDL reported ordinary rumble but no trigger
rumble. An interactive test physically distinguished the large low-frequency
main motor from the lighter/faster high-frequency main motor. Both trigger-only
calls returned unsupported and neither trigger moved.

The controller appeared at an evdev path such as `/dev/input/event...`.

## Direct SDL/GIP test

SDL 3.4 already contained:

- `SDL_RumbleJoystickTriggers()`
- explicit left/right trigger intensities
- an Xbox GIP USB backend
- support code for Xbox and 8BitDo devices

The raw USB node initially lacked an active-user write ACL. After applying an
exact VID:PID udev rule, SDL still required
`SDL_HIDAPI_LIBUSB_WHITELIST=0` to consider this device for libusb. The GIP
backend also connects asynchronously, so the diagnostic pumps events for 2.5
seconds before enumeration.

With direct GIP enabled, the SDL path changed to a USB interface path such as
`3-5:1.0`. SDL reported both ordinary and trigger-rumble capability. A person
holding the controller then confirmed all of these independently:

- main low-frequency motor only
- main high-frequency motor only
- left-trigger motor only
- right-trigger motor only
- both trigger motors together

This proved that Linux could drive all four physical motors on this device
without an xpad or kernel patch.

## Wine/Proton trace

Wine's SDL winebus backend already carried four values named approximately:

- rumble intensity
- buzz intensity
- left-trigger intensity
- right-trigger intensity

It called both the ordinary and trigger SDL rumble functions. A Wine HID trace
from FH6 recorded thousands of four-channel haptics updates, including varying
left-only and right-only trigger values. The game and Wine path were therefore
not collapsing trigger rumble into the two main channels.

Under the Steam Linux Runtime build, Wine's SDL device was the evdev/xpad
`Generic X-Box pad`. A zero-intensity SDL probe inside the same runtime could
not see a direct USB joystick. Pressure Vessel rejected attempts to share
`/dev/bus/usb` because `/dev` is reserved by the container.

## Final path

Switching FH6 to the native CachyOS Proton build removed that container boundary
and allowed Wine's SDL backend to open the same direct USB/GIP device that had
passed the standalone test. The user then physically confirmed genuine,
game-controlled impulse-trigger feedback while driving.

The final working path was:

```text
FH6
  -> Wine four-channel haptics
  -> sdl2-compat
  -> SDL3 direct GIP
  -> libusb
  -> independent main and trigger motors
```

## Approaches deliberately not used

- No trigger vibration synthesized from ordinary rumble.
- No trigger-position click effect.
- No community xpad fork that mirrors two channels into four motors.
- No broad `0666` USB permission rule.
- No kernel, SDL, Wine, or Proton source patch.

## Current limitation

The udev permission and direct SDL/GIP approach should generalize to other USB
GIP controllers, but protocol family, firmware mode, SDL support, metadata, and
physical motor layout must all be verified. Bluetooth is a different transport
and is not covered by the confirmed result.
