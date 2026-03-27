param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$stateFile = Join-Path $repoRoot "logs\fullstack-state.json"
$ports = 35666, 35667, 18086, 8080
$pidSet = [System.Collections.Generic.HashSet[int]]::new()

function Add-Pid {
    param([Nullable[int]]$ProcessId)

    if ($ProcessId -and $ProcessId.Value -gt 0) {
        $null = $pidSet.Add($ProcessId.Value)
    }
}

if (Test-Path $stateFile) {
    try {
        $state = Get-Content $stateFile | ConvertFrom-Json
        Add-Pid $state.bridge_pid
        Add-Pid $state.web_pid
        Add-Pid $state.server_launcher_pid
        Add-Pid $state.client_launcher_pid
    } catch {
    }
}

foreach ($port in $ports) {
    $matches = netstat -ano | Select-String (":" + $port)
    foreach ($match in $matches) {
        $parts = ($match.ToString() -split "\s+") | Where-Object { $_ -ne "" }
        if ($parts.Length -ge 5) {
            $procId = [int]$parts[-1]
            if ($procId -gt 0) {
                $null = $pidSet.Add($procId)
            }
        }
    }
}

Get-Process EmptyEpsilon -ErrorAction SilentlyContinue | ForEach-Object {
    $null = $pidSet.Add($_.Id)
}

foreach ($procId in $pidSet) {
    try {
        Stop-Process -Id $procId -Force -ErrorAction Stop
    } catch {
    }
}

if (Test-Path $stateFile) {
    Remove-Item -LiteralPath $stateFile -Force
}

Write-Host ("Stopped processes: " + (($pidSet | Sort-Object) -join ", "))
