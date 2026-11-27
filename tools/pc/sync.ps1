# Copies a bo1-360-nx build from the network folder to this PC, only what changed.
# Run it through sync.cmd. -Source: the network folder (it holds manifest.json). -Dest: where the game goes.
param(
  [Parameter(Mandatory = $true)][string]$Source,
  [Parameter(Mandatory = $true)][string]$Dest
)
$ErrorActionPreference = 'Stop'
$Dest = $Dest.TrimEnd('\')
$Source = $Source.TrimEnd('\')

$srcManifest = Join-Path $Source 'manifest.json'
if (-not (Test-Path -LiteralPath $srcManifest)) { throw "No manifest.json in $Source. Check SOURCE in sync.cmd." }
$new = Get-Content -LiteralPath $srcManifest -Raw | ConvertFrom-Json
$stateDir = Join-Path $Dest '.sync'
$stateFile = Join-Path $stateDir 'manifest.json'
$old = @{}
if (Test-Path -LiteralPath $stateFile) {
  foreach ($e in (Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json).files) { $old[$e.path] = $e }
}

Write-Host "Build of $($new.built): $($new.files.Count) files"
$copied = 0; $bytes = 0; $wanted = @{}
$i = 0
foreach ($e in $new.files) {
  $i++
  $wanted[$e.path] = $true
  $rel = $e.path -replace '/', '\'
  $dst = Join-Path $Dest $rel
  $prev = $old[$e.path]
  if ($e.keep -and (Test-Path -LiteralPath $dst)) { $old[$e.path] = $e; continue }  # your settings stay
  if ($prev -and $prev.sha256 -eq $e.sha256 -and (Test-Path -LiteralPath $dst) -and
      ((Get-Item -LiteralPath $dst).Length -eq $e.size)) { continue }
  $src = Join-Path $Source $rel
  $dir = Split-Path -Parent $dst
  if (-not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
  Write-Host ("[{0}/{1}] {2} ({3:N1} MB)" -f $i, $new.files.Count, $e.path, ($e.size / 1MB))
  $part = "$dst.part"
  Copy-Item -LiteralPath $src -Destination $part -Force
  $hash = (Get-FileHash -LiteralPath $part -Algorithm SHA256).Hash.ToLower()
  if ($hash -ne $e.sha256) { Remove-Item -LiteralPath $part -Force; throw "Hash mismatch for $($e.path): copy it again." }
  Move-Item -LiteralPath $part -Destination $dst -Force
  $copied++; $bytes += $e.size
  # Record progress as we go, so a stopped sync resumes without copying again.
  $old[$e.path] = $e
}

# Files the new build no longer has (only ones an earlier sync put here).
$removed = 0
foreach ($p in @($old.Keys)) {
  if (-not $wanted.ContainsKey($p)) {
    $f = Join-Path $Dest ($p -replace '/', '\')
    if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force; $removed++ }
  }
}

if (-not (Test-Path -LiteralPath $stateDir)) { New-Item -ItemType Directory -Path $stateDir -Force | Out-Null }
Copy-Item -LiteralPath $srcManifest -Destination $stateFile -Force
Write-Host ("Done: {0} copied ({1:N2} GB), {2} removed, {3} up to date." -f $copied, ($bytes / 1GB), $removed, ($new.files.Count - $copied))
Write-Host "Start Zombies with bo1zm.exe (Multiplayer: bo1mp.exe) in $Dest. Queued test builds: builds\<name>\ (see BUILDS.txt)."
