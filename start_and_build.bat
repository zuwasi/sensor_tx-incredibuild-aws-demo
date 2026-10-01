@echo off
REM ============================================================
REM  sensor_tx - IncrediBuild AWS SCL Helper Startup Script
REM  Starts EC2, waits for Tailscale, verifies helper, builds
REM
REM  Copy demo-config.example.bat to demo-config.bat and edit it
REM  with YOUR instance ID, region, project path, and helper IP.
REM ============================================================
setlocal enabledelayedexpansion

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
echo   IncrediBuild AWS SCL Helper - Startup + Build
echo   Project: %PROJECT%
echo   EC2:     %EC2_ID% (%REGION%)
echo ============================================================
echo.

REM -- Step 1: Check IncrediBuild services ----------------------
echo [1/5] Checking IncrediBuild services...
sc query Incredibuild_Agent | findstr RUNNING >nul 2>&1
if errorlevel 1 (
    echo     Starting IncrediBuild services...
    for %%S in (Incredibuild_Agent "Incredibuild BuildCache" Incredibuild_CoordinatorService "Incredibuild Endpoint Service" Incredibuild_LicenseService Incredibuild_Manager) do (
        net start %%S >nul 2>&1
    )
    timeout /t 5 /nobreak >nul
) else (
    echo     IncrediBuild services already running.
)
echo     Done.
echo.

REM -- Step 2: Start EC2 instance -------------------------------
echo [2/5] Starting EC2 instance %EC2_ID%...
aws ec2 start-instances --instance-ids %EC2_ID% --region %REGION% --query "StartingInstances[0].CurrentState.Name" --output text 2>nul
if errorlevel 1 (
    echo     Instance may already be running. Checking state...
)
echo     Waiting for running state...
aws ec2 wait instance-running --instance-ids %EC2_ID% --region %REGION%
for /f "delims=" %%i in ('aws ec2 describe-instances --instance-ids %EC2_ID% --region %REGION% --query "Reservations[0].Instances[0].State.Name" --output text') do set EC2_STATE=%%i
echo     EC2 state: !EC2_STATE!
if "!EC2_STATE!"=="running" (
    for /f "delims=" %%i in ('aws ec2 describe-instances --instance-ids %EC2_ID% --region %REGION% --query "Reservations[0].Instances[0].PublicIpAddress" --output text') do set EC2_PUBLIC=%%i
    echo     Public IP: !EC2_PUBLIC!
) else (
    echo     ERROR: EC2 did not reach running state.
    pause
    exit /b 1
)
echo     Done.
echo.

REM -- Step 3: Wait for Tailscale -------------------------------
echo [3/5] Waiting for Tailscale connection to EC2 helper...
echo     (Tailscale service must be running on Windows)
set TS_OK=0
for /l %%N in (1,1,12) do (
    ping -n 1 -w 3000 %TS_HELPER_IP% >nul 2>&1
    if not errorlevel 1 (
        set TS_OK=1
        echo     Tailscale connected! Helper reachable at %TS_HELPER_IP%
        goto :ts_connected
    )
    echo     Waiting for Tailscale... attempt %%N/12
    timeout /t 10 /nobreak >nul
)
:ts_connected
if "!TS_OK!"=="0" (
    echo     WARNING: Could not ping EC2 helper via Tailscale after 2 minutes.
    echo     Tailscale may need manual login. Open Tailscale GUI and check.
    echo     Continuing anyway - IncrediBuild may still use local cores.
)
echo     Done.
echo.

REM -- Step 4: Verify SCL helper registered with coordinator ----
echo [4/5] Verifying SCL helper registered with coordinator...
set COORD_XML=%TEMP%\ib_coord_check.xml
"C:\Program Files (x86)\Incredibuild\xgCoordConsole.exe" /LOCAL /EXPORTSTATUS="%COORD_XML%" >nul 2>&1
if exist "%COORD_XML%" (
    findstr /i "ib-scl-helper" "%COORD_XML%" >nul 2>&1
    if not errorlevel 1 (
        echo     SCL helper ib-scl-helper is registered with coordinator.
    ) else (
        echo     WARNING: ib-scl-helper not found in coordinator status.
        echo     The helper may still be starting up on EC2.
    )
    del "%COORD_XML%" >nul 2>&1
) else (
    echo     WARNING: Could not export coordinator status.
)
echo     Done.
echo.

REM -- Step 5: Run the build ------------------------------------
echo [5/5] Launching IncrediBuild build...
echo.
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%build.ps1" -UseIncrediBuild -VerboseBuild
echo.
echo ============================================================
echo   Build complete. Check output above for results.
echo   Build Monitor: %SCRIPT_DIR%ib_threadx.bmon.ib_mon
echo   Build Log:     %SCRIPT_DIR%ib_threadx.log
echo ============================================================
echo.
echo To STOP the EC2 instance when done:
echo   stop_ec2.bat
echo.
pause
