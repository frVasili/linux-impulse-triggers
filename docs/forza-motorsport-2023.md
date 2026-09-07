# Forza Motorsport (2023): controls and genuine impulse triggers

Confirmed by the project owner on 2026-09-06: menu controls and in-game
impulse-trigger feedback work on the tested USB 8BitDo controller. This report
combines human testing with Wine traces. It does not claim compatibility with
other controllers, distributions, Bluetooth transports, or arbitrary Proton builds.

## Tested configuration

| Component | Version / configuration |
|---|---|
| Game | Steam Forza Motorsport (2023), AppID `2440510` |
| Controller | 8BitDo Ultimate Wired Controller for Xbox, `2dc8:2015`, USB GIP |
| Host | CachyOS, kernel `7.2.2-1-cachyos` |
| Proton | `GE-Proton11-3-FM`, with the WGI controller-ID workaround below |
| Runtime | Steam Linux Runtime 4, `steamrt4_platform_4.0.20260805.254769` |
| Private controller libraries | SDL 3.4.16, sdl2-compat 2.32.72, libusb 1.0.30 |
| Steam Input | Disabled |
| Xbox authentication | Existing audited Xodus `forza-online` setup |

The working path is:

```text
Motorsport Windows.Gaming.Input
  -> Wine controller identity / gamepad / four-channel haptics
  -> SDL2 compatibility -> SDL3 direct GIP -> libusb
  -> two grip motors and two independent trigger motors
```

GE-Proton and these controller libraries run inside Steam's container. Native
CachyOS Proton is not required for this result.

## Startup prerequisites retained

