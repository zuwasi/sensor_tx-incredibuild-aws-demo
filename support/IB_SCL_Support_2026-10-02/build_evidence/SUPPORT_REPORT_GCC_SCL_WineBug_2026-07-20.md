# IncrediBuild Support Report — GCC/SCL Misguidance + SCL Wine `GetIpAddrTable` Crash

**Date:** 2026-07-20
**Customer environment:** IncrediBuild for Windows 10.36.0 (build 18415)
**Coordinator ID:** `AB42BA03-531E-4A75-828B-3A4C3BC7BD34`
**License tier:** Internal, 29 days remaining, 150 floating helper cores, SaaS broker enabled
**Submitted by:** End-user (SCD training project — Eclipse CDT + MinGW GCC sensor_tx/ThreadX demo)

---

## 1. Executive summary

We engaged IncrediBuild support to confirm whether **GCC/MinGW** builds could be accelerated using a **Linux SCL Helper on AWS EC2**. Support explicitly told us "it is OK and gcc is supported." Based on that guidance we invested approximately **6+ hours of engineering time** and incurred **AWS EC2 charges** (c6i.2xlarge, On-Demand, us-east-1) standing up the SCL infrastructure.

After the SCL Helper failed to register, we performed a full root-cause investigation and discovered **three independent blockers**, two of which are product-level issues that support should have flagged before advising us to proceed:

1. **GCC is not supported by SCL** — your own published documentation contradicts the guidance we received.
2. **The SCL container crashes on startup** — `BuildService.exe` calls the deprecated Win32 `GetIpAddrTable` API, which returns `ERROR_FILE_NOT_FOUND (2)` under the Wine 8.0.2 bundled in your `scl:latest_10.36.0` image. The agent never registers.
3. **No GCC interception profile ships with IncrediBuild** — `BuildCache_profile.xml` intercepts cl, link, lib, clang-cl, clang++, clang-tidy, midl, etc. but has no entry for `gcc` or `g++`.

We are submitting this report to (a) request a fix for the SCL Wine crash, (b) flag the incorrect pre-sales guidance, and (c) ask for an official GCC interception profile or a clear statement that GCC acceleration is unsupported so we can plan accordingly.

---

## 2. Environment

| Component | Value |
|---|---|
| IncrediBuild for Windows | 10.36.0 (build 18415) |
| Coordinator | Windows, hostname `ai-logic-lab`, Tailscale IP `[COORDINATOR_TS_IP]` |
| SCL image | `public.ecr.aws/incredibuild/scl:latest_10.36.0` |
| Image digest | `sha256:70277e83bfe6c1d64762156ed1812d29676219979ed6174ed62592b45d6ab0e8` |
| Image size / age | 11.6 GB, built 6 days ago |
| Wine in container | `wine-8.0.2` (`wine-stable 8.0.2~jammy-1`) |
| Container OS | Ubuntu 22.04.5 LTS (Jammy) |
| EC2 instance | `c6i.2xlarge`, us-east-1, On-Demand |
| Networking | `--network host` + Tailscale mesh (direct connection, ~141 ms RTT) |
| Coordinator to Helper | TCP 31100/31104/31105 all reachable from container; Windows firewall rules enabled |
| Helper container env | `COORD_IP=[COORDINATOR_TS_IP]`, `HELPER_CORES=8`, `SERVICE_PORT=31105`, `HELPER_PORT=31106` |
| Build project | Eclipse CDT, MinGW UCRT64 GCC 15.2.0, ThreadX RTOS sensor demo |

Container launch command (matches your official single-container docs):

```bash
docker run --detach --restart unless-stopped \
  --hostname ib-scl-helper \
  --name IncredibuildHelper \
  --env COORD_IP=[COORDINATOR_TS_IP] \
  --env HELPER_CORES=8 \
  --network host \
  public.ecr.aws/incredibuild/scl:latest_10.36.0
```

---

## 3. Problem 1 — GCC is not supported by SCL (contradicts support guidance)

### What support told us
> "It is OK and gcc is supported."

This was given in response to a direct question about whether GCC/MinGW compiles would be distributed to an AWS Linux SCL Helper. Relying on this, we built the full AWS + Tailscale + SCL stack.

