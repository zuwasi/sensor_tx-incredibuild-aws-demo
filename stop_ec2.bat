@echo off
REM ============================================================
REM  Stop EC2 instance to save costs
REM  Reads EC2_ID and REGION from demo-config.bat
REM ============================================================
setlocal

set "SCRIPT_DIR=%~dp0"
if not exist "%SCRIPT_DIR%demo-config.bat" (
    echo ERROR: demo-config.bat not found next to this script.
    echo Copy demo-config.example.bat to demo-config.bat and edit it.
    pause
    exit /b 1
)
call "%SCRIPT_DIR%demo-config.bat"

echo.
echo ============================================================
echo   Stopping EC2 instance %EC2_ID%
echo ============================================================
echo.
aws ec2 stop-instances --instance-ids %EC2_ID% --region %REGION% --query "StoppingInstances[0].CurrentState.Name" --output text
echo Waiting for stopped state...
aws ec2 wait instance-stopped --instance-ids %EC2_ID% --region %REGION%
for /f "delims=" %%i in ('aws ec2 describe-instances --instance-ids %EC2_ID% --region %REGION% --query "Reservations[0].Instances[0].State.Name" --output text') do set STATE=%%i
echo.
echo EC2 state: %STATE%
echo.
echo Optionally stop IncrediBuild services to free resources:
echo   net stop Incredibuild_Agent
echo   net stop "Incredibuild BuildCache"
echo   net stop Incredibuild_CoordinatorService
echo   net stop "Incredibuild Endpoint Service"
echo   net stop Incredibuild_LicenseService
echo   net stop Incredibuild_Manager
echo.
pause
