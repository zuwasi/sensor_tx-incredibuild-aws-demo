<#
 .SYNOPSIS
   Command-line build for the sensor_tx ThreadX demo.
 .DESCRIPTION
   Builds the ThreadX RTOS sensor demo (sensor_threadx.exe via Makefile.threadx)
   and optionally the bare-metal sensor_tx demo (Makefile), reproducing the
   Eclipse CDT build outside the IDE.

   With -UseIncrediBuild, the make build is wrapped by IncrediBuild IBConsole
   so clang/clang++ compile tasks are distributed across the IncrediBuild grid
   (including an AWS SCL Linux Helper, see ib_aws/README.md) to reduce build time.
 .EXAMPLE
   .\build.ps1                  # clean + build ThreadX target
   .\build.ps1 -Target bare     # clean + build bare-metal sensor_tx
   .\build.ps1 -NoClean         # incremental build, no clean first
   .\build.ps1 -Run             # build then run the produced .exe
   .\build.ps1 -VerboseBuild    # stream full clang output to console
   .\build.ps1 -UseIncrediBuild              # distribute clang tasks via IncrediBuild grid
   .\build.ps1 -UseIncrediBuild -MaxCpus 64  # cap grid cores used at 64
   .\build.ps1 -UseIncrediBuild -UseCloudHelpers:$false  # LAN helpers only
#>
[CmdletBinding()]
param(
    [ValidateSet('threadx','bare')]
    [string]$Target = 'threadx',
    [switch]$NoClean,
    [switch]$Run,
    [switch]$VerboseBuild,
    [switch]$UseIncrediBuild,
    [int]$MaxCpus = 0,
    [bool]$UseCloudHelpers = $true
)

$ErrorActionPreference = 'Continue'

# The project root is wherever this script lives, so the demo works from any folder.
$ProjectRoot = $PSScriptRoot
Set-Location $ProjectRoot

# UCRT64 first => clang is the MSYS2 UCRT64 Clang toolchain (GCC-compatible
# MinGW ABI, drop-in replacement for gcc/g++). SCL Helpers accelerate Clang.
# MinGW make is used because it invokes cmd.exe as the recipe shell, which the
# Makefile.threadx clean recipes require (`del`, `if exist`, `rmdir /S /Q`).
$env:PATH = 'C:\msys64\ucrt64\bin;C:\MinGW\bin;' + $env:PATH
$makeExe = 'C:\MinGW\bin\make.exe'

if (-not (Test-Path $makeExe)) { throw "make not found at $makeExe (install MinGW make to C:\MinGW, see README)" }
if (-not (Get-Command clang -ErrorAction SilentlyContinue)) { throw 'clang not found on PATH (install mingw-w64-ucrt-x86_64-clang via MSYS2)' }

Write-Host "==> Project  : $ProjectRoot"
Write-Host "==> Target   : $Target"
Write-Host "==> Make     : $makeExe"
Write-Host "==> Compiler :"
& clang --version | Select-Object -First 1
Write-Host ''

switch ($Target) {
    'threadx' {
        $Makefile  = 'Makefile.threadx'
        $Artifact  = 'sensor_threadx.exe'
        $BuildArgs = 'all'
        $CleanArgs = 'clean'
    }
    'bare' {
        $Makefile  = 'Makefile'
        $Artifact  = 'sensor_tx.exe'
        $BuildArgs = 'BUILD_MODE=debug'
        # BUILD_MODE must be set during parse, otherwise the Makefile's
        # parse-time $(error) fires even for the clean target.
        $CleanArgs = 'BUILD_MODE=debug clean'
    }
}

