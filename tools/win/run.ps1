# Starts the game for an unattended test. Stops any running copy first unless -Keep.
#   .\run.ps1 zm -Pads 4 [-Pipe bo1ctl] [-Extra '--bo1zm_local_players=4'] [-Keep]
# Prints the process id and the log file. Virtual pads: tools\bo1ctl.py.
param(
  [ValidateSet('zm', 'mp')][string]$Target = 'zm',
  [int]$Pads = 1,
  [string]$Pipe = 'bo1ctl',
  [string[]]$Extra = @(),
  [switch]$Keep,
  [string]$Game = ''
)
if (-not $Game) { $Game = Split-Path -Parent (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path }
if (-not $Keep) { Get-Process "bo1$Target" -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 1 }
$before = @(Get-ChildItem "$Game\logs" -Filter "bo1$Target*.log" -ErrorAction SilentlyContinue).FullName
$args = @("--input_script_pads=$Pads", "--input_script_pipe=$Pipe") + $Extra
$p = Start-Process "$Game\bo1$Target.exe" -WorkingDirectory $Game -ArgumentList $args -PassThru
Start-Sleep 3
$log = Get-ChildItem "$Game\logs" -Filter "bo1$Target*.log" | Where-Object { $_.FullName -notin $before } |
  Sort-Object LastWriteTime | Select-Object -Last 1
"pid=$($p.Id) log=$($log.FullName)"
