@echo off

:: Set Project Name Here
set projectName=Mesa

echo.
echo Clearing Binaries and Intermediate
echo.

:: Delete Directories for Intermediate and Binaries
if exist Intermediate\ (
    rmdir /s /q "Intermediate"
    echo Deleted Directory - %~dp0Intermediate
) else (
    echo [No Clear Required] - %~dp0Intermediate
)


if exist Binaries\ (
    rmdir /s /q "Binaries"
    echo Deleted Directory - %~dp0Binaries
) else (
    echo [No Clear Required] - %~dp0Binaries
)

echo.
echo Clearing Plugin Binaries and Intermediate
echo.

:: Delete Directories for Intermediate and Binaries for Plugins
for /d %%i in (Plugins/*) do (

    echo Found Plugin "%%i"

    if exist "%~dp0Plugins\%%i\Intermediate"\ (

        :: Seemed like a good idea to put var here but if it fucks up you nuke the whole project (I learned the hard way)
        rmdir /s /q "%~dp0Plugins\%%i\Intermediate"
        echo Deleted Directory - Plugins/%%i/Intermediate
    ) else (
        echo [No Clear Required] - Plugins/%%i/Intermediate
    )
    echo.
)

echo Clearing Code Workspaces
echo.

:: Delete Directories for Code Workspaces
if exist .vs/ (
    rmdir /s /q ".vs"
    echo Deleted Directory - %~dp0.vs
)
if exist .vscode (
    rmdir /s /q ".vscode"
    echo Deleted Directory - %~dp0.vscode
)
if exist .idea (
    rmdir /s /q ".idea"
    echo Deleted Directory - %~dp0.idea
)

:: Delete Files for Code Workspaces, .sln ect
if exist %projectName%.xcworkspace (
    del /s /f /q "%projectName%.xcworkspace"
)
if exist %projectName%.sln (
    del /s /f /q "%projectName%.sln"
)
if exist %projectName%_Win64.sln (
    del /s /f /q "%projectName%_Win64.sln"
)
if exist %projectName%.code-workspace (
    del /s /f /q "%projectName%.code-workspace"
)

echo.
echo Regenerating Project, Please Wait.
echo.

:: Run CommandLet to regenerate .sln
"C:\Program Files (x86)\Epic Games\Launcher\Engine\Binaries\Win64\UnrealVersionSelector.exe" /projectfiles "%~dp0\%projectName%.uproject"