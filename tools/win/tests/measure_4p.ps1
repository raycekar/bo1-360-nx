# 4-player Kino frame-rate measurement: .\measure_4p.ps1 [-Extra '--x=y',...]  -> last 3 [frames] lines
param([string[]]$Extra = @())
$Win = Split-Path -Parent $PSScriptRoot                     # tools\win
$Dev = (Resolve-Path (Join-Path $Win '..\..\..')).Path   # holds bo1-360-nx, tools, out
$Game = Split-Path -Parent $Dev                          # game\, update\, user\, logs\
$Out = Join-Path $Dev 'out'
$c = (Join-Path $Win 'bo1ctl.py')
& (Join-Path $Win 'run.ps1') zm -Pads 4 -Extra $Extra | Out-Null
py -I $c run $PSScriptRoot\zm_4p_kino.txt | Out-Null
Start-Sleep 90
$log = (Get-ChildItem $Game\logs\bo1zm*.log | Sort-Object LastWriteTime | Select-Object -Last 1).FullName
Select-String $log -Pattern '\[frames\]' | Select-Object -Last 3 | ForEach-Object { ($_.Line -split '\[frames\] ')[1] }