The game already launched and signed in using
[AllanVester's GE-Proton11-3-FM](https://github.com/AllanVester/proton-ge-custom-forza-motorsport/releases/tag/GE-Proton11-3-FM)
and [Xodus](https://github.com/AllanVester/xodus/tree/forza-online).
The controller work retained this startup/authentication setup.

- Proton source tag commit: `a353dffe9e4400e359fada4af1daec91215c85be`.
- Wine source revision: `a6e6199f81aeb8a96e8ff281e781f5eba2146fe7`.
- Xodus source revision: `518a0ebdbf019113f0e21cfc80c8414fe976f082`.
- A legitimately obtained Microsoft `xgameruntime.dll.threading` was already
  installed for the startup workaround. No Microsoft binaries are distributed here.

Xodus handles Xbox authentication credentials. Review its source and setup
before using it; sign in through the official Microsoft page, never by sharing
credentials in an issue. The tested service used a user-owned mode-0600 Unix
socket inside a mode-0700 runtime directory. Normal logging used `XODUS_LOG=info`.
No fresh sign-in was needed for the controller fix.

## First failure: two-motor SDL fallback

Inside the stock Runtime 4 environment, SDL 3.4.14 opened the controller as
`Generic X-Box pad` through evdev. Ordinary rumble capability was true; trigger
capability was false. The usual direct-GIP hints alone did not change that.

The raw USB node was visible and accessible. This corrects the earlier broad
claim that the Steam container necessarily prevents direct USB access. Device
visibility and a functioning SDL/libusb backend are separate requirements.

Private copies of the host's SDL3, sdl2-compat and libusb were placed in a
project-local directory. A per-game launcher prepended that directory to
`LD_LIBRARY_PATH` **after entering the runtime, immediately before Proton**.
The runtime then opened the direct GIP interface, recognized the full 8BitDo
name, and advertised both ordinary and trigger rumble. The SDL2 probe opened
the same controller. The actual game's Wine device process was checked to
confirm it mapped all three private libraries.

The launcher used these controller settings:

```sh
SDL_HIDAPI_LIBUSB=1
SDL_HIDAPI_LIBUSB_WHITELIST=0
SDL_JOYSTICK_HIDAPI=1
SDL_JOYSTICK_HIDAPI_XBOX=1
SDL_JOYSTICK_HIDAPI_XBOX_ONE=1
SDL_JOYSTICK_HIDAPI_GIP=1
SDL_JOYSTICK_HIDAPI_GIP_RESET_FOR_METADATA=0
PROTON_PREFER_SDL=1
```

In this GE build, `PROTON_PREFER_SDL=1` sets `PROTON_DISABLE_HIDRAW=1` and
`PROTON_NO_STEAMINPUT=1`. Here that selects the direct four-motor SDL path;
those flags alone would not turn evdev into a trigger-capable device.

Do not use `SDL_DYNAMIC_API` / `SDL3_DYNAMIC_API` as interchangeable shortcuts:
they also reached a Windows SDL helper, which tried to load a Linux library
and displayed an error. That approach was rejected in a disposable prefix.

## Second failure: the game rejected the physical controller ID

**SDL enumeration was not enough.** The owner tested the library-only fix and
confirmed the game still ignored the controller. XInput polling in the first
trace came from the Xalia helper, not proof of input reaching Motorsport.

With `WINEDEBUG=+hid,+input`, the game's actual Windows.Gaming.Input path showed:

1. Wine created RawGameController and Gamepad objects for the physical device.
2. Wine found all four haptic capabilities.
3. Motorsport requested `IRawGameController2.NonRoamableId`.
4. The provider returned `E_NOTIMPL`; the game released the controller.

That Wine revision implemented this identity only for Steam virtual VID:PID
`28de:11ff`. The decisive fix came from the published exact-build workaround in
[volcmen/forza-motorsport-linux](https://github.com/volcmen/forza-motorsport-linux/blob/022fc3bdcfab4c73abc5ec518b04335fcd4299bd/manifests/supported-builds.toml).
Credit for the published workaround belongs to that project. We independently
matched the installed DLL hashes and inspected its disassembly before applying it.

For **this exact `windows.gaming.input.dll` only**:

| File offset | Original bytes | Replacement |
|---|---|---|
| `0x14932` | `75 3a` | `90 90` |
| `0x1493a` | `75 32` | `90 90` |

Original SHA-256:
`57538166ba052dc763232880f18a4b48a07ab735a61b5b8fbf5a8f56a05f2ca5`

Patched SHA-256:
`f520d9b4d44d9cdc11e67396583a1ccc8c53eca60e0b3a69233f358556b59245`

The two conditional branches reject non-Steam VID/PID values. Removing those
branches allows the existing ID formatter to run for this physical controller.
No input-state or haptic-output code was changed.

With Motorsport closed, originals were backed up and these two files were
replaced atomically, rather than edited through potentially shared hard links:

- `GE-Proton11-3-FM/files/lib/wine/x86_64-windows/windows.gaming.input.dll`
- `steamapps/compatdata/2440510/pfx/drive_c/windows/system32/windows.gaming.input.dll`

Both original and resulting hashes were verified. No third-party installer,
storage patch, or downloaded DLL was used. Do not apply these offsets to any
other hash. This is a version-specific workaround, not the fuller source fix
for stable physical identities: it reuses the existing formatter, including
its process-dependent identity. See the upstream project's source-fix notes.

After relaunch, Motorsport repeatedly called `Gamepad.GetCurrentReading`
instead of discarding the controller. The owner confirmed menu input worked,
then confirmed the driving/trigger test worked.

## Independent output evidence

A saved driving trace contained 1,664 four-channel SDL haptic updates:

| Channel | Updates with a nonzero value |
|---|---:|
| Main low-frequency | 309 |
| Main high-frequency | 1,637 |
| Left trigger | 208 |
| Right trigger | 546 |

There were 201 updates with LT active and RT zero, and 539 with RT active and
LT zero. Example tuples `(main-low, main-high, LT, RT)`:

```text
(0, 12681, 9174, 0)
(0,  4392,    0, 10485)
```

These are game-generated independent channels, not grip-rumble mirroring or
trigger-pressure synthesis. Trace values establish the software path; the
owner's physical test establishes the felt result. Zero-intensity API success
alone was not considered proof. Raw logs and controller serials are not published.

## Reproduction, portability and rollback

This is a report of a tested local setup, not a turnkey installer. Reproduction
requires the startup prerequisites, narrowly scoped raw USB permission, a
runtime-compatible SDL/libusb stack, and verification in the exact game runtime.
Run standalone probes with the game closed so they do not compete for USB access.
Preserve an already-working game prefix and keep copies of original launch options.

The launcher retained the existing startup settings, with the socket path
derived from the current user's runtime directory:

```text
PRESSURE_VESSEL_FILESYSTEMS_RW=<user-runtime-dir>/xodus.sock
WINEDLLOVERRIDES=xgameruntime=b
PROTON_VKD3D_HEAP=1
VKD3D_CONFIG=skip_application_workarounds,descriptor_heap,avoid_image_buffer_aliasing
```

The private libraries are snapshots. Arbitrary CachyOS binaries should not be
assumed compatible with older hosts/runtimes or other CPU architectures. A
portable package should build against the target runtime and verify USB access
and all four capabilities there. Flatpak adds another permissions boundary and
was not tested. Verbose `+hid,+input` tracing was disabled after verification.

This result shows that container-based Proton can support genuine impulse
triggers. It makes FH6 on other distributions a plausible follow-up, **not a
confirmed result yet**. The GIP backend is upstream SDL functionality. Other
distributions are not inherently limited to Valve's stock Proton, and FH6 should
not receive Motorsport's ID workaround unless its own trace demonstrates the need.

To revert, close Motorsport, restore both original WGI DLLs from verified
backups, and restore the prior launch options to stop using the private library
directory. Do not reset the game prefix. Updating/reinstalling Proton can replace
the patched DLL, so recheck hashes rather than blindly reapplying offsets.

No controller firmware, kernel, xpad driver, or FH6 settings were changed.
No sudo was needed during this follow-up because the existing USB permission
rule was already installed and the changed files were user-owned.
