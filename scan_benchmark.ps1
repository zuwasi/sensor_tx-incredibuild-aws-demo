# ============================================================
# Parasoft C/C++test Scan Benchmark: Standard vs IncrediBuild
# Runs the same cold scan twice:
#   [1/2] cpptestcli -trace make (standard, local cores only)
#   [2/2] cpptestcli -trace make under IBConsole, distributed
#         to local + AWS SCL helper cores
# The scan is a full cold pass: traced rebuild + Flow Analysis.
# Run from the project root (or anywhere; uses the script folder).
# ============================================================
param(
    # Your local Parasoft settings file (DTP license connection).
    # Copy cpptest-scan-localsettings.example.properties and fill it in.
    [string]$LocalSettings = "cpptest-scan-localsettings.properties",

    # Built-in test configuration name from your C++test installation.
    [string]$TestConfig = "Flow Analysis Standard",

    # C++test installation directory.
    [string]$CpptestHome = "C:\parasoft\CPP_STD\cpptest"
)

$ProjectRoot = $PSScriptRoot
Set-Location $ProjectRoot
$env:PATH = "C:\msys64\ucrt64\bin;C:\MinGW\bin;" + $env:PATH

$cpptestCli = Join-Path $CpptestHome "cpptestcli.exe"
$makeExe    = "C:\MinGW\bin\make.exe"
$ibConsole  = "C:\Program Files (x86)\IncrediBuild\IBConsole.exe"
$makefile   = "Makefile.threadx"

if (-not (Test-Path $LocalSettings)) {
    Write-Error "Local settings file '$LocalSettings' not found. Copy cpptest-scan-localsettings.example.properties to '$LocalSettings' and fill in your DTP license settings."
}
if (-not (Test-Path $cpptestCli)) {
    Write-Error "cpptestcli not found at '$cpptestCli'. Pass -CpptestHome <your C++test install dir>."
}

# IBConsole needs a single, properly quoted command; use a space-free copy of
# the built-in test configuration to keep /COMMAND parsing simple.
$configCopy = Join-Path $env:TEMP "cpptest_scan_config.properties"
Copy-Item (Join-Path $CpptestHome "configs\builtin\$TestConfig.properties") $configCopy -Force

$scanPreArgs  = @(
    "-localsettings", "`"$LocalSettings`"",
    "-config", "`"$configCopy`"",
    "-compiler", "gcc_11-64"
)
$traceArgs = @("-trace", "`"$makeExe`"", "-f", $makefile, "all")
# NOTE: -report must be placed BEFORE the -trace arguments: everything after
# -trace is forwarded to make as the build command.

function Invoke-ColdScan {
    param([string]$ReportDir)
    New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null
    & $makeExe -f $makefile clean 2>&1 | Out-Null
    $allArgs = $scanPreArgs + @("-report", "`"$ReportDir`"") + $traceArgs
    $start = Get-Date
    & $cpptestCli @allArgs 2>&1 | Out-Null
    $end = Get-Date
    [math]::Round(($end - $start).TotalSeconds, 2)
}

# Read the violation count from a generated report. The XML has many
# per-category total attributes; the largest one is the overall count.
function Get-ViolationCount {
    param([string]$ReportDir)
    $xml = Join-Path $ReportDir "report.xml"
    if (-not (Test-Path $xml)) { return "?" }
    $totals = Select-String -Path $xml -Pattern 'total="(\d+)"' -AllMatches |
        ForEach-Object { $_.Matches } |
        ForEach-Object { [int]$_.Groups[1].Value }
    if ($totals) { return ($totals | Measure-Object -Maximum).Maximum }
    return "?"
}

Write-Host ""
Write-Host "============================================================"
Write-Host "  Parasoft scan benchmark: Standard vs IncrediBuild"
Write-Host "  Project: $ProjectRoot"
Write-Host "  Config:  $TestConfig"
Write-Host "============================================================"
Write-Host ""

Write-Host "[1/2] Baseline: standard cold scan (no IncrediBuild)..."
$baselineSecs = Invoke-ColdScan -ReportDir (Join-Path $ProjectRoot "reports\scan_baseline")
$baselineViol = Get-ViolationCount (Join-Path $ProjectRoot "reports\scan_baseline")
Write-Host "  Baseline scan time: $baselineSecs seconds, violations: $baselineViol"
Write-Host ""

Write-Host "[2/2] IncrediBuild cold scan (local cores + AWS SCL helper)..."
$ibReport  = Join-Path $ProjectRoot "reports\scan_ib"
$ibLog     = Join-Path $ProjectRoot "scan_ib_benchmark.log"
$ibMon     = Join-Path $ProjectRoot "scan_ib_benchmark.bmon"
# Clean first so both runs are cold (no object reuse).
& $makeExe -f $makefile clean 2>&1 | Out-Null
$cmdString = "`"$cpptestCli`" " + ($scanPreArgs -join " ") + " -report `"$ibReport`" " + ($traceArgs -join " ")
$ibArgs = @(
    "/COMMAND=`"$cmdString`"",
    "/USECLOUDHELPERS=True",
    "/PROFILE=`"$(Join-Path $ProjectRoot 'profile.xml')`"",
    "/LOG=`"$ibLog`"",
    "/MON=`"$ibMon`""
)
$start = Get-Date
cmd /c ("`"$ibConsole`" " + ($ibArgs -join ' ') + " 2>&1") | Out-Null
$end = Get-Date
$ibSecs = [math]::Round(($end - $start).TotalSeconds, 2)
$ibViol = Get-ViolationCount $ibReport
Write-Host "  IncrediBuild scan time: $ibSecs seconds, violations: $ibViol"
Write-Host ""

$speedup = if ($ibSecs -gt 0) { [math]::Round($baselineSecs / $ibSecs, 2) } else { "?" }
Write-Host "============================================================"
Write-Host "  BENCHMARK RESULTS"
Write-Host "============================================================"
Write-Host ""
Write-Host "  Standard cold scan:           $baselineSecs seconds ($baselineViol violations)"
Write-Host "  IncrediBuild cold scan:       $ibSecs seconds ($ibViol violations)"
Write-Host "  Speedup:                      ${speedup}x"
if ($baselineViol -ne $ibViol) {
    Write-Host ""
    Write-Host "  WARNING: violation counts differ between runs!"
}
Write-Host ""
Write-Host "  Logs: $ibLog"
Write-Host "  Build Monitor capture: $ibMon (open in IncrediBuild Build Monitor"
Write-Host "  to see which tasks ran on the AWS helper)"
Write-Host ""
