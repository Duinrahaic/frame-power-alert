# frame-power-alert

Head-locked pill on the Steam Frame when its USB-C input stops charging it
(input offline or below 9 V, e.g. a flat PD100 falling back to 5 V), and a green
"Charging" pill when power returns. An amber "Low battery" pill when the headset battery
drops to 25%. Only transitions alert; nothing shows for the state at startup.

Runs on the Frame as a user service bound to `steamvr.service`; reads
`/sys/class/power_supply/tcpm-source-psy-*` four times a second.

- `./deploy.sh` — build on the Frame and (re)install the service
- `./deploy.sh preview` — also render each state to `preview/*.png`
- On the Frame, `frame-power-alert --demo` — play each alert once in the headset, with sound
- Logs: `journalctl --user -u frame-power-alert`
- Remove: `systemctl --user disable --now frame-power-alert && rm ~/.local/bin/frame-power-alert ~/.config/systemd/user/frame-power-alert.service`

Tuning constants (timings, threshold, position) are at the top of `src/main.cpp`.
