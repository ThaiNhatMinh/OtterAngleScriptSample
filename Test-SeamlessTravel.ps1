<#
.SYNOPSIS
    Quick-test script for OtterServerMesh proxy-based seamless travel.
.DESCRIPTION
    Builds the project (optional), launches 2 game servers + 1 proxy server + 1 client,
    and monitors for migration events. All processes are tracked and auto-killed on exit.
.EXAMPLE
    .\Test-SeamlessTravel.ps1 -Build
    .\Test-SeamlessTravel.ps1 -SkipBuild -ClientOnly
    .\Test-SeamlessTravel.ps1 -Cleanup
#>

param(
    [switch]$Build,
    [switch]$SkipBuild,
    [switch]$ClientOnly,
    [switch]$Cleanup,
    [string]$EngineDir = "C:\Projects\UnrealEngine",
    [string]$ProjectDir = "C:\Projects\Sample\AngelScriptSample",
    [string]$Config = "DebugGame",
    [string]$MapName = "Lvl_ThirdPerson"
)

# ─── Config ───
$ProjectFile = Join-Path $ProjectDir "AngelScriptSample.uproject"
$GameMode = "/Script/AngelScriptSample.ProxyTestGameMode"
$EngineExe = Join-Path $EngineDir "Engine\Binaries\Win64"
$EditorExe = Join-Path $EngineExe "UnrealEditor-Win64-$Config.exe"
$CmdExe   = Join-Path $EngineExe "UnrealEditor-Win64-$Config-Cmd.exe"
$BatchDir = Join-Path $EngineDir "Engine\Build\BatchFiles"
$BuildBat = Join-Path $BatchDir "Build.bat"
$UatBat   = Join-Path $BatchDir "RunUAT.bat"

$GS0_Port = 15000
$GS1_Port = 15001
$ProxyPort = 17000

# ─── Track processes for cleanup ───
$script:ServerProcesses = @()

function Cleanup {
    Write-Host "`n=== Cleaning up server processes ===" -ForegroundColor Yellow
    foreach ($proc in $script:ServerProcesses) {
        if ($proc.HasExited -eq $false) {
            Write-Host "  Stopping PID $($proc.Id) ($($proc.Name))..." -ForegroundColor DarkYellow
            $proc.Kill()
            $proc.WaitForExit(5000)
        }
    }
    # Also find any orphaned instances
    $orphans = Get-Process -Name "UnrealEditor*" -ErrorAction SilentlyContinue |
        Where-Object { $_.CommandLine -match "OtterNodeId|OtterProxyMode|OtterMeshPort" -and $_.HasExited -eq $false }
    foreach ($p in $orphans) {
        Write-Host "  Killing orphan PID $($p.Id)..." -ForegroundColor DarkYellow
        $p.Kill()
    }
    Write-Host "=== Cleanup complete ===" -ForegroundColor Green
}

function Launch-Server {
    param([string]$Title, [string]$Args)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $CmdExe
    $psi.Arguments = $Args
    $psi.UseShellExecute = $true
    $psi.CreateNoWindow = $false
    $psi.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Normal
    $psi.LoadUserProfile = $false
    $proc = [System.Diagnostics.Process]::Start($psi)
    $script:ServerProcesses += $proc
    Write-Host "  Launched [$Title] PID=$($proc.Id)" -ForegroundColor Cyan
    return $proc
}

# ─── Cleanup-only mode ───
if ($Cleanup) {
    Cleanup
    exit 0
}

# ─── Step 0: Build ───
if ($Build) {
    Write-Host "=== Building project (Server target) ===" -ForegroundColor Magenta
    & $BuildBat "AngelScriptSampleServer" "Win64" "$Config" "$ProjectFile" -Wait
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Server build FAILED (exit code: $LASTEXITCODE). Check the error above." -ForegroundColor Red
        exit 1
    }
    Write-Host "Build complete!" -ForegroundColor Green
}
elseif (-not $SkipBuild) {
    Write-Host "NOTE: Use -Build to build first, or -SkipBuild to skip." -ForegroundColor DarkYellow
    Write-Host "      If the project isn't built, the test will fail." -ForegroundColor DarkYellow
}

# ─── Trap Ctrl+C for cleanup ───
$null = Register-EngineEvent -SourceIdentifier PowerShell.Exiting -Action { Cleanup }

