# sensor_tx: ThreadX Demo with IncrediBuild + AWS SCL Helper Acceleration

A fully self-contained **ThreadX RTOS sensor demo** (Eclipse CDT project, MinGW/Clang
on Windows) that builds from the command line and can be accelerated **5x+** by
distributing clang compile tasks to a **Linux SCL Helper running on AWS EC2** via
IncrediBuild.

Reference results from the reference machine (Intel Core Ultra 9 275HX, 24 cores,
+ 8 remote cores on a `c6i.2xlarge` EC2 helper, clean builds):

| Build mode | Clean-build time |
|---|---|
| Standard make (1 core) | ~81 s |
| IncrediBuild (local + AWS SCL helper) | ~15 s (**5.27x speedup**) |

## What you need (all licensed tools)

| Tool | Used for | Where to get it |
|---|---|---|
| MSYS2 UCRT64 **clang** | the actual compiler | https://www.msys2.org (`pacman -S mingw-w64-ucrt-x86_64-clang`) |
| **MinGW make** (`C:\MinGW\bin\make.exe`) | build driver | https://osdn.net/projects/mingw/ or `mingw32-make` |
| **IncrediBuild for Windows** (tested 10.36.0) | build orchestration/distribution, SCL license required for Linux helpers | https://www.incredibuild.com (commercial license) |
| **AWS EC2** account | hosts the Linux SCL Helper (Ubuntu 22.04, `c6i.2xlarge` or similar) | https://aws.amazon.com |
| **AWS CLI** | `start_and_build.bat` / `stop_ec2.bat` start/stop the instance | `winget install Amazon.AWSCLI` |
| **Tailscale** | recommended mesh VPN so the helper can reach your Coordinator | https://tailscale.com (free tier is fine) |

Parasoft C/C++test is **not required to build or run this demo**; the project
originates from a Parasoft Eclipse demo but the build pipeline here is plain
clang + make + IncrediBuild.

## Repository layout

```
sensor_threadx.c/.h      ThreadX sensor demo application (the demo source)
sensor_tx.c, time_monitor.c, sensor_asm.h, tx_user.h
Makefile                 bare-metal target (sensor_tx.exe)
Makefile.threadx         ThreadX target (sensor_threadx.exe) - the accelerated target
threadx/                 Eclipse ThreadX kernel sources (Apache-2.0/Eclipse Public License)
threadx_port/            Windows/MinGW port layer
profile.xml              IncrediBuild interception profile (distributes clang)
build.ps1                command-line build (standard or -UseIncrediBuild)
benchmark.ps1            clean-build benchmark: standard vs IncrediBuild
start_and_build.bat      one-click: start EC2 + verify helper + accelerated build
stop_ec2.bat             stop the EC2 instance to stop billing
ib_aws/                  CloudFormation template + user-data to create the SCL helper
```

## Step 1 - Build locally (no AWS needed)

```powershell
git clone https://github.com/zuwasi/sensor_tx-incredibuild-aws-demo.git
cd sensor_tx-incredibuild-aws-demo
.\build.ps1                 # clean + build, produces sensor_threadx.exe
.\build.ps1 -Run            # build and run it
.\build.ps1 -UseIncrediBuild   # distribute to your local IncrediBuild grid
```

## Step 2 - Create the AWS SCL Helper (one time)

Follow **[ib_aws/README.md](ib_aws/README.md)**. Two options:

- **CloudFormation (recommended):** deploy `ib_aws/scl-helper.yaml` with your
  Coordinator IP, key pair, and instance type.
- **Manual:** launch Ubuntu 22.04 EC2 and boot it with `ib_aws/scl-helper-userdata.sh`
  as user data.

The helper runs the official `public.ecr.aws/incredibuild/scl:latest_<version>`
Docker container, which must match your installed IncrediBuild version exactly.

The helper must be able to reach your Windows Coordinator on TCP 31105 (and task
ports). With Tailscale installed on both sides this works across the internet
without opening inbound ports on your PC.

## Step 3 - One-click accelerated build

1. Copy `demo-config.example.bat` to `demo-config.bat` and fill in your EC2
   instance ID, region, project path, and helper's Tailscale IP.
2. Double-click (or run from a terminal) `start_and_build.bat`. It will:
   1. ensure the IncrediBuild services are running,
   2. start your EC2 instance and wait until it is running,
   3. wait for Tailscale connectivity to the helper,
   4. verify the helper registered with your Coordinator,
   5. run the accelerated build and report where the logs are.
3. Run `stop_ec2.bat` when you are done so the instance stops billing.

To measure your own speedup, run `benchmark.ps1` with the helper online
(each run cleans first, so it is a cold-vs-cold comparison).

## Where the results are

- `build_threadx.log` / `build_bare.log` - raw compiler output
- `ib_threadx.log` - IncrediBuild build log
- `ib_threadx.bmon` - IncrediBuild Build Monitor capture (open in the
  IncrediBuild Build Monitor UI to see which agent ran each task)

## KnownIncrediBuild/SCL gotchas (learned the hard way)

- SCL officially accelerates **clang** (and MSVC cl), **not gcc**. This demo uses
  MSYS2 clang for that reason.
- The SCL image tag must match your IncrediBuild version exactly, or the helper
  silently never registers.
- SCL runs the Windows toolchain under Wine; a few Win32 APIs (e.g.
  `GetIpAddrTable`) can misbehave depending on the image version. If the helper
  container restarts in a loop, check `docker logs IncredibuildHelper`.

## License

This project's own code (`sensor_*`, `time_monitor.*`, `tx_user.h`, scripts,
makefiles, AWS templates) is licensed under the **GNU GPL v2**
(see [LICENSE](LICENSE)). The `threadx/` directory is Eclipse ThreadX, which is
distributed under its own upstream license (Eclipse Public License 2.0 /
Apache-2.0 style headers in each file).