# Run make through cmd /c so its stderr (clang warnings) is not converted into
# PowerShell terminating errors, and so cmd-style clean recipes work natively.
function Invoke-Make {
    param([string]$MakeArgs)
    cmd /c ("`"" + $makeExe + "`" -f `"" + $Makefile + "`" " + $MakeArgs + " 2>&1")
}

if (-not $NoClean) {
    Write-Host '==> Cleaning previous build...'
    Invoke-Make $CleanArgs | Out-Host
    Write-Host ''
}

Write-Host '==> Building...'
$logFile = Join-Path $ProjectRoot ('build_' + $Target + '.log')

if ($UseIncrediBuild) {
    # Locate IncrediBuild IBConsole. It wraps make and intercepts clang/clang++ to
    # distribute compile tasks across the Coordinator's Helper grid, including
    # the AWS SCL Linux Helper.
    $ibConsole = $null
    foreach ($cand in @(
        'C:\Program Files (x86)\IncrediBuild\IBConsole.exe',
        'C:\Program Files\IncrediBuild\IBConsole.exe'
    )) {
        if (Test-Path $cand) { $ibConsole = $cand; break }
    }
    if (-not $ibConsole) {
        throw 'IncrediBuild IBConsole.exe not found. Install IncrediBuild for Windows or run without -UseIncrediBuild.'
    }
    $ibVer = (Get-Item $ibConsole).VersionInfo.ProductVersion
    Write-Host "==> IBConsole     : $ibConsole (v$ibVer)"
    Write-Host "==> Cloud helpers : $UseCloudHelpers"
    if ($MaxCpus -gt 0) { Write-Host "==> Max CPUs      : $MaxCpus" }

    # IBConsole wraps the make command and intercepts spawned compilers.
    # /COMMAND          runs an arbitrary command line (our make invocation).
    # /USECLOUDHELPERS  enables cloud (AWS SCL) helpers in the grid.
    # /SHOWAGENT        prints which Agent executed each distributed task.
    # /LOG              writes the IncrediBuild build log.
    # /LOGLEVEL         sets logging verbosity (Detailed = level 5, for support).
    # /MON              saves the Build Monitor file (.bmon) for support export.
    # /PROFILE          interception profile defining clang as distributed.
    # /MAXCPUS          overrides the max cores used from the grid.
    $ibLog = Join-Path $ProjectRoot ('ib_' + $Target + '.log')
    $ibMon = Join-Path $ProjectRoot ('ib_' + $Target + '.bmon')
    $ibProfile = Join-Path $ProjectRoot 'profile.xml'
    $makeCmd = "`"$makeExe`" -f `"$Makefile`" $BuildArgs"
    $ibArgs = @("/COMMAND=`"$makeCmd`"", "/USECLOUDHELPERS=$UseCloudHelpers", '/SHOWAGENT', "/PROFILE=`"$ibProfile`"", '/LOGLEVEL=Detailed', "/LOG=`"$ibLog`"", "/MON=`"$ibMon`"")
    if ($MaxCpus -gt 0) { $ibArgs += "/MAXCPUS=$MaxCpus" }
    # Force enough parallel jobs to overflow local cores to remote helpers
    $ibArgs += '/AVOIDLOCAL=ON'
    Write-Host "==> Profile       : $ibProfile"
    Write-Host "==> Log level     : Detailed (level 5)"
    Write-Host "==> Build Monitor : $ibMon"
    Write-Host "==> Distributing build through IncrediBuild..."
    cmd /c ("`"$ibConsole`" " + ($ibArgs -join ' ') + " 2>&1") | Tee-Object -FilePath $logFile | Out-Host
    $buildExit = $LASTEXITCODE
} else {
    $output = Invoke-Make $BuildArgs
    $output | Set-Content -Path $logFile -Encoding UTF8
    if ($VerboseBuild) {
        $output | Out-Host
    } else {
        # Show only the interesting lines; all warnings are kept in the log file.
        $output | Where-Object { $_ -match 'error:|undefined reference|Build complete|Linking|recipe for target' } | Out-Host
    }
    $buildExit = $LASTEXITCODE
}

Write-Host ''
if ($buildExit -ne 0) {
    Write-Host "!! Build FAILED (exit $buildExit). Full log: $logFile" -ForegroundColor Red
    exit $buildExit
}

$exe = Join-Path $ProjectRoot $Artifact
if (Test-Path $exe) {
    $info = Get-Item $exe
    Write-Host ("==> OK: {0}  ({1:N0} bytes, {2})" -f $info.Name, $info.Length, $info.LastWriteTime) -ForegroundColor Green
} else {
    Write-Host "!! Build reported success but $Artifact was not found." -ForegroundColor Red
    exit 1
}

if ($Run) {
    Write-Host ''
    Write-Host "==> Running $Artifact"
    & $exe
    exit $LASTEXITCODE
}
