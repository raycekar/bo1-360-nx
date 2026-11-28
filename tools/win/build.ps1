# Builds bo1zm.exe / bo1mp.exe natively on Windows (clang + lld-link with the xwin CRT/SDK splat).
# Layout (docs/building-windows.md): <dev>\bo1-360-nx (this repo), <dev>\tools\{llvm,cmake,ninja.exe,xwin-sdk},
# <dev>\out\build-win-<target>; the game folder (game\, update\, user\) is <dev>'s parent unless -Game says otherwise.
#   .\build.ps1 zm            Zombies   -> <dev>\out\build-win-zm
#   .\build.ps1 mp            Multiplayer
#   -Jobs N (default: logical CPUs - 2)   -March x86-64-v3 (AVX2, default) or x86-64-v2
#   -Deploy  copies the exe + DLLs to the game folder (<dev>'s parent or -Game; your settings toml is kept)
param(
  [ValidateSet('zm', 'mp')][string]$Target = 'zm',
  [int]$Jobs = [Math]::Max(1, [Environment]::ProcessorCount - 2),
  [string]$March = 'x86-64-v3',
  [switch]$Deploy,
  [switch]$Reconfigure,
  [string]$Game = ''
)
$ErrorActionPreference = 'Stop'
$Dev = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
if (-not $Game) { $Game = Split-Path -Parent $Dev }
$Root = Join-Path $Dev 'bo1-360-nx'
$T = Join-Path $Dev 'tools'
$env:PATH = "$T\cmake\bin;$T;$T\llvm\bin;$env:PATH"
$env:BO1_MARCH = $March
$env:BO1_DEBUG_LINES = '1'  # PDBs with line tables for the exe and the DLLs (profiling, crash triage)
$src = Join-Path $Root $Target
$out = Join-Path $Dev "out\build-win-$Target"
$gen = if ($Target -eq 'zm') { 'generated\default' } else { 'generated\default_mp' }
if (-not (Test-Path (Join-Path $src "$gen\sources.cmake"))) { throw "No $gen\sources.cmake: run codegen first" }
foreach ($d in Get-ChildItem (Join-Path $Root 'sdk\thirdparty') -Directory) {
  $g = Join-Path $d.FullName '.git'; if (-not (Test-Path $g)) { New-Item -ItemType Directory $g | Out-Null }
}
$fwd = { param($p) $p -replace '\\', '/' }
if ($Reconfigure -or -not (Test-Path (Join-Path $out 'build.ninja'))) {
  cmake -S $src -B $out -G Ninja `
    "-DCMAKE_TOOLCHAIN_FILE=$(& $fwd "$Root\cmake\windows-xwin.cmake")" `
    "-DBO1_LLVM=$(& $fwd "$T\llvm")" "-DBO1_XWIN=$(& $fwd "$T\xwin-sdk")" `
    "-DCMAKE_MAKE_PROGRAM=$(& $fwd "$T\ninja.exe")" `
    -DCMAKE_BUILD_TYPE=Release `
    -DREXGLUE_BUILD_HOST_TOOLS=OFF -DREXGLUE_BUILD_TESTS=OFF -DREXGLUE_ENABLE_TRACY=OFF `
    -DREXGLUE_USE_VULKAN=ON -DREXGLUE_RECOMP_DEBUG_INFO=line-tables-only `
    -DGIT_EXECUTABLE=C:/nonexistent/git "-DREXGLUE_OUTPUT_DIR=$(& $fwd "$out\sdk-out")" `
    "-DREXSDK_DIR=$(& $fwd "$Root\sdk")"
  if ($LASTEXITCODE) { throw "cmake configure failed" }
}
$sw = [Diagnostics.Stopwatch]::StartNew()
cmake --build $out --target "bo1$Target" -j $Jobs
if ($LASTEXITCODE) { throw "build failed" }
Write-Host ("Built bo1$Target in {0:N0} s" -f $sw.Elapsed.TotalSeconds)
if ($Deploy) {
  $running = Get-Process "bo1$Target" -ErrorAction SilentlyContinue
  if ($running) { $running | Stop-Process -Force; $running | Wait-Process -Timeout 20 -ErrorAction SilentlyContinue }
  $files = @(Get-Item (Join-Path $out "bo1$Target.exe"), (Join-Path $out "bo1$Target.pdb")) +
    @(Get-ChildItem -Path (Join-Path $out 'sdk-out\*') -Include *.dll, *.pdb -File)
  foreach ($f in $files) {
    for ($try = 1; ; $try++) {  # the exe stays locked for a moment after the game exits
      try { Copy-Item $f.FullName (Join-Path $Game $f.Name) -Force -ErrorAction Stop; break }
      catch { if ($try -ge 20) { throw }; Start-Sleep -Milliseconds 500 }
    }
  }
  Write-Host "Deployed $($files.Count) file(s) to ${Game}: $($files.Name -join ', ')"
}
