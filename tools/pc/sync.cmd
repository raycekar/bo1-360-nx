@echo off
rem ---------------------------------------------------------------------------
rem  bo1-360-nx: copy the latest build from the network folder to this folder.
rem  Put this file in the folder where you want the game (any folder) and run it.
rem  The first run copies everything (about 11 GB); later runs copy only changes.
rem
rem  Set SOURCE to the build folder on the network drive (the folder that holds
rem  manifest.json). Example, if the server's workspace is drive F: at
rem  F:\Projects\claude_workspace :
rem      set "SOURCE=F:\Projects\claude_workspace\bo1recomp\out\win"
rem ---------------------------------------------------------------------------
set "SOURCE=F:\Projects\claude_workspace\bo1recomp\out\win"

if not exist "%SOURCE%\manifest.json" (
  echo Cannot find "%SOURCE%\manifest.json".
  echo Edit sync.cmd and set SOURCE to the build folder on your network drive.
  pause
  exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%SOURCE%\sync.ps1" -Source "%SOURCE%" -Dest "%~dp0."
pause
