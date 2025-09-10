---
name: run_program_on_switch
description: Send the built OSP2.nro to a Nintendo Switch over the network with nxlink and run it. Use when the user asks to run/test/deploy the app on the Switch.
---

# Run program on Switch

Send a homebrew `.nro` to a Switch running the Homebrew Menu (with netloader active, Y button) and launch it:

```sh
nxlink -a {switch_ip} {nro_name}
```

- `{switch_ip}` is usually `192.168.1.12` — use that unless the user gives another address.
- `{nro_name}` with the current setup is `cmake-build-switch/OSP2.nro` (relative to the repository root).
- `nxlink` is at `/opt/devkitpro/tools/bin/nxlink` (on PATH).

So the usual invocation, from the repository root, is:

```sh
nxlink -a 192.168.1.12 cmake-build-switch/OSP2.nro
```

Notes:

- **Ask the user before running this** — it deploys to real hardware, and the Switch must be sitting on the netloader screen. Never send as an unrequested side effect of another task.
- Build the Switch target first (see CLAUDE.md, `cmake-build-switch`) so the `.nro` is up to date before sending.
- Add `-s` (`nxlink -s -a ...`) to also open the nxlink stdio server and stream the app's console/log output back — useful for debugging, but it blocks the shell while the app runs (run it in the background or with a timeout).
- If the transfer fails, the Switch is probably not on the netloader screen (press Y in the Homebrew Menu) or the IP is wrong; report that to the user instead of retrying blindly.
