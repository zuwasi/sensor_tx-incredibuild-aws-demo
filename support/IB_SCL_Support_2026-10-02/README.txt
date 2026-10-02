================================================================================
 IncrediBuild SCL Helper - Support Evidence Package
 Date: 2026-10-02
 Follow-up to: SUPPORT_TICKET_2026-07-20.txt / SUPPORT_REPORT_GCC_SCL_WineBug_2026-07-20.md
================================================================================

SUMMARY
-------
Remote task distribution to the AWS SCL helper worked on 2026-07-21 (95/95
clang tasks executed on 'Ib-scl-helper (Core #1-8)'). On 2026-10-01 the helper
container was recreated from the same image tag, and remote distribution has
not worked since: every build executes 100% of tasks as "(Local CPU N)".

The SCL Wine bug from the July ticket reproduces at EVERY startup on ALL three
image tags we tested, and the helper never opens its task ports 31106-31113
(only the agent service port 31105 binds). Per the documented port model,
initiators must connect directly to helper ports 31106 + N - 1, so the
coordinator can never dispatch tasks to this helper.

TESTED CONFIGURATIONS (all reproduce the bug)
---------------------------------------------
1. public.ecr.aws/incredibuild/scl:latest_10.36.0  digest sha256:70277e83bfe6...
2. public.ecr.aws/incredibuild/scl:latest_10.36.2  digest sha256:78f53ea6a407...
3. public.ecr.aws/incredibuild/scl:latest_10.38.0  digest sha256:f09734f74805...
   (note: with a 10.36.0 coordinator, the 10.38.0 image's installer pulled the
    coordinator-matching agent build 18415 into the container; same crash)

Launch command (identical to the official single-container docs):
  docker run --detach --restart unless-stopped \
    --hostname ib-scl-helper \
    --name IncredibuildHelper \
    --env COORD_IP=[COORDINATOR_TS_IP] \
    --env HELPER_CORES=8 \
    --network host \
    public.ecr.aws/incredibuild/scl:<tag>

THINGS ALREADY RULED OUT
------------------------
- License: valid, tier "Test Environment", 200 floating helper cores free,
  "Vnext SaaS Broker: true", no denials in LicenseService.log during builds.
- Networking: bidirectional TCP verified (helper->coordinator 31104/31105,
  coordinator/initiator->helper 31105), Tailscale direct ~150 ms.
- Firewall: coordinator-side inbound rules Allow/Any for 31104-31124.
- Interception: works (tasks are intercepted and tagged "(Local CPU N)").
- Profile: clang AllowRemote="true" + MainProcess AllowIntercept="true"
  (identical to the support-supplied profile that worked in July).
- Full EC2 instance reboot: no change.

KEY FINDING: CONFLICTING WORKER PORT REGISTRY (written by the SCL setup,
overwritten again on every container start, manual corrections do not survive):
  [HKLM\Software\Wow6432Node\Xoreax\IncrediBuild\Worker]
    ForcePortNum = 31106
    MinPort      = 31105   <- collides with BuildService's own 31105
    MaxPort      = 31105   <- an 8-core helper needs 31106-31113
Question for support: is this expected, or a setup bug? Which entrypoint
flags produce a correct per-core task-port range?

PACKAGE CONTENTS
----------------
helper_logs\
  BuildService.log               - helper build service log (10.38.0 image,
                                   coordinator-matched agent build 18415),
                                   shows the EWin32Error 0x0EEDFADE
                                   "Failed to query size of IP address table:
                                   File not found (2)" /
                                   "Failed to load network configuration"
                                   exceptions at startup and periodically
  wine_registry_and_version.txt  - Worker + BuildService registry keys and
                                   Wine version (wine-8.0.2)
  docker_inspect.json            - container config (10.38.0)
  docker_ps.txt                  - container state
  docker_container_stdout.log    - container stdout tail
  host_listening_ports.txt       - host listening sockets (only 31105 bound)

coordinator_logs\  (Windows initiator/coordinator host, IB 10.36.0)
  CoordinatorCore.log            - agent lifecycle; helper registers, then is
                                   deleted as offline; NO successful
                                   registration after the container recreation
  CoordinatorService.log         - service log
  LicenseService.log             - license status, pool data (no denials)
  BuildSystem.log                - initiator-side build service log
  IbConsole.log                  - IBConsole starts
  xgCoordConsole.log
  coordinator_status_export.xml  - xgCoordConsole /LOCAL /EXPORTSTATUS during
                                   the problem: helper Online=True,
                                   HelperCores=8, RegisteredCores=0, no builds

build_evidence\
  profile.xml                    - interception profile used (identical to the
                                   July working one)
  build_command_used.ps1         - the exact build invocation
  ib_build_log_all_local_2026-10-02.log - full build log: 95/95 tasks
                                   "(Local CPU N)", 0 remote
  SUPPORT_TICKET_2026-07-20.txt  - original July ticket (context)
  SUPPORT_REPORT_GCC_SCL_WineBug_2026-07-20.md - original July report

WHAT WE NEED
------------
1. A working scl image (10.36.x or 10.38.x) where the Wine GetIpAddrTable
   failure is fixed so the helper stably registers AND opens task ports
   31106-31113, reproducing the confirmed-working July behavior.
2. Any workaround that survives container restarts (registry override, image
   env var, entrypoint flag).
3. Confirmation whether Worker MinPort/MaxPort=31105 written by the SCL setup
   is expected or a bug, and the correct setup flags for the task-port range.

Infrastructure reference (as in July):
- Initiator/Coordinator: Windows 11 "[COORDINATOR_HOST]", 24 cores
- Helper: AWS EC2 c6i.2xlarge "ib-scl-helper", 8 cores, Ubuntu 22.04
- Connection: Tailscale mesh VPN (direct)
================================================================================
