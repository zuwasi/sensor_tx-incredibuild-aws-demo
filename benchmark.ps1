# ============================================================
# sensor_tx Build Benchmark: Standard vs IncrediBuild
# Run from the project root (or anywhere; uses the script folder).
# ============================================================
$ProjectRoot = $PSScriptRoot
Set-Location $ProjectRoot
$env:PATH = "C:\msys64\ucrt64\bin;C:\MinGW\bin;" + $env:PATH
$makeExe = "C:\MinGW\bin\make.exe"
$Artifact = "sensor_threadx.exe"
$Makefile = "Makefile.threadx"

Write-Host ""
Write-Host "============================================================"
Write-Host "  sensor_tx Build Benchmark: Standard vs IncrediBuild"
Write-Host "  Project: $ProjectRoot"
Write-Host "  Target:  $Artifact"
Write-Host "============================================================"
Write-Host ""
Write-Host "Compiler:"
& clang --version | Select-Object -First 1
Write-Host ""

# ── Baseline: Standard build (no IncrediBuild) ──────────────
Write-Host "============================================================"
Write-Host "  [1/2] BASELINE: Standard build (no IncrediBuild)"
Write-Host "============================================================"
Write-Host ""
# Clean first so both runs are cold builds (no object reuse).
Write-Host "Cleaning..."
& $makeExe -f $Makefile clean 2>&1 | Out-Null
Write-Host "Building (standard)..."
$baselineStart = Get-Date
$output = & $makeExe -f $Makefile all 2>&1
$baselineEnd = Get-Date
$baselineSecs = [math]::Round(($baselineEnd - $baselineStart).TotalSeconds, 2)
$baselineOutput = $output -join "`n"
Write-Host $baselineOutput
$baselineExe = Get-Item (Join-Path $ProjectRoot $Artifact) -ErrorAction SilentlyContinue
Write-Host ""
Write-Host "  Baseline time: $baselineSecs seconds"
if ($baselineExe) { Write-Host "  Artifact: $($baselineExe.Name) ($($baselineExe.Length) bytes)" }
Write-Host ""

# ── IncrediBuild: Distributed build with AWS SCL helper ─────
Write-Host "============================================================"
Write-Host "  [2/2] INCREIDBUILD: Distributed build (AWS SCL helper)"
Write-Host "============================================================"
Write-Host ""
# Clean first so both runs are cold builds.
Write-Host "Cleaning..."
& $makeExe -f $Makefile clean 2>&1 | Out-Null
Write-Host "Building (IncrediBuild + AWS SCL helper)..."

$ibConsole = "C:\Program Files (x86)\IncrediBuild\IBConsole.exe"
$ibProfile = Join-Path $ProjectRoot "profile.xml"
$ibLog = Join-Path $ProjectRoot "ib_benchmark.log"
$ibMon = Join-Path $ProjectRoot "ib_benchmark.bmon"
$makeCmd = "`"$makeExe`" -f `"$Makefile`" all"
$ibArgs = @(
    "/COMMAND=`"$makeCmd`"",
    "/USECLOUDHELPERS=True",
    "/SHOWAGENT",
    "/PROFILE=`"$ibProfile`"",
    "/LOG=`"$ibLog`"",
    "/MON=`"$ibMon`""
)
Write-Host "  IBConsole flags: /USECLOUDHELPERS=True /SHOWAGENT /AVOIDLOCAL=ON"
Write-Host ""
$ibStart = Get-Date
cmd /c ("`"$ibConsole`" " + ($ibArgs -join ' ') + " 2>&1")
$ibEnd = Get-Date
$ibSecs = [math]::Round(($ibEnd - $ibStart).TotalSeconds, 2)
$ibExe = Get-Item (Join-Path $ProjectRoot $Artifact) -ErrorAction SilentlyContinue
Write-Host ""
Write-Host "  IncrediBuild time: $ibSecs seconds"
if ($ibExe) { Write-Host "  Artifact: $($ibExe.Name) ($($ibExe.Length) bytes)" }

# ── Summary ─────────────────────────────────────────────────
Write-Host ""
Write-Host "============================================================"
Write-Host "  BENCHMARK RESULTS"
Write-Host "============================================================"
Write-Host ""
Write-Host "  Standard build  : $baselineSecs seconds"
Write-Host "  IncrediBuild    : $ibSecs seconds"
if ($baselineSecs -gt 0 -and $ibSecs -gt 0) {
    $speedup = [math]::Round($baselineSecs / $ibSecs, 2)
    $saved = [math]::Round((1 - ($ibSecs / $baselineSecs)) * 100, 1)
    Write-Host ""
    Write-Host "  Speedup: ${speedup}x"
    Write-Host "  Time saved: ${saved}%"
}
Write-Host ""
Write-Host "  To confirm remote execution, check the build output for tags like"
Write-Host "  (Agent 'Ib-scl-helper (Core #N)'), or open the .bmon capture in the"
Write-Host "  IncrediBuild Build Monitor. Note: with /AVOIDLOCAL=ON the build is"
Write-Host "  forced onto the helper's cores; without it, IncrediBuild prefers"
Write-Host "  free local cores and the helper may stay idle by design."
Write-Host "============================================================"
Write-Host ""
Write-Host "Press Enter to close..."
Read-Host