### What your documentation says
Your official SCL installation page ([Linux Helper (SCL) Installation](https://docs.incredibuild.com/win/latest/windows/Linux%20Helper%20(SCL)%20Installation.htm), last updated 2025-12-22) includes an explicit **Limitations** section:

> SCL accelerates the following tools only (tasks using other tools are not supported and are not distributed to SCL Helpers):
> - Cl.exe
> - Cl-filter.exe
> - Clang.exe
> - ShaderCompileWorker.exe (Unreal Engine)
> - Clang++.exe (Nintendo compiler)
> - Clang-Orbis.exe (PS4 compiler)
> - Clang-Prospero.exe (PS5 compiler)

**GCC (`gcc.exe` / `g++.exe`) is not in this list.** Our project uses `CC = gcc` / `CXX = g++` (MinGW UCRT64 15.2.0), so even if the container registered perfectly, **zero compile tasks would have been distributed to the SCL Helper**.

### Impact
We would have spent additional hours debugging "why isn't the helper getting tasks" before discovering the tool list limitation — a limitation support should have flagged immediately. This is the core of our complaint: the guidance sent us down a path that was guaranteed to fail at the task-distribution stage regardless of any other fix.

### Request
1. Confirm in writing whether GCC/MinGW acceleration via SCL is supported or not.
2. If it is supposed to be supported, provide the interception profile entry for `gcc`/`g++` and update the Limitations documentation.
3. If it is not supported, ensure support staff are aware so other customers are not misadvised.

---

## 4. Problem 2 — SCL container crashes: `GetIpAddrTable` returns `ERROR_FILE_NOT_FOUND` under Wine

Even setting the GCC question aside, **the SCL Helper never registers with the Coordinator** because `BuildService.exe` crashes on startup. This is a product defect in the SCL image.

### Evidence

#### 4.1 Helper-side `BuildService.log` (inside container Wine prefix)
Path: `/root/.wine/drive_c/Program Files (x86)/Incredibuild/Logs/BuildService.log`

```
----------Exception---------------------20-07-2026 12:51:05.539----------------
EWin32Error   Code=0x0EEDFADE   Flags=0x1   Addr=0x7B012566   PID=660   TID=684   Build=18415

Failed to query size of IP address table: File not found (2)

----------Raised Exception--------------20-07-2026 12:51:05.540----------------
EWin32Error PID=660 TID=684 Build=18415

Failed to load network configuration
Failed to query size of IP address table: File not found (2)
```

The same crash reproduces identically on a **fresh container** (we destroyed and recreated it to rule out state corruption from an initial start that occurred while Tailscale was temporarily down).

#### 4.2 Coordinator-side `CoordinatorCore.log`
Path: `C:\Program Files (x86)\IncrediBuild\Logs\CoordinatorCore.log`

```
20-07-2026 15:35:04.038 Agent ib-scl-helper turned offline
20-07-2026 15:36:02.696 Deleting offline agent ib-scl-helper
20-07-2026 15:42:21.702 Agent ib-scl-helper turned offline
20-07-2026 15:42:22.027 Agent ib-scl-helper turned online
20-07-2026 15:42:37.853 Agent ib-scl-helper turned offline
20-07-2026 15:43:34.574 Deleting offline agent ib-scl-helper
20-07-2026 15:50:01.705 Agent ib-scl-helper turned offline
20-07-2026 15:51:01.352 Deleting offline agent ib-scl-helper
```

The agent flickers online for ~15 seconds, then crashes and is deleted. It never stabilizes. This pattern repeats on every container restart.

#### 4.3 `Setup.exe` hangs at "Verifying access permissions"
The container's `ib_setup_console.exe` spawns `Setup.exe`, which reaches `Verifying access permissions...` and then hangs indefinitely because the `BuildService.exe` agent it is waiting on has already crashed. In our first run, `Setup.exe` (PID 163) stayed in that state for **over 3 hours** until we manually restarted the container.

Container logs at the hang point:
```
Preparing to install...
Gathering information...
Verify user login data...
Verifying access permissions...
<hang — no further output>
```

### Root cause analysis

The crash is caused by IncrediBuild's Delphi `BuildService.exe` calling the **deprecated** Win32 `GetIpAddrTable` API. Under the Wine 8.0.2 bundled in your SCL image, this API returns `ERROR_FILE_NOT_FOUND (2)`.

We verified that Wine's IP Helper stack is otherwise functional:

```
$ sudo docker exec IncredibuildHelper bash -c 'WINEDEBUG=+iphlpapi wine ipconfig /all'
0494:trace:iphlpapi:GetNetworkParams info 00000000, size 0061FB3C
0494:trace:iphlpapi:GetNetworkParams info 00728E00, size 0061FB3C
0494:trace:iphlpapi:GetIpStatisticsEx 0061FA44 2
    Hostname. . . . . . . . . . . . . : ib-scl-helper
    Primary DNS suffix. . . . . . . . : tail84436b.ts.net
    Node type . . . . . . . . . . . . : Hybrid
    IP routing enabled. . . . . . . . : No
0494:trace:iphlpapi:GetAdaptersAddresses (0, 00000080, 00000000, 00000000, 0061FB3C)
```

`GetNetworkParams`, `GetAdaptersAddresses`, and `GetIpStatisticsEx` all work correctly and return the expected host/network data. Only `GetIpAddrTable` fails. This is consistent with Wine implementing the modern adapter enumeration APIs (which `ipconfig` uses) but not the legacy `GetIpAddrTable` path that IncrediBuild's Delphi code calls.

We also confirmed that `/proc/net/fib_trie`, `/proc/net/route`, and `/proc/net/if_inet6` are all readable from inside the container, and that `--network host` is in effect (the container sees the host's `ens5` and Tailscale `tailscale0` interfaces). The issue is not a network or procfs visibility problem — it is a Wine API implementation gap.

### Request
1. Patch `BuildService.exe` (or the SCL Wine build) so that it uses `GetAdaptersAddresses` instead of the deprecated `GetIpAddrTable`, or provide a Wine build where `GetIpAddrTable` is implemented against `/proc/net`.
2. Provide a timeframe for a fixed `scl:latest_10.36.x` image.
3. As a workaround, if one exists, tell us how to make `GetIpAddrTable` succeed under the current image (e.g., a registry override, a Wine config change, or an env var).

---

## 5. Problem 3 — No GCC interception profile ships with IncrediBuild

Independent of SCL, **IncrediBuild for Windows itself has no out-of-the-box interception profile for GCC/G++**. The default `BuildCache_profile.xml` (at `C:\Program Files (x86)\IncrediBuild\resources\BuildCache_profile.xml`) defines interception entries for:

- `cl`, `cl-filter`, `cld`
- `link`, `lib`, `lld-link`
- `clang-cl`, `clang++`, `clang-tidy`
- `prospero-clang`, `orbis-clang`
- `midl`, `csc`
- `ccrh850`, `ecom800`

There is **no `<Tool Filename="gcc" .../>` or `<Tool Filename="g++" .../>` entry**. The only GCC-related reference in the entire profile set is a single line in `chromium.ib_profile.xml` for `x86_64-w64-mingw32-as` (the assembler).

This means that even on a purely local IncrediBuild grid (no SCL, no cloud), wrapping a MinGW `make` invocation with `IBConsole.exe /COMMAND="make ..."` would **not distribute any GCC compile tasks** — `make` would run locally on the initiating machine only. This is consistent with what we observed: the Build Monitor showed only the local machine (`Ai-logic-lab`, 12 cores) and no helpers, even with `/USECLOUDHELPERS=True`.

### Request
1. Provide an official GCC/G++ interception profile (XML) for MinGW-w64 toolchains, or confirm that GCC interception is unsupported and must be custom-built by the customer.
2. If custom profiles are the intended path, point us to documentation for writing a `<Tool>` entry for GCC that correctly handles MinGW argument parsing (`-I`, `-D`, `-c`, `-o`, response files, etc.).

---

## 6. Cost and time impact

| Item | Detail |
|---|---|
| Engineering time | ~6 hours (AWS setup, Tailscale mesh, Docker, debugging across Windows + Linux + Wine) |
| AWS EC2 cost | c6i.2xlarge On-Demand in us-east-1, ~$0.34/hr, running for several hours (instance still up at time of writing) |
| Outcome | No working SCL acceleration; no GCC task distribution; container crashes on every start |
| Avoidable? | Yes — if support had flagged the SCL tool-list limitation and the lack of a GCC profile before we built the AWS stack |

---

## 7. What we need from IncrediBuild

1. **Acknowledgment** that GCC is not in the SCL supported-tools list, and that the guidance we received was incorrect.
2. **A fix or workaround** for the `GetIpAddrTable` / `ERROR_FILE_NOT_FOUND` Wine crash so the SCL Helper can register (this affects all SCL users on this image, not just GCC users).
3. **An official GCC interception profile** or a clear statement that GCC acceleration is customer-supported-only, with documentation for writing the profile.
4. **Guidance on whether switching our project to Clang** (which IS in the SCL supported list) will work end-to-end with SCL, including any profile configuration needed for `clang.exe` / `clang++.exe` under MinGW.

---

## 8. Attachments / log locations

All logs are available on request. Key files:

| Log | Location |
|---|---|
| Coordinator Service log | `C:\Program Files (x86)\IncrediBuild\Logs\CoordinatorService.log` (Windows) |
| Coordinator Core log | `C:\Program Files (x86)\IncrediBuild\Logs\CoordinatorCore.log` (Windows) |
| License Service log | `C:\Program Files (x86)\IncrediBuild\Logs\LicenseService.log` (Windows) |
| Helper BuildService log | `/root/.wine/drive_c/Program Files (x86)/Incredibuild/Logs/BuildService.log` (inside SCL container) |
| Build Cache profile | `C:\Program Files (x86)\IncrediBuild\resources\BuildCache_profile.xml` (Windows) |
| Container inspect | `docker inspect IncredibuildHelper` (EC2 host) |

We can provide full log dumps, `docker inspect` output, Wine debug traces (`WINEDEBUG=+iphlpapi`), and screen captures of the IncrediBuild Build Monitor on request.

---

*Prepared 2026-07-20. We would appreciate a timely response as the EC2 instance is incurring On-Demand charges while we await a resolution.*
