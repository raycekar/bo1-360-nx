# Two copies of Zombies on this PC over System Link: A (127.0.0.1) hosts, B (127.0.0.2) joins, A starts the match.
# Screens: $Out\link_*.png. Extra settings for both copies: -Extra '--x=y'.
param([string[]]$Extra = @())
$Win = Split-Path -Parent $PSScriptRoot                     # tools\win
$Dev = (Resolve-Path (Join-Path $Win '..\..\..')).Path   # holds bo1-360-nx, tools, out
$Game = Split-Path -Parent $Dev                          # game\, update\, user\, logs\
$Out = Join-Path $Dev 'out'
$c = (Join-Path $Win 'bo1ctl.py')
$net = @('--net_system_link_peers=127.0.0.1,127.0.0.2') + $Extra
& (Join-Path $Win 'run.ps1') zm -Pads 1 -Pipe bo1a -Extra (@('--net_lan_address=127.0.0.1') + $net)
& (Join-Path $Win 'run.ps1') zm -Pads 1 -Pipe bo1b -Keep -Extra (@('--net_lan_address=127.0.0.2', "--user_data_root=$Game/user2") + $net)
function Step($pipe, $line) { py -I $c --pipe $pipe $line.Split(' ') | Out-Null }
function Shot($name) {
  py -I $c --pipe bo1a raw shot "$Out\link_${name}_a.png" | Out-Null
  py -I $c --pipe bo1b raw shot "$Out\link_${name}_b.png" | Out-Null
}
$ja = Start-Job { py -I $using:c --pipe bo1a run (Join-Path $using:PSScriptRoot 'zm_systemlink.txt') }
$jb = Start-Job { py -I $using:c --pipe bo1b run (Join-Path $using:PSScriptRoot 'zm_systemlink.txt') }
$ja, $jb | Wait-Job -Timeout 150 | Out-Null
foreach ($p in 'bo1a', 'bo1b') { Step $p 'press 0 A'; Start-Sleep 2; Step $p 'press 0 A' }   # notice OK, System Link
Start-Sleep 5
Step bo1a 'press 0 DOWN'; Step bo1a 'press 0 A'                                            # A: Create Game
Start-Sleep 6
Step bo1b 'press 0 A'                                                                       # B: Join Game (list)
Start-Sleep 8
Shot list
Step bo1b 'press 0 A'                                                                       # B: join the host
Start-Sleep 10
Shot lobby
"start pressed at $(Get-Date -Format 'HH:mm:ss.fff')"
Step bo1a 'press 0 UP'; Step bo1a 'press 0 A'                                               # A: Start Match
Start-Sleep 60
Shot match
