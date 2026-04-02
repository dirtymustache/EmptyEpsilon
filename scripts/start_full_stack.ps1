param(
    [string]$Scenario = "scenario_00_basic.lua",
    [string]$NativeUsername = "native_user",
    [string]$WebUsername = "web_user",
    [string]$Station = "relay",
    [string]$NativeStation = "helms",
    [int]$ServerPort = 35666,
    [int]$BridgePort = 35667,
    [int]$WebPort = 18086,
    [int]$AdminPort = 8080,
    [switch]$NoBrowser,
    [switch]$NoNativeClient
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildDir = Join-Path $repoRoot "build-win-msvc"
$nativeExe = Join-Path $buildDir "EmptyEpsilon.exe"
$wasmHtml = Join-Path $repoRoot "build-wasm\EmptyEpsilon.html"
$stateFile = Join-Path $repoRoot "logs\fullstack-state.json"
$logDir = Join-Path $repoRoot ("logs\fullstack-" + (Get-Date -Format "yyyyMMdd-HHmmss"))

$candidateDllPaths = @("C:\Users\mrwil\.conda\envs\facedancer\Library\bin")
if ($env:CONDA_PREFIX) {
    $candidateDllPaths += (Join-Path $env:CONDA_PREFIX "Library\bin")
}
$candidateDllPaths = $candidateDllPaths | Where-Object { $_ -and (Test-Path $_) }

$dllPath = $candidateDllPaths | Select-Object -First 1

if (-not (Test-Path $nativeExe)) {
    throw "Missing native executable: $nativeExe"
}
if (-not (Test-Path $wasmHtml)) {
    throw "Missing wasm build output: $wasmHtml"
}
if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    throw "python was not found on PATH."
}
if (-not $dllPath) {
    throw "Could not find the SDL2/Freetype DLL directory. Expected a Conda env with Library\bin."
}

New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$stopScript = Join-Path $PSScriptRoot "stop_full_stack.ps1"
if (Test-Path $stopScript) {
    & $stopScript | Out-Null
}

function Start-NativeProcess {
    param(
        [string]$Arguments,
        [string]$StdOutLog,
        [string]$StdErrLog
    )

    $cmdLine = 'set PATH=' + $dllPath + ';' + $buildDir + ';%PATH% && "' + $nativeExe + '" ' + $Arguments
    return Start-Process -FilePath "cmd.exe" -ArgumentList "/c", $cmdLine -WorkingDirectory $repoRoot -RedirectStandardOutput $StdOutLog -RedirectStandardError $StdErrLog -PassThru
}

$bridge = Start-Process -FilePath "python" `
    -ArgumentList @(
        "scripts/wasm_ws_bridge.py",
        "--listen-host", "127.0.0.1",
        "--listen-port", "$BridgePort",
        "--target-host", "127.0.0.1",
        "--target-port", "$ServerPort",
        "--verbose"
    ) `
    -WorkingDirectory $repoRoot `
    -RedirectStandardOutput (Join-Path $logDir "bridge.out.log") `
    -RedirectStandardError (Join-Path $logDir "bridge.err.log") `
    -PassThru

$web = Start-Process -FilePath "python" `
    -ArgumentList @(
        "scripts/serve_wasm.py",
        "--host", "127.0.0.1",
        "--port", "$WebPort",
        "--directory", "build-wasm",
        "--proxy-admin-base", "http://127.0.0.1:$AdminPort"
    ) `
    -WorkingDirectory $repoRoot `
    -RedirectStandardOutput (Join-Path $logDir "serve.out.log") `
    -RedirectStandardError (Join-Path $logDir "serve.err.log") `
    -PassThru

$server = Start-NativeProcess `
    -Arguments ('headless=1 server_scenario=' + $Scenario + ' server_port=' + $ServerPort + ' httpserver=' + $AdminPort) `
    -StdOutLog (Join-Path $logDir "server.out.log") `
    -StdErrLog (Join-Path $logDir "server.err.log")

Start-Sleep -Seconds 3

$client = $null
if (-not $NoNativeClient) {
    $client = Start-NativeProcess `
        -Arguments ('autoconnect=' + $NativeStation + ' autoconnect_address=127.0.0.1:' + $ServerPort + ' autoconnectship=solo username=' + $NativeUsername) `
        -StdOutLog (Join-Path $logDir "client.out.log") `
        -StdErrLog (Join-Path $logDir "client.err.log")
}

$webUrl = "http://127.0.0.1:$WebPort/EmptyEpsilon.html?bridge=ws://127.0.0.1:$BridgePort&station=$Station&username=$WebUsername"
$adminUrl = "http://127.0.0.1:$WebPort/admin.html"

if (-not $NoBrowser) {
    Start-Process $webUrl | Out-Null
    Start-Process $adminUrl | Out-Null
}

Start-Sleep -Seconds 3

$state = [pscustomobject]@{
    repo_root = $repoRoot
    log_dir = $logDir
    bridge_pid = $bridge.Id
    web_pid = $web.Id
    server_launcher_pid = $server.Id
    client_launcher_pid = if ($client) { $client.Id } else { $null }
    web_url = $webUrl
    admin_url = $adminUrl
    server_port = $ServerPort
    bridge_port = $BridgePort
    web_port = $WebPort
    admin_port = $AdminPort
}
$state | ConvertTo-Json | Set-Content -Path $stateFile

$emptyEpsilon = Get-Process EmptyEpsilon -ErrorAction SilentlyContinue | Select-Object Id, MainWindowTitle

Write-Host "Full stack started."
Write-Host "Log dir: $logDir"
Write-Host "Web URL: $webUrl"
Write-Host "Admin URL: $adminUrl"
Write-Host "Bridge PID: $($bridge.Id)"
Write-Host "Web PID: $($web.Id)"
if ($emptyEpsilon) {
    Write-Host "EmptyEpsilon processes:"
    $emptyEpsilon | ForEach-Object {
        Write-Host ("  " + $_.Id + "  " + $_.MainWindowTitle)
    }
}
