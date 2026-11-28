# Solo + god mode on each Zombies map (mission select index), 5 captures each: $Out\tour\<map>_N.png
param([int[]]$Maps = @(0, 1, 3, 4, 5, 6, 7, 8, 9, 10), [int]$WaitMs = 60000)
$Win = Split-Path -Parent $PSScriptRoot                     # tools\win
$Dev = (Resolve-Path (Join-Path $Win '..\..\..')).Path   # holds bo1-360-nx, tools, out
$Game = Split-Path -Parent $Dev                          # game\, update\, user\, logs\
$Out = Join-Path $Dev 'out'
$names = 'kino', 'five', 'deadops', 'nacht', 'verruckt', 'shinonuma', 'derriese', 'ascension', 'cotd', 'shangrila', 'moon'
$c = (Join-Path $Win 'bo1ctl.py')
New-Item -ItemType Directory -Force $Out\tour | Out-Null
foreach ($m in $Maps) {
  $n = $names[$m]
  & (Join-Path $Win 'run.ps1') zm -Pads 1 -Extra '--bo1zm_god=true' | Out-Null
  $steps = @('wait 45000', 'press 0 START', 'wait 3000', 'press 0 DOWN', 'press 0 DOWN', 'press 0 A', 'wait 3000',
             'press 0 DOWN', 'press 0 DOWN', 'press 0 DOWN', 'press 0 A', 'wait 3000')
  $steps += @('press 0 DOWN') * $m
  $steps += @('press 0 A', "wait $WaitMs")
  $steps | Set-Content "$env:TEMP\tour.txt"
  py -I $c run "$env:TEMP\tour.txt" | Out-Null
  py -I $c raw shot "$Out\tour\${n}_0.png" | Out-Null
  foreach ($i in 1..3) {
    py -I $c stick 0 R 1 0 420 | Out-Null
    Start-Sleep 1
    py -I $c raw shot "$Out\tour\${n}_$i.png" | Out-Null
  }
  py -I $c stick 0 R 0 1 500 | Out-Null
  Start-Sleep 1
  py -I $c raw shot "$Out\tour\${n}_4.png" | Out-Null
  $log = (Get-ChildItem $Game\logs\bo1zm*.log | Sort-Object LastWriteTime | Select-Object -Last 1).FullName
  $f = Select-String $log -Pattern '\[frames\]' | Select-Object -Last 1 | ForEach-Object { ($_.Line -split '\[frames\] ')[1] }
  $e = (Select-String $log -Pattern 'Com_Error|Sys_Error|OutOfMemory').Count
  "$n : $f ; errors $e"
}
Get-Process bo1zm -ErrorAction SilentlyContinue | Stop-Process -Force
