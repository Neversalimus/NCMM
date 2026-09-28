@echo off
setlocal EnableExtensions DisableDelayedExpansion
set "PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if not exist "%PS%" set "PS=powershell.exe"
set "SCRIPT=%~dp0NCMM.ps1"
set "LOGDIR=%~dp0logs"
set "LOG=%LOGDIR%\launcher.log"
if not exist "%LOGDIR%" mkdir "%LOGDIR%" >nul 2>&1
set "INTERACTIVE=0"
set "ACTION="
set "RC=0"

if not exist "%SCRIPT%" (
  echo NCMM launcher ERROR: missing "%SCRIPT%"
  set "RC=3"
  goto :finish
)

if "%~1"=="" goto :menu
goto :direct

:menu
set "INTERACTIVE=1"
cls
echo NCMM Infrastructure 0.8.3.1 ^| Host 0.8.0 ^| API 2.0 Core
echo.
echo 1  Install / repair
echo 2  Check updates
echo 3  Apply certified update
echo 4  Probe current experimental
echo 5  Deep probe + create adapter
echo 6  Runtime verify ^(after first game launch^)
echo 7  Collect diagnostics ZIP
echo 8  Recover interrupted transaction
echo 9  Static package test
echo 0  Exit
echo.
choice /c 1234567890 /n /m "Choose action: "
set "CHOICE_RC=%ERRORLEVEL%"
echo.
if "%CHOICE_RC%"=="1" set "ACTION=Install"
if "%CHOICE_RC%"=="2" set "ACTION=Check"
if "%CHOICE_RC%"=="3" set "ACTION=Update"
if "%CHOICE_RC%"=="4" set "ACTION=Probe"
if "%CHOICE_RC%"=="5" set "ACTION=Adapter"
if "%CHOICE_RC%"=="6" set "ACTION=SelfTest"
if "%CHOICE_RC%"=="7" set "ACTION=Diagnostics"
if "%CHOICE_RC%"=="8" set "ACTION=Recover"
if "%CHOICE_RC%"=="9" set "ACTION=Test"
if "%CHOICE_RC%"=="10" exit /b 0
if not defined ACTION (
  echo Menu input failed. ERRORLEVEL=%CHOICE_RC%
  set "RC=2"
  goto :finish
)
goto :run_action

:direct
if /i "%~1"=="install"     set "ACTION=Install"
if /i "%~1"=="check"       set "ACTION=Check"
if /i "%~1"=="update"      set "ACTION=Update"
if /i "%~1"=="probe"       set "ACTION=Probe"
if /i "%~1"=="deepprobe"   set "ACTION=DeepProbe"
if /i "%~1"=="adapter"     set "ACTION=Adapter"
if /i "%~1"=="selftest"    set "ACTION=RuntimeVerify"
if /i "%~1"=="runtime"     set "ACTION=RuntimeVerify"
if /i "%~1"=="verify"      set "ACTION=OfflineVerify"
if /i "%~1"=="diagnostics" set "ACTION=Diagnostics"
if /i "%~1"=="recover"     set "ACTION=Recover"
if /i "%~1"=="test"        set "ACTION=Test"
if /i "%~1"=="package"     goto :package
if /i "%~1"=="setfeed"     goto :setfeed
if not defined ACTION (
  echo Unknown NCMM command: %~1
  echo Valid commands: install, check, update, probe, deepprobe, adapter, verify, selftest, runtime, diagnostics, recover, test, package, setfeed
  set "RC=2"
  goto :finish
)
goto :run_action

:package
if "%~2"=="" (
  echo Usage: NCMM.cmd package ^<path-to-zip^>
  set "RC=2"
  goto :finish
)
>>"%LOG%" echo [%date% %time%] action=Package START path=%~2
"%PS%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" -Action Package -PackagePath "%~2"
set "RC=%ERRORLEVEL%"
>>"%LOG%" echo [%date% %time%] action=Package EXIT=%RC%
goto :finish

:setfeed
if "%~2"=="" (
  echo Usage: NCMM.cmd setfeed ^<https-url^>
  set "RC=2"
  goto :finish
)
>>"%LOG%" echo [%date% %time%] action=SetFeed START
"%PS%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" -Action SetFeed -Url "%~2"
set "RC=%ERRORLEVEL%"
>>"%LOG%" echo [%date% %time%] action=SetFeed EXIT=%RC%
goto :finish

:run_action
>>"%LOG%" echo [%date% %time%] action=%ACTION% START
"%PS%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" -Action "%ACTION%"
set "RC=%ERRORLEVEL%"
>>"%LOG%" echo [%date% %time%] action=%ACTION% EXIT=%RC%
goto :finish

:finish
echo.
if "%RC%"=="0" (
  echo NCMM command completed successfully.
) else (
  echo NCMM command FAILED. Exit code: %RC%
  echo Launcher log: "%LOG%"
)
if "%INTERACTIVE%"=="1" pause
if not "%RC%"=="0" if "%INTERACTIVE%"=="0" pause
exit /b %RC%
