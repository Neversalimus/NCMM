@echo off
setlocal
if not defined NCMM_SCCACHE_WRAPPER_DIR exit /b 87
if not exist "%NCMM_SCCACHE_WRAPPER_DIR%\logs\" exit /b 87
set "NCMM_LOG_ATTEMPT=0"
:allocate_log
set /a NCMM_LOG_ATTEMPT+=1 >nul
if %NCMM_LOG_ATTEMPT% GTR 128 exit /b 87
set "NCMM_COMPILER_LOG=%NCMM_SCCACHE_WRAPPER_DIR%\logs\compile-%RANDOM%-%RANDOM%-%RANDOM%.log"
rem Newly started cmd processes can have identical random sequences. Atomically
rem reserve the name so concurrent compilers never overwrite another log.
mkdir "%NCMM_COMPILER_LOG%.lock" 2>nul
if errorlevel 1 goto allocate_log
sccache.exe cl.exe %* > "%NCMM_COMPILER_LOG%" 2>&1
set "NCMM_COMPILER_EXIT=%ERRORLEVEL%"
if "%NCMM_COMPILER_EXIT%"=="0" goto cached_success
rem A direct retry compiles the same source with the same MSVC flags. Preserve
rem its complete diagnostics even when MSBuild MultiToolTask drops tool output.
copy /y "%NCMM_COMPILER_LOG%" "%NCMM_COMPILER_LOG%.failed.log" >nul
cl.exe %* > "%NCMM_COMPILER_LOG%.direct.log" 2>&1
set "NCMM_COMPILER_EXIT=%ERRORLEVEL%"
type "%NCMM_COMPILER_LOG%.direct.log" >> "%NCMM_COMPILER_LOG%.failed.log"
echo Direct compiler exit: %NCMM_COMPILER_EXIT%>> "%NCMM_COMPILER_LOG%.failed.log"
rem MSBuild also scans tool stdout for "error :". A recovered cache startup
rem failure must not become a build error when the real compiler succeeds.
rem Keep the raw cache failure in its artifact; print real compiler diagnostics.
type "%NCMM_COMPILER_LOG%.direct.log"
if "%NCMM_COMPILER_EXIT%"=="0" echo Cache unavailable; direct compilation succeeded. Cache diagnostics retained.
exit /b %NCMM_COMPILER_EXIT%
:cached_success
type "%NCMM_COMPILER_LOG%"
exit /b 0
