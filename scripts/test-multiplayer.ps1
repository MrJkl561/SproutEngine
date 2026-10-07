# The Windows half of test-multiplayer.bat (see there): runs the server and
# two clients and checks what they print.
param([Parameter(Mandatory = $true)][string]$Editor, [int]$Port = 7790)

$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root "editor\tests\multiplayer"
$logs = Join-Path ([IO.Path]::GetTempPath()) ("sprout-multiplayer-" + [Guid]::NewGuid())
New-Item -ItemType Directory -Path $logs | Out-Null

function Start-Game([string]$name, [string]$arguments) {
    Start-Process -FilePath $Editor -ArgumentList "--project `"$project`" $arguments" -NoNewWindow -PassThru `
        -RedirectStandardOutput (Join-Path $logs "$name.txt") -RedirectStandardError (Join-Path $logs "$name-err.txt")
}

# Build the test's scripts first, so the three programs don't all try to at once
& $Editor --project $project --check-scripts | Out-Null
if ($LASTEXITCODE -ne 0) { & $Editor --project $project --check-scripts; exit 1 }

$server = Start-Game "server" "--play --server --frames 900 -- --port $Port"
Start-Sleep -Seconds 1
$client1 = Start-Game "client1" "--play --headless --frames 780 -- --port $Port --expect-leave"
Start-Sleep -Seconds 1
$client2 = Start-Game "client2" "--play --headless --frames 360 -- --port $Port"
$server, $client1, $client2 | Wait-Process -Timeout 120

$ok = $true
foreach ($who in "server", "client1", "client2") {
    Write-Output "--- $who"
    $text = Get-Content (Join-Path $logs "$who.txt") -ErrorAction SilentlyContinue
    $text | Select-String -Pattern "CHECK|PASSED|FAILED|Couldn't|rror" | ForEach-Object { $_.Line }
    $want = if ($who -eq "server") { "SERVER PASSED" } else { "CLIENT PASSED" }
    if (-not ($text -match $want)) { $ok = $false }
}
if ($ok) { Write-Output "Multiplayer test passed."; exit 0 }
Write-Output "Multiplayer test FAILED (full output in $logs)."
exit 1
