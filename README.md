<div align="center">

# Frame Power Alert

A SteamVR overlay that alerts you when your Steam Frame stops charging.

<img src="docs/not-charging.png" width="32%" alt="Not charging alert">
<img src="docs/low-battery.png" width="32%" alt="Low battery alert">
<img src="docs/charging.png" width="32%" alt="Charging alert">

</div>

<div align="center">

[Key Features](#key-features) •
[Pre-Requisites](#pre-requisites) •
[Installation](#installation) •
[Usage](#usage) •
[Configuration](#configuration) •
[Contact & Support](#contact--support) •
[Other Projects](#other-projects) •
[Mentions](#mentions) •
[License](#license)

</div>

<br>
<br>

## Key Features

<ul style="font-size: 16px;">
  <li>🔌 <strong>Charge Monitoring</strong> - Alerts when the USB-C input goes offline or drops below 9 V, and again when charging resumes.</li>
  <li>🪫 <strong>Low Battery Warning</strong> - Alerts when the headset battery drops to 25%.</li>
  <li>⚡ <strong>Fast Detection</strong> - Checks power four times a second and alerts within about half a second of a change.</li>
  <li>👓 <strong>In-Headset Overlay</strong> - Shows below your view with the battery level and a ring that counts down until it hides.</li>
  <li>🔔 <strong>Audio Cue</strong> - Plays a short chime with each alert.</li>
  <li>🚀 <strong>Runs on the Frame</strong> - Starts and stops with SteamVR. No PC software required.</li>
</ul>

| Alert        | Trigger                                     | Shown for |
| ------------ | ------------------------------------------- | --------- |
| Not charging | USB-C input goes offline or drops below 9 V | 10 s      |
| Charging     | USB-C input returns at 9 V or above         | 4 s       |
| Low battery  | Battery drops to 25% or below               | 10 s      |

Only changes raise an alert. Nothing is shown for the state at startup.

## Pre-Requisites

- A Steam Frame with SSH access.
- SteamVR installed on the Frame at `/opt/steamvr`.
- `g++` and PipeWire's `pw-play` on the Frame (included with SteamOS).
- A machine with `ssh`, `scp` and a POSIX shell to deploy from.

## Installation

Clone the repository and run the deploy script. It copies the sources to the Frame, builds them there and installs a
systemd user service.

```sh
git clone https://github.com/Duinrahaic/frame-power-alert.git
cd frame-power-alert
./deploy.sh
```

The script connects to the SSH host `steamframe` by default. To use a different host or extra SSH options:

```sh
FRAME=steamos@192.168.1.50 ./deploy.sh
FRAME_SSH_OPTS="-o Port=2222" ./deploy.sh
```

The service starts with SteamVR and restarts automatically after a crash.

### Uninstall

Run on the Frame:

```sh
systemctl --user disable --now frame-power-alert
rm ~/.local/bin/frame-power-alert ~/.config/systemd/user/frame-power-alert.service
rm -r ~/frame-power-alert
```

## Usage

Run on the Frame:

| Task                       | Command                                     |
| -------------------------- | ------------------------------------------- |
| Play each alert once       | `~/.local/bin/frame-power-alert --demo`     |
| Check the service          | `systemctl --user status frame-power-alert` |
| View logs                  | `journalctl --user -u frame-power-alert`    |

To re-render the images in `docs/` after changing the design, run `./deploy.sh preview` from the deploy machine.

## Configuration

| Setting                                  | Location                       |
| ---------------------------------------- | ------------------------------ |
| Thresholds, timings, size and placement  | Constants at the top of `src/main.cpp` |
| Colors, labels and icons                 | `styleFor` in `src/pill.h`     |
| Chime notes                              | `main` in `src/main.cpp`       |

Run `./deploy.sh` again after making changes.

## Contact & Support

For additional support, please consider joining my Discord server [here](https://discord.gg/aZQfy6H9fA).

[![Ko-fi](https://img.shields.io/badge/Ko--fi-F16061?style=flat-square&logo=ko-fi&logoColor=white)](https://ko-fi.com/Duinrahaic)
[![X](https://img.shields.io/badge/X-000000?style=flat-square&logo=x&logoColor=white)](https://x.com/Duinrahaic)
[![Bluesky](https://img.shields.io/badge/Bluesky-00A0DC?style=flat-square&logo=bluesky&logoColor=white)](https://bsky.app/profile/duinrahaic.app)
[![GitHub](https://img.shields.io/badge/GitHub-181717?style=flat-square&logo=github&logoColor=white)](https://github.com/Duinrahaic)

## Other Projects

- [FitOSC](https://github.com/Duinrahaic/FitOSC) - An Avatar Treadmill Controller for VRChat.
- [VRCDollyManager](https://github.com/Duinrahaic/VRCDollyManager) - A Dolly Manager for VRChat.
- [VRCPictureToClipboard](https://github.com/Duinrahaic/VRCPictureToClipboard) - A tool to copy the latest picture from VRChat to your clipboard.

## Mentions

Frame Power Alert is built on the shoulders of these open-source libraries:

- [OpenVR SDK](https://github.com/ValveSoftware/openvr) - Valve's VR hardware access SDK.
- [stb](https://github.com/nothings/stb) - Single-file libraries for font rendering and PNG output.
- [Lucide Icons](https://github.com/lucide-icons/lucide) - Open-source icon toolkit.

## License

Frame Power Alert is licensed under the [MIT License](LICENSE). Modify and distribute as needed.

---
