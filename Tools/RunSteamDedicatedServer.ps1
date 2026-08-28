[CmdletBinding()]
param(
    [string]$EngineRoot = "D:\Epic Games\UE_5.7",

    [string]$Map,

    [ValidateRange(1, 65535)]
    [int]$Port = 17777,

    [ValidateRange(1, [int]::MaxValue)]
    [int]$SteamAppId = 480,

    [string[]]$AdditionalArguments = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot "HeavenBreach.uproject"
$editorExecutable = Join-Path $EngineRoot "Engine\Binaries\Win64\UnrealEditor.exe"

if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf))
{
    throw "Project file was not found: $projectFile"
}

if (-not (Test-Path -LiteralPath $editorExecutable -PathType Leaf))
{
    throw "UnrealEditor.exe was not found. Pass the installed UE 5.7 directory with -EngineRoot. Expected: $editorExecutable"
}

$editorBaseDirectory = Split-Path -Parent $editorExecutable
$steamAppIdFile = Join-Path $editorBaseDirectory "steam_appid.txt"
$expectedAppId = $SteamAppId.ToString([Globalization.CultureInfo]::InvariantCulture)
$steamAppIdFileCreated = $false
$serverProcess = $null

$existingEndpoints = @(
    Get-NetUDPEndpoint `
        -LocalPort $Port `
        -ErrorAction SilentlyContinue
)

if ($existingEndpoints.Count -gt 0)
{
    Write-Host "Port $Port is already in use:" -ForegroundColor Red

    foreach ($endpoint in $existingEndpoints)
    {
        $ownerProcess = Get-Process `
            -Id $endpoint.OwningProcess `
            -ErrorAction SilentlyContinue

        $ownerName = if ($null -ne $ownerProcess)
        {
            $ownerProcess.ProcessName
        }
        else
        {
            "Unknown"
        }

        Write-Host (
            "  {0}:{1} | PID: {2} | Process: {3}" -f `
                $endpoint.LocalAddress,
                $endpoint.LocalPort,
                $endpoint.OwningProcess,
                $ownerName
        )
    }

    throw "Dedicated server was not started because UDP port $Port is already in use."
}

try
{
    if (Test-Path -LiteralPath $steamAppIdFile)
    {
        $existingAppId = (Get-Content -LiteralPath $steamAppIdFile -Raw).Trim()

        if ($existingAppId -ne $expectedAppId)
        {
            throw "The existing Steam AppID file contains '$existingAppId', expected '$expectedAppId'. It was not modified: $steamAppIdFile"
        }

        Write-Host "Using existing Steam AppID file: $steamAppIdFile"
    }
    else
    {
        [IO.File]::WriteAllText(
            $steamAppIdFile,
            $expectedAppId,
            [Text.Encoding]::ASCII
        )

        $steamAppIdFileCreated = $true
        Write-Host "Created Steam AppID file: $steamAppIdFile"
    }

    $writtenAppId = (Get-Content -LiteralPath $steamAppIdFile -Raw).Trim()
    if ($writtenAppId -ne $expectedAppId)
    {
        throw "Steam AppID verification failed for: $steamAppIdFile"
    }

    # Do not keep this file open. Editor-hosted server and client processes
    # share Engine/Binaries/Win64, and SteamShared must be able to recreate or
    # replace steam_appid.txt while another Unreal process is already running.

    $serverArguments = @(
        "`"$projectFile`""
    )

    if (-not [string]::IsNullOrWhiteSpace($Map))
    {
        $serverArguments += $Map
    }

    $serverArguments += @(
        "-server"
        "-log"
        "-port=$Port"
        "-NoSound"
        "-ddc=InstalledNoZenLocalFallback"
    ) + $AdditionalArguments

    Write-Host "Starting HeavenBreach dedicated server"
    Write-Host "  Executable : $editorExecutable"
    Write-Host "  Project    : $projectFile"
    $mapDescription = if ([string]::IsNullOrWhiteSpace($Map))
    {
        "ServerDefaultMap (Config/DefaultEngine.ini)"
    }
    else
    {
        $Map
    }

    Write-Host "  Map        : $mapDescription"
    Write-Host "  Game port  : $Port"
    Write-Host "  Steam AppID: $expectedAppId"
    Write-Host ""
    Write-Host "Expected startup markers:"
    Write-Host "  STEAM: [AppId: $expectedAppId] Game Server API initialized 1"
    Write-Host "  Online | Subsystem: STEAM"
    Write-Host ""

    $serverProcess = Start-Process `
        -FilePath $editorExecutable `
        -ArgumentList $serverArguments `
        -PassThru

    Write-Host "Server process started | PID: $($serverProcess.Id)"
    Write-Host "Waiting for UDP port $Port..."

    $serverEndpoint = $null

    while ($null -eq $serverEndpoint)
    {
        $serverProcess.Refresh()

        if ($serverProcess.HasExited)
        {
            throw (
                "Dedicated server exited before opening UDP port {0}. Exit code: {1}" -f `
                    $Port,
                    $serverProcess.ExitCode
            )
        }

        $serverEndpoint = Get-NetUDPEndpoint `
            -LocalPort $Port `
            -ErrorAction SilentlyContinue |
            Where-Object { $_.OwningProcess -eq $serverProcess.Id } |
            Select-Object -First 1

        if ($null -eq $serverEndpoint)
        {
            Start-Sleep -Milliseconds 500
        }
    }

    Write-Host (
        "Server ready | Address: {0}:{1} | PID: {2}" -f `
            $serverEndpoint.LocalAddress,
            $serverEndpoint.LocalPort,
            $serverEndpoint.OwningProcess
    ) -ForegroundColor Green
    Write-Host "This PowerShell remains attached until the server exits."

    Wait-Process -Id $serverProcess.Id
    $serverProcess.Refresh()

    if ($serverProcess.ExitCode -ne 0)
    {
        throw "Dedicated server exited with code $($serverProcess.ExitCode)"
    }
}
finally
{
    $serverIsStillRunning = $false

    if ($null -ne $serverProcess)
    {
        $serverProcess.Refresh()
        $serverIsStillRunning = -not $serverProcess.HasExited
    }

    if (
        $steamAppIdFileCreated -and
        -not $serverIsStillRunning -and
        (Test-Path -LiteralPath $steamAppIdFile -PathType Leaf)
    )
    {
        $resolvedBaseDirectory = [IO.Path]::GetFullPath($editorBaseDirectory).TrimEnd('\', '/')
        $resolvedAppIdFile = [IO.Path]::GetFullPath($steamAppIdFile)
        $resolvedAppIdParent = [IO.Path]::GetDirectoryName($resolvedAppIdFile).TrimEnd('\', '/')

        if (
            -not $resolvedAppIdParent.Equals($resolvedBaseDirectory, [StringComparison]::OrdinalIgnoreCase) -or
            [IO.Path]::GetFileName($resolvedAppIdFile) -ne "steam_appid.txt"
        )
        {
            throw "Refusing to clean up an unexpected path: $resolvedAppIdFile"
        }

        Remove-Item -LiteralPath $resolvedAppIdFile -Force
        Write-Host "Removed temporary Steam AppID file: $resolvedAppIdFile"
    }
    elseif ($steamAppIdFileCreated -and $serverIsStillRunning)
    {
        Write-Warning ((
                "The script stopped while server PID {0} is still running. " +
                "The Steam AppID file was left in place: {1}"
            ) -f $serverProcess.Id, $steamAppIdFile)
    }
}
