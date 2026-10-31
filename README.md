# frame-power-alert

A SteamVR overlay for the Steam Frame that shows a notification when the headset stops or starts charging over
USB-C, or when its battery runs low.

## Description

The program runs on the Frame as a background OpenVR application. It checks the USB-C input and battery state four
times a second and shows a notification below the center of view whenever the state changes.

| Alert        | Trigger                                    | Color | Shown for |
| ------------ | ------------------------------------------ | ----- | --------- |
| Not charging | USB-C input goes offline or drops below 9 V | Red   | 10 s      |
| Charging     | USB-C input returns at 9 V or above        | Green | 4 s       |
| Low battery  | Battery drops to 25% or below              | Amber | 10 s      |

- Only transitions raise an alert. Nothing is shown for the state at startup.
- A power change must hold for three consecutive reads (about 0.5 s) before it counts.
- Each notification has a status icon, the battery percentage and a capacity bar.
- A ring around the icon counts down until the notification hides.
- Each alert plays a short two-note chime through PipeWire.

Power state is read from sysfs:

- USB-C input: `/sys/class/power_supply/tcpm-source-psy-*` (`online`, `voltage_now`)
- Battery: `/sys/class/power_supply/max1720x_bat*` (`capacity`)

## Requirements

On the Frame:

- SteamOS with SteamVR installed at `/opt/steamvr` (provides `libopenvr_api.so`)
- `g++` with C++17 support
- PipeWire's `pw-play`
- The Noto Sans Bold font at `/usr/share/fonts/noto/NotoSans-Bold.ttf`

On the machine you deploy from:

- A POSIX shell with `ssh` and `scp`
- SSH access to the Frame

## Installation

`deploy.sh` copies the sources to the Frame, builds them there and installs a systemd user service.

```sh
./deploy.sh
```

| Variable         | Default      | Purpose                                   |
| ---------------- | ------------ | ----------------------------------------- |
| `FRAME`          | `steamframe` | SSH destination of the Frame              |
| `FRAME_SSH_OPTS` | (empty)      | Extra options passed to every `ssh`/`scp` |

Example:

```sh
FRAME=steamos@192.168.1.50 ./deploy.sh
```

The script installs:

- `~/.local/bin/frame-power-alert`
- `~/.config/systemd/user/frame-power-alert.service`

The service is enabled under `steamvr.service`. It starts and stops with SteamVR and restarts after a crash.

## Usage

These commands run on the Frame.

| Task                                    | Command                                            |
| --------------------------------------- | -------------------------------------------------- |
| Play each alert once in the headset     | `~/.local/bin/frame-power-alert --demo`            |
| Check the service                       | `systemctl --user status frame-power-alert`        |
| View logs                               | `journalctl --user -u frame-power-alert`           |

The demo runs alongside the service and exits when done.

To render each alert to a PNG without a headset, run this from the deploy machine:

```sh
./deploy.sh preview
```

The images are written to `preview/`.

## Configuration

Thresholds, timings, size and placement are constants at the top of `src/main.cpp`. Colors, labels and icons are
set in `styleFor` in `src/pill.h`. Run `./deploy.sh` again after changing them.

## Uninstall

On the Frame:

```sh
systemctl --user disable --now frame-power-alert
rm ~/.local/bin/frame-power-alert ~/.config/systemd/user/frame-power-alert.service
rm -r ~/frame-power-alert
```

## Project layout

| Path                        | Contents                                                 |
| --------------------------- | -------------------------------------------------------- |
| `src/main.cpp`              | Power polling, alert logic, overlay placement, animation |
| `src/pill.h`                | Software renderer for the notification and ring layers   |
| `src/icons.h`               | Lucide icon paths                                        |
| `src/chime.h`               | Chime synthesis and playback                             |
| `tools/preview.cpp`         | Renders each alert to a PNG                              |
| `frame-power-alert.service` | systemd user unit                                        |
| `deploy.sh`                 | Build and install script                                 |
| `vendor/`                   | Third-party headers                                      |

## Third-party code

- [OpenVR](https://github.com/ValveSoftware/openvr) (`vendor/openvr.h`), BSD 3-Clause
- [stb_truetype and stb_image_write](https://github.com/nothings/stb) (`vendor/stb_*.h`), MIT or public domain
- [Lucide](https://lucide.dev) icons (`src/icons.h`), ISC
