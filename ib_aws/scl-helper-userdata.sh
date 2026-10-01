#!/bin/bash
# ============================================================================
# IncrediBuild SCL (Linux Helper) bootstrap for AWS Ubuntu 22.04 EC2
# ----------------------------------------------------------------------------
# Launch an Ubuntu 22.04 LTS x86_64 EC2 instance (>= 8 vCPU, e.g. c6i.2xlarge)
# and paste this as the "User data" in the Advanced details section, OR run
# it manually after first boot:
#   sudo bash scl-helper-userdata.sh <COORDINATOR_IP> <IB_VERSION>
#
# Defaults (edit if you prefer not to pass args):
#   COORD_IP = 10.0.0.10        <- your Windows IncrediBuild Coordinator IP
#   IBVER    = 10.36.0          <- MUST match the IB version on your Initiator
#
# Ports the Coordinator must be able to reach on this host (security group
# inbound from the Coordinator's IP):
#   TCP 31105              (Agent Service)
#   TCP 31106 - 31113      (one task port per core + one, for 8 cores)
# Outbound: all (the Helper pulls the ECR image and calls back to Coordinator)
# ============================================================================
set -euo pipefail

COORD_IP="${1:-10.0.0.10}"
IBVER="${2:-10.36.0}"
HELPER_CORES="${3:-8}"
CONTAINER_NAME="IncredibuildHelper"

echo "==> SCL Helper bootstrap"
echo "    Coordinator IP : $COORD_IP"
echo "    IB version     : $IBVER"
echo "    Helper cores   : $HELPER_CORES"

# --- 1. Base packages -------------------------------------------------------
export DEBIAN_FRONTEND=noninteractive
apt-get update -y
apt-get upgrade -y
apt-get install -y ca-certificates curl docker.io

systemctl enable docker
systemctl start docker

# --- 2. Pull and run the SCL container -------------------------------------
docker rm -f "$CONTAINER_NAME" 2>/dev/null || true

# Image tag must match the installed IncrediBuild version EXACTLY.
# public.ecr.aws/incredibuild/scl:latest_<IBVER>
IMAGE="public.ecr.aws/incredibuild/scl:latest_${IBVER}"
echo "==> Pulling $IMAGE"
docker pull "$IMAGE"

echo "==> Starting SCL container"
# --network host so the container binds the host's network interfaces
# directly (required for the Coordinator to reach the Agent ports).
# Hostname is set to the host's hostname so it shows up uniquely in the
# IncrediBuild Monitor grid.
docker run --detach --restart unless-stopped \
       --hostname "$(hostname)" \
       --name "$CONTAINER_NAME" \
       --env COORD_IP="$COORD_IP" \
       --env HELPER_CORES="$HELPER_CORES" \
       --network host \
       "$IMAGE"

# --- 3. Verify -------------------------------------------------------------
sleep 5
echo "==> Container status:"
docker ps --filter "name=$CONTAINER_NAME" --format "table {{.Names}}\t{{.Status}}\t{{.Image}}"
echo ""
echo "==> Last container logs (tail):"
docker logs --tail 30 "$CONTAINER_NAME" || true
echo ""
echo "DONE."
echo "Open the IncrediBuild Build Monitor on your Windows Initiator and confirm"
echo "this host ('$(hostname)') appears in the Agent grid with $HELPER_CORES cores."
echo "If it does not appear, check: (1) IB version match, (2) security group"
echo "ports 31105-31113 open from the Coordinator, (3) COORD_IP reachability."