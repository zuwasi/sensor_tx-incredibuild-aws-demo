# IncrediBuild SCL on AWS - Build Acceleration for sensor_tx

This adds AWS Linux Helpers (IncrediBuild Smart Compatibility Layer) to your
IncrediBuild for Windows grid so that `sensor_tx` gcc compile tasks are
distributed to cheap Linux EC2 capacity, reducing build time.

> Confirmed with IncrediBuild support: gcc tasks **are** distributed to SCL
> Helpers under your SCL license (the public doc's "Limitations" list is stale).

## Architecture

```
[Windows PC]                      [AWS EC2 Ubuntu 22.04]
 IncrediBuild Initiator  <--->    SCL Helper container (Docker)
 IncrediBuild Coordinator          runs gcc via customized Wine
   |                               registers with Coordinator
   |  /USECLOUDHELPERS=TRUE
   v
 build.ps1 -UseIncrediBuild
   IBConsole.exe /COMMAND="make -f Makefile.threadx all"
     intercepts gcc/g++ -> distributes to grid (incl. AWS SCL Helper)
```

## Prerequisites (Windows side - already satisfied on this machine)

- IncrediBuild for Windows 10.36.0 installed (Initiator + Coordinator).
  - IBConsole: `C:\Program Files (x86)\IncrediBuild\IBConsole.exe` (verified)
- SCL license applied to the Coordinator (talk to your IB CSM).
- Coordinator service running and reachable from AWS on TCP 31105 + task ports.

## Step 1 - Launch the AWS SCL Helper

### Option A: CloudFormation (one-click, recommended)

```powershell
aws cloudformation deploy `
  --stack-name ib-scl-helper `
  --template-file "ib_aws\scl-helper.yaml" `
  --parameter-overrides `
      CoordinatorIP=<YOUR_COORDINATOR_PUBLIC_IP> `
      IBVersion=10.36.0 `
      HelperCores=8 `
      InstanceType=c6i.2xlarge `
      KeyName=<YOUR_EXISTING_KEYPAIR> `
      CoordinatorLocation=<YOUR_COORDINATOR_PUBLIC_IP>/32 `
  --capabilities CAPABILITY_IAM
```

Replace:
- `<YOUR_COORDINATOR_PUBLIC_IP>` - your Windows Coordinator's public IP
  (or VPN/VPC-reachable IP). The Helper must be able to reach it on TCP 31105.
- `<YOUR_EXISTING_KEYPAIR>` - an EC2 key pair in the target region.
- `--region <region>` if your default region is not where you want the Helper.

The stack creates: a new VPC + subnet + IGW, a security group opening
TCP 31105-31200 + 22 from `CoordinatorLocation`, an Ubuntu 22.04 instance
of the chosen type, installs Docker, and starts the SCL container pointed
at your Coordinator.

### Option B: Manual EC2 launch

1. Launch an Ubuntu 22.04 LTS x86_64 EC2 instance (>= 8 vCPU, e.g. c6i.2xlarge).
2. Security group inbound (from your Coordinator IP):
   - TCP 31105 (Agent Service)
   - TCP 31106-31200 (task ports - one per core + one)
   - TCP 22 (SSH, for troubleshooting)
3. Paste `scl-helper-userdata.sh` as the instance User Data, OR SSH in and run:
   ```bash
   sudo bash scl-helper-userdata.sh <COORDINATOR_IP> 10.36.0 8
   ```

## Step 2 - Confirm the Helper joined the grid

On your Windows PC, open the **IncrediBuild Build Monitor**. The AWS Helper
host should appear in the Agent grid with its hostname and the allocated
core count. If it does not appear, check in order:

1. **Version match** - the `IBVersion` / SCL image tag must equal your
   installed IncrediBuild version exactly (10.36.0 here). Mismatch = silent.
2. **Network** - from the Helper host: `nc -vz <COORDINATOR_IP> 31105`.
   From the Coordinator: `Test-NetConnection <HELPER_IP> -Port 31105`.
3. **Security group** - ports 31105-31200 open to the Coordinator's IP.
4. **Coordinator reachability** - the Coordinator must be on a public IP
   (or reachable via VPN/Direct Connect from the AWS VPC).

## Step 3 - Build with acceleration

From PowerShell, build `sensor_tx` with IncrediBuild distribution enabled:

```powershell
# Default: ThreadX target, cloud helpers on
powershell -NoProfile -ExecutionPolicy Bypass -File `
  "build.ps1" -UseIncrediBuild

# Cap the grid cores used (e.g. 64 across local + AWS helper)
... -UseIncrediBuild -MaxCpus 64

# Use only LAN helpers (disable AWS/cloud helpers for this run)
... -UseIncrediBuild -UseCloudHelpers:$false

# Build the bare-metal target with IB
... -UseIncrediBuild -Target bare
```

The script wraps `make` with:
```
IBConsole.exe /COMMAND="make -f Makefile.threadx all" `
             /USECLOUDHELPERS=TRUE /SHOWAGENT /LOG="ib_threadx.log"
```

`/SHOWAGENT` prints which Agent (local vs. AWS Helper hostname) executed
each distributed task, so you can visually confirm the AWS Helper is doing
work. The IncrediBuild build log is written to `ib_threadx.log` next to
the project; the raw make output is in `build_threadx.log`.

## Files

| File | Purpose |
|------|---------|
| `scl-helper.yaml` | CloudFormation template - one-click AWS Helper. |
| `scl-helper-userdata.sh` | Standalone bootstrap script for manual EC2 launch. |
| `../build.ps1` | Build script with `-UseIncrediBuild` switch. |

## Cost notes

- `c6i.2xlarge` (8 vCPU) on-demand is roughly $0.34/h (us-east-1, varies).
- Use a **Spot** instance for build-only Helpers (Helpers are stateless
  containers; losing one mid-build just reruns the affected tasks on the
  grid). Spot typically cuts cost ~70%.
- Stop the instance when not building. The SCL container has
  `--restart unless-stopped`, so it comes back automatically on boot.

## Tuning

- For hosts with >16 cores, run multiple SCL containers (8-16 cores each)
  with unique hostnames and non-overlapping `SERVICE_PORT` ranges. See the
  IncrediBuild SCL doc and your CSM for the multi-container script.
- Do not over-provision `HELPER_CORES` beyond physical cores.
- Keep `make -j` reasonable; IBConsole manages the real parallelism via the
  grid. Let IB control the job count rather than forcing a huge `-j`.