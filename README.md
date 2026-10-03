# sensor_tx: ThreadX Demo with IncrediBuild + AWS SCL Helper Acceleration

A fully self-contained **ThreadX RTOS sensor demo** (Eclipse CDT project, MinGW/Clang
on Windows) that builds from the command line and can be accelerated by
distributing clang compile tasks to a **Linux SCL Helper running on AWS EC2** via
IncrediBuild. It also includes a **Parasoft C/C++test static-analysis scan**
benchmark, including an honest negative result: see
[Parasoft scan section](#parasoft-cc-test-static-analysis-scan) below.

Reference results from the reference machine (Intel Core Ultra 9 275HX, 24 cores,
+ 8 remote cores on a `c6i.2xlarge` EC2 helper, cold builds, verified 3 Oct 2026):

| Workload | Standard | IncrediBuild (local + AWS SCL helper) |
|---|---|---|
| Cold build (standard `make`, serial) | 17.9 s | 18.7 s (helper active: tasks tagged `Ib-scl-helper (Core #N)`) |
| Parasoft cold scan (traced rebuild + Flow Analysis Standard, 193 files, 13 violations) | 49.8 s | 56.8 s (**no speedup**: IncrediBuild sees 0 offloadable tasks inside `cpptestcli`, see scan section) |

On this reference machine the local box already has 24 fast cores, so a small
193-file project gains little from adding 8 remote cores; the remote helper
pays off on weaker initiators or larger projects. The scan result is a
fundamental limitation, not a configuration problem: details below.

---

## Complete setup guide (from a clean Windows PC)

Everything below was verified end-to-end on Windows 11. Follow the steps in
order. Steps 1-3 install tools; steps 4-8 build and then add the AWS helper.

### Step 1 - Install the compiler and make (about 10 minutes)

1. Install **MSYS2** from https://www.msys2.org (accept the default install
   path `C:\msys64`).
2. Open the **MSYS2 UCRT64** shell from the Start menu and run:
   ```bash
   pacman -Syu
   pacman -S mingw-w64-ucrt-x86_64-clang mingw-w64-ucrt-x86_64-make
   ```
3. The build scripts expect MinGW make at `C:\MinGW\bin\make.exe`. Create it
   from the MSYS2 copy (any terminal, or Explorer):
   ```bat
   mkdir C:\MinGW\bin
   copy C:\msys64\ucrt64\bin\mingw32-make.exe C:\MinGW\bin\make.exe
   ```
4. Verify in PowerShell:
   ```powershell
   C:\msys64\ucrt64\bin\clang.exe --version     # prints clang 20.x or newer
   C:\MinGW\bin\make.exe --version              # prints GNU Make
   ```

### Step 2 - Install IncrediBuild (licensed, about 15 minutes)

1. Install **IncrediBuild for Windows** (tested with 10.36.0) from your
   IncrediBuild download portal. Choose the **Coordinator + Initiator** setup
   on this machine.
2. Apply your license. The Linux-helper (SCL) feature must be enabled on your
   license; if you are unsure, ask your IncrediBuild contact whether
   "SCL / Linux Cloud Helpers" is included.
3. Verify:
   ```powershell
   & "C:\Program Files (x86)\IncrediBuild\IBConsole.exe" /?
   ```
   If that prints help, the Initiator is installed. Check that the
   **Incredibuild_CoordinatorService** and **Incredibuild_Agent** Windows
   services are running.

### Step 3 - Install the AWS CLI and Tailscale (about 15 minutes)

```powershell
winget install Amazon.AWSCLI
winget install Tailscale.Tailscale
```

1. Configure the AWS CLI with an IAM user that can create EC2/VPC resources:
   ```powershell
   aws configure
   # AWS Access Key ID / Secret Key: from the IAM console
   # Default region name: us-east-1 (or your region - use it consistently below)
   ```
2. Log in to Tailscale (the tray icon, then "Log in"). This network is what
   lets the AWS helper reach your Coordinator without opening ports on your PC.
3. Find your **Coordinator's Tailscale IP** (you will need it in Step 5):
   ```powershell
   tailscale ip -4
   # example output: 100.101.102.103
   ```
4. Create a **Tailscale auth key** for the EC2 helper:
   go to https://login.tailscale.com/admin/settings/keys, generate a
   **reusable** auth key (starts with `tskey-auth-`), and copy it. Also create
   an **EC2 key pair** in your region if you do not have one
   (AWS console, EC2, "Key pairs", "Create key pair", name it e.g.
   `ib-scl-helper-key`).

### Step 4 - Build the demo locally (no AWS yet)

```powershell
git clone https://github.com/zuwasi/sensor_tx-incredibuild-aws-demo.git
cd sensor_tx-incredibuild-aws-demo
.\build.ps1                 # clean + build, produces sensor_threadx.exe
.\build.ps1 -Run            # build and run the demo
.\build.ps1 -UseIncrediBuild   # same build, distributed on your local cores
```

`build.ps1 -UseIncrediBuild` works with zero AWS setup: IncrediBuild will use
your local cores. The AWS helper (next steps) adds remote cores on top.

### Step 5 - Create the AWS SCL Helper (one time, about 10 minutes)

Deploy the included CloudFormation template. It creates a VPC, an Ubuntu
22.04 instance, installs Docker + Tailscale, and starts the official
IncrediBuild SCL container automatically:

```powershell
aws cloudformation deploy `
  --stack-name ib-scl-helper `
  --template-file ib_aws\scl-helper.yaml `
  --parameter-overrides `
      CoordinatorIP=<YOUR_TAILSCALE_IP_FROM_STEP_3> `
      TailscaleAuthKey=<YOUR_TSKKEY_AUTH_KEY> `
      IBVersion=<YOUR_INSTALLED_IB_VERSION e.g. 10.36.0> `
      HelperCores=8 `
      InstanceType=c6i.2xlarge `
      KeyName=ib-scl-helper-key `
      VolumeSize=30 `
  --capabilities CAPABILITY_IAM `
  --region us-east-1
```

Parameter notes:

| Parameter | What to put there |
|---|---|
| `CoordinatorIP` | the Tailscale IP from Step 3.3 (`tailscale ip -4` on Windows) |
| `TailscaleAuthKey` | the `tskey-auth-...` key from Step 3.4 |
| `IBVersion` | must match your installed IncrediBuild version **exactly** (run `& "C:\Program Files (x86)\IncrediBuild\IBConsole.exe"` help or check Add/Remove Programs), or the helper silently never joins |
| `HelperCores` | 8 for `c6i.2xlarge`; do not exceed the instance vCPU count |
| `InstanceType` | `c6i.2xlarge` (8 vCPU, roughly $0.34/h on-demand in us-east-1) |
| `KeyName` | an EC2 key pair existing in that region (for emergency SSH) |

Prefer to click through the console instead? Open the CloudFormation console,
"Create stack", upload `ib_aws/scl-helper.yaml`, and fill in the same
parameters. Manual instance launch without CloudFormation is also possible:
launch Ubuntu 22.04 and paste `ib_aws/scl-helper-userdata.sh` as user data
(see `ib_aws/README.md`, Option B).

When the stack completes, find the helper's Tailscale IP:
`aws ec2 describe-instances ...` or, simpler, it appears as machine
`ib-scl-helper` in https://login.tailscale.com/admin/machines. Copy that
`100.x.y.z` address for Step 6.

### Step 6 - One-click accelerated build

1. Copy `demo-config.example.bat` to `demo-config.bat` and fill in:
   - `EC2_ID`: your instance ID, from
     `aws ec2 describe-instances --filters Name=tag:aws:cloudformation:stack-name,Values=ib-scl-helper --query "Reservations[].Instances[].InstanceId" --output text`
   - `REGION`: the region you deployed to
   - `PROJECT`: the full path of this repository on your disk
   - `TS_HELPER_IP`: the helper's Tailscale IP from Step 5
2. Run `start_and_build.bat`. It will:
   1. ensure the IncrediBuild services are running,
   2. start your EC2 instance and wait until it is running,
   3. wait for Tailscale connectivity to the helper (pings it for up to 2 min),
   4. verify the helper registered with your Coordinator,
   5. run the accelerated build and print where the logs are.
3. Run `stop_ec2.bat` when you are done so the instance stops billing.

If step 3 warns "Tailscale may need manual login", open the Tailscale GUI on
Windows and log in again, then rerun `start_and_build.bat`.

### Step 7 - Verify the helper is actually doing work

Watch the build output: with `/SHOWAGENT` on (default in the scripts), each
compile line is tagged with the agent that executed it. Remote tasks are
executed by the EC2 helper's hostname (`ib-scl-helper`). You can also open
`ib_threadx.bmon` (Build Monitor capture) in the IncrediBuild Build Monitor
for a visual per-task timeline.

### Step 8 - Measure your own speedup

```powershell
.\benchmark.ps1
```

It runs a standard clean build and an IncrediBuild clean build, each with
`make clean` first, so the comparison is cold-vs-cold, and prints the speedup.

---

## Parasoft C/C++test static-analysis scan

