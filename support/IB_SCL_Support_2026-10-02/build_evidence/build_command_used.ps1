$ErrorActionPreference = 'Continue'
Set-Location "C:\Users\danie\sensor_tx-incredibuild-aws-demo"
$env:PATH = 'C:\msys64\ucrt64\bin;C:\MinGW\bin;' + $env:PATH

Write-Output '=== CLEAN ==='
cmd /c '"C:\MinGW\bin\make.exe" -f Makefile.threadx clean 2>&1' | Out-Null

Write-Output '=== STARTING IB BUILD (background) ==='
$ibArgs = '/COMMAND=""C:\MinGW\bin\make.exe" -f Makefile.threadx all" /USECLOUDHELPERS=True /SHOWAGENT /PROFILE="C:\Users\danie\sensor_tx-incredibuild-aws-demo\profile.xml" /LOG="C:\Users\danie\.amp\in\live_ib.log" /AVOIDLOCAL=ON'
$proc = Start-Process -FilePath "C:\Program Files (x86)\IncrediBuild\IBConsole.exe" -ArgumentList $ibArgs -RedirectStandardOutput "C:\Users\danie\.amp\in\live_stdout.txt" -RedirectStandardError "C:\Users\danie\.amp\in\live_stderr.txt" -PassThru -NoNewWindow

$seen = @{}
for ($i = 0; $i -lt 90; $i++) {
    Start-Sleep -Milliseconds 900
    if ($proc.HasExited -and $i -gt 3) { break }
    & "C:\Program Files (x86)\IncrediBuild\xgCoordConsole.exe" /LOCAL /EXPORTSTATUS="C:\Users\danie\.amp\in\poll.xml" | Out-Null
    try {
        [xml]$x = Get-Content C:\Users\danie\.amp\in\poll.xml -Raw
        foreach ($a in $x.SelectNodes('//Agent')) {
            $host_ = $a.Host
            $line = "t={0}s {1}: Building={2} RegCores={3} HelperRegType={4} WorkFor={5}" -f $i, $host_, $a.Building, $a.RegisteredCores, $a.HelperRegType, $a.WorkingForAgents
            if (-not $seen.ContainsKey($line)) { $seen[$line] = $true; Write-Output $line }
        }
    } catch { }
}
$proc.WaitForExit()
Write-Output ('=== EXIT: ' + $proc.ExitCode + ' ===')
Write-Output '=== distinct agent lines during build ==='
Write-Output '=== task agent distribution in IB log ==='
Select-String -Path C:\Users\danie\.amp\in\live_ib.log -Pattern "\(Agent '([^']+)'\) -success" | ForEach-Object { $_.Matches[0].Groups[1].Value -replace ' \(Core #\d+\)','' } | Group-Object | Sort-Object Count -Descending | Format-Table Name, Count -AutoSize | Out-String -Width 120
Get-Content C:\Users\danie\.amp\in\live_ib.log -Tail 8
