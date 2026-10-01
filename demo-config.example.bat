REM ============================================================
REM  Copy this file to demo-config.bat and edit the values below.
REM  demo-config.bat is git-ignored; never commit real values.
REM ============================================================

REM Your EC2 SCL helper instance ID (from "aws ec2 run-instances" output)
set EC2_ID=i-0123456789abcdef0

REM AWS region where the instance lives
set REGION=us-east-1

REM Absolute path to THIS project (where build.ps1 lives)
set PROJECT=C:\path\to\sensor_tx-incredibuild-aws-demo

REM Tailscale IP of the EC2 helper (from the Tailscale admin console),
REM used only for the reachability pre-check.
set TS_HELPER_IP=100.x.y.z