This repo also runs a real **Parasoft C/C++test** scan (Flow Analysis Standard)
over the ThreadX demo, using `cpptestcli` with a traced build
(`cpptestcli ... -report <dir> -trace make -f Makefile.threadx all`).

### What was measured (3 Oct 2026, same machine)

| Scan mode | Cold-scan time | Violations | Helper tasks |
|---|---|---|---|
| Standard cold scan (no IncrediBuild) | 49.8 s | 13 | n/a |
| Cold scan under `IBConsole /USECLOUDHELPERS=True` | 56.8 s | 13 | **0** |

The IncrediBuild Coordinator reports `Max. Needed Helper Cores = 0` for the scan
build: `cpptestcli` runs the compiler-instrumentation and analysis phases in its
own child processes, and none of them are eligible for IncrediBuild
distribution, even with the interception profile active and the EC2 helper
idle and ready. The scan is therefore **not faster with IncrediBuild** (it is
slightly slower due to the tracing overhead). For scan acceleration, use
`cpptestcli`'s own local parallelism instead.

### Setting up the scan yourself

1. Install **Parasoft C/C++test Professional** and a **DTP server** (the scan
   publishes results to DTP and needs DTP-based licensing). Start the DTP
   service and make sure `https://localhost:8443` responds.
2. If DTP uses a self-signed certificate, export it
   (`https://localhost:8443` in the browser, or grab it with PowerShell) and
   import it into the JVM used by `cpptestcli`:
   ```powershell
   & "C:\parasoft\CPP_STD\cpptest\bin\jre\bin\keytool.exe" -importcert `
     -alias dtp2026 -file dtp_cert.cer `
     -keystore "C:\parasoft\CPP_STD\cpptest\bin\jre\lib\security\cacerts" `
     -storepass changeit -noprompt
   ```
3. Copy `cpptest-scan-localsettings.example.properties` to
   `cpptest-scan-localsettings.properties` and fill in your DTP URL, user and
   encoded password, plus your network-license settings. This file is
   gitignored: never commit it.
4. Run the benchmark:
   ```powershell
   .\scan_benchmark.ps1
   ```
   It runs the same cold scan twice (standard, then under IBConsole) and
   reports times, violation counts, and speedup.

### Why `analysis_stubs/` exists

The ThreadX Win32 port includes real Windows SDK headers
(`<windows.h>`, `<mmsystem.h>`, `<winbase.h>`), which the Parasoft flow-analysis
frontend cannot preprocess (it fails silently and reports 0 violations). The
minimal stub headers in `analysis_stubs/` provide just enough Win32
declarations for both clang and the Parasoft frontend; `Makefile.threadx` puts
this directory first on the include path permanently. This is also why the
planted demo bugs (a 2048-byte `memset` into a 16-byte buffer at
`sensor_threadx.c:939` and a `strcpy` into a 4-byte buffer at
`sensor_threadx.c:1389`) are reliably found: the cold scan reports **13
violations**, including both buffer overflows (BD-PARAM-UNUSED,
APS-SEC-BUFFER-OVERFLOW and friends).

---

## Repository layout

```
sensor_threadx.c/.h      ThreadX sensor demo application (the demo source)
sensor_tx.c, time_monitor.c, sensor_asm.h, tx_user.h
Makefile                 bare-metal target (sensor_tx.exe)
Makefile.threadx         ThreadX target (sensor_threadx.exe) - the accelerated target
threadx/                 Eclipse ThreadX kernel sources (upstream license)
threadx_port/            Windows/MinGW port layer
analysis_stubs/          minimal Win32 header stubs so Parasoft can preprocess the sources
profile.xml              IncrediBuild interception profile (distributes clang)
build.ps1                command-line build (standard or -UseIncrediBuild)
benchmark.ps1            clean-build benchmark: standard vs IncrediBuild
scan_benchmark.ps1       Parasoft cold-scan benchmark: standard vs IncrediBuild
cpptest-scan-localsettings.example.properties
                         template for your local Parasoft/DTP scan settings (fill in,
                         rename to cpptest-scan-localsettings.properties, never commit)