try {
    # ─── Step 1: Launch Game Server 0 ───
    Write-Host "`n=== Launching GS_0 (cell 0,0) port $GS0_Port ===" -ForegroundColor Green
    $gs0Args = @(
        """$ProjectFile""", $MapName, "-game -server -log",
        "-GameMode=$GameMode",
        "-OtterNodeId=GS_0",
        "-OtterMeshPort=$GS0_Port",
        "-OtterMeshNumServers=2",
        "-OtterMeshPeers=""127.0.0.1:$GS1_Port"""
    )
    $gs0 = Launch-Server -Title "GS_0" -Args ($gs0Args -join " ")
    Start-Sleep -Seconds 5

    # ─── Step 2: Launch Game Server 1 ───
    Write-Host "`n=== Launching GS_1 (cell 1,0) port $GS1_Port ===" -ForegroundColor Green
    $gs1Args = @(
        """$ProjectFile""", $MapName, "-game -server -log",
        "-GameMode=$GameMode",
        "-OtterNodeId=GS_1",
        "-OtterMeshPort=$GS1_Port",
        "-OtterMeshNumServers=2",
        "-OtterMeshPeers=""127.0.0.1:$GS0_Port"""
    )
    $gs1 = Launch-Server -Title "GS_1" -Args ($gs1Args -join " ")
    Start-Sleep -Seconds 5

    # ─── Step 3: Launch Proxy Server ───
    Write-Host "`n=== Launching Proxy Server port $ProxyPort ===" -ForegroundColor Green
    $proxyArgs = @(
        """$ProjectFile""", $MapName, "-game -server -log",
        "-GameMode=$GameMode",
        "-OtterProxyMode",
        "-OtterProxyPort=$ProxyPort",
        "-GameServers=""127.0.0.1:$GS0_Port,127.0.0.1:$GS1_Port""",
        "-OtterCellSize=50000",
        "-OtterGridX=8",
        "-OtterGridY=8"
    )
    $proxy = Launch-Server -Title "Proxy" -Args ($proxyArgs -join " ")
    Start-Sleep -Seconds 5

    # ─── Step 4: Launch Client (if not ClientOnly, skip this and let user connect) ───
    if (-not $ClientOnly) {
        Write-Host "`n=== Launching Client (connect to proxy :$ProxyPort) ===" -ForegroundColor Green
        $clientArgs = @(
            """$ProjectFile""", "127.0.0.1:$ProxyPort", "-game -log"
        )
        $client = Launch-Server -Title "Client" -Args ($clientArgs -join " ")
        Write-Host "`nClient connecting to proxy at 127.0.0.1:$ProxyPort" -ForegroundColor Cyan
    }
    else {
        Write-Host "`nClient NOT launched. Connect manually to 127.0.0.1:$ProxyPort" -ForegroundColor Yellow
    }

    # ─── Verification Steps ───
    Write-Host "`n============================================" -ForegroundColor Magenta
    Write-Host "      Seamless Travel Test Running" -ForegroundColor Magenta
    Write-Host "============================================" -ForegroundColor Magenta
    Write-Host ""
    Write-Host "Expected log output (check each window):" -ForegroundColor White
    Write-Host "  GS_0:  'Listening on port $GS0_Port as GS_0'" -ForegroundColor Gray
    Write-Host "  GS_0:  'Proxy migration host registered.'" -ForegroundColor Gray
    Write-Host "  GS_1:  'Connected to peer GS_0'" -ForegroundColor Gray
    Write-Host "  GS_1:  'Proxy migration host registered.'" -ForegroundColor Gray
    Write-Host "  Proxy: 'Starting proxy on port $ProxyPort'" -ForegroundColor Gray
    Write-Host "  Proxy: 'Connecting migration beacon to GS_0'" -ForegroundColor Gray
    Write-Host "  Proxy: 'Connecting migration beacon to GS_1'" -ForegroundColor Gray
    if (-not $ClientOnly) {
        Write-Host "  Proxy: 'Registered client Player_0 (route 0) -> GS 0'" -ForegroundColor Gray
        Write-Host "  Client: '--- Proxy Seamless Travel Demo ---'" -ForegroundColor Gray
    }
    Write-Host ""
    Write-Host "=== Migration Test ===" -ForegroundColor White
    Write-Host "Walk the character past X=50000 in the client window." -ForegroundColor Yellow
    Write-Host "The Proxy window should show:" -ForegroundColor Yellow
    Write-Host "  'Player crossed cell boundary (0,0) -> (1,0)'" -ForegroundColor Gray
    Write-Host "  'Migrating route 0 from GS_0 -> GS_1'" -ForegroundColor Gray
    Write-Host ""
    Write-Host "Debug command (on proxy window): DebugProxyRoutes" -ForegroundColor Cyan
    Write-Host ""

    # ─── Wait for user to finish ───
    Write-Host "Press ENTER to shutdown all servers..." -ForegroundColor Red -NoNewline
    $null = Read-Host
}
finally {
    Cleanup
}

Write-Host "Test complete." -ForegroundColor Green