start_and_build.bat      one-click: start EC2 + verify helper + accelerated build
stop_ec2.bat             stop the EC2 instance to stop billing
ib_aws/                  CloudFormation template + user-data to create the SCL helper
```

## Troubleshooting

| Symptom | Fix |
|---|---|
| Helper never appears in the Build Monitor | IB version mismatch is the usual cause: the SCL image tag must equal your installed version exactly. Also check `docker logs IncredibuildHelper` on the EC2 host (`ssh ubuntu@<helper-tailscale-ip>`). |
| Build runs but everything stays local | The helper is not registered, or `-UseCloudHelpers:$false` was passed. Rerun `start_and_build.bat` and watch step [4/5]. |
| Build log says `Agent is not registered as initiator` / `Missing License` and runs in Stand Alone mode | Your machine is not assigned an **Initiator** role on the Coordinator. Open IncrediBuild Manager (`https://localhost:8000`), go to **Agents**, select your Windows machine, then **Set Agent Role** and set **Initiator: Fixed** (requires the license to have Fixed Initiator slots). See the gotcha below. |
| Helper shows `WorkingFor=` empty forever / tasks never dispatch | The license is missing the **SCL Helpers** product feature. Check with `"C:\Program Files (x86)\IncrediBuild\LicenseServiceConsole.exe" Status <CoordinatorId>`: `ProductFeatures` must contain `SCL Helpers` and the helper agent must show `HelperRegType="Floating"`. Without that feature the coordinator never sends tasks to the Linux helper even though it appears online. |
| Helper online and licensed but builds still show only `Local CPU N` tags | Open IncrediBuild Manager (`https://localhost:8000`), Agents page, select `ib-scl-helper`, open its `...` menu, **Set Helper Role**. If it shows `None`, that is the bug: set **Floating helper** (cores = 8) and Submit. Activating a new license or a helper agent upgrade can silently reset this role to `None` while the agent still shows `Ready` and `enabled`; the coordinator then reports `Max. Helper Cores = 0` for every build. Verified fix: after setting the role back, the same build showed `Max. Needed Helper Cores = 63, Max. Helper Cores = 8` and output tags `Agent 'Ib-scl-helper (Core #N)'`. |
| `make not found at C:\MinGW\bin\make.exe` | Do Step 1.3. |
| `aws` not recognized | Install the AWS CLI (Step 3) and open a new terminal. |
| Tailscale ping to helper fails | Log in to Tailscale on Windows; on the AWS side confirm the machine shows "Online" at login.tailscale.com/admin/machines. |
| SCL container restarts in a loop | Known Wine issue in some SCL image versions (`GetIpAddrTable` crash). Pull a newer `public.ecr.aws/incredibuild/scl:latest_<version>` image and report to IncrediBuild support with `ib_aws/` logs. |

## Known IncrediBuild/SCL gotchas (learned the hard way)

- SCL officially accelerates **clang** (and MSVC cl), **not gcc**. This demo uses
  MSYS2 clang for that reason.
- The SCL image tag must match your IncrediBuild version exactly, or the helper
  silently never registers.
- SCL runs the Windows toolchain under Wine; a few Win32 APIs can misbehave
  depending on the image version.
- Two separate license requirements must hold before remote distribution works:
  1. The license's **ProductFeatures** must include **SCL Helpers** (this is a
     separate feature from the Windows grid features; a license without it
     activates fine and still never dispatches to the Linux helper).
  2. The Windows initiator machine must be assigned an **Initiator role**
     (Agents page in IncrediBuild Manager, "Set Agent Role", Initiator: Fixed).
     Changing/activating a new license can silently reset this assignment, so
     if a build suddenly says "Agent is not registered as initiator", re-check
     the role in the Manager. You can verify the assignment with:
     ```powershell
     & "C:\Program Files (x86)\IncrediBuild\xgCoordConsole.exe" /LOCAL /EXPORTSTATUS=coord.xml
     # then look for InitiatorRegType="Fixed" and RegisteredInitiator="True"
     # on your machine's <Agent> element
     ```
  3. The same reset can hit the **helper role** of the EC2 SCL agent: after a
     license change the helper can show Ready/enabled but its **Set Helper
     Role** is silently back to `None` (0 cores), so nothing dispatches to it.
     Re-set it to **Floating helper** in the Manager (see Troubleshooting).
- A useful sanity check before building: the coordinator status XML should show
  the helper agent with `HelperRegType="Floating"` and, once a build starts,
  `WorkingForAgents="<your machine>"` while the initiator shows
  `InitiatorRegType="Fixed"`.

## Cost notes

- `c6i.2xlarge` on-demand is roughly $0.34/h in us-east-1 (varies by region).
- The instance is **stopped** by `stop_ec2.bat`; stopped instances bill only
  for EBS storage (cents per day at 30 GB).
- For a build-only helper, consider Spot capacity (typically ~70% cheaper);
  helpers are stateless, losing one mid-build just reruns affected tasks.

## License

This project's own code (`sensor_*`, `time_monitor.*`, `tx_user.h`, scripts,
makefiles, AWS templates) is licensed under the **GNU GPL v2**
(see [LICENSE](LICENSE)). The `threadx/` directory is Eclipse ThreadX, which is
distributed under its own upstream license (license headers in each file).
