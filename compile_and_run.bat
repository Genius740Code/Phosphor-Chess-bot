@echo off
echo Compiling project...
mingw32-make -f MakeFile

if %ERRORLEVEL% neq 0 (
    echo Compilation failed.
    pause
    exit /b 1
)

echo Moving .exe files to build\exe directory...
move *.exe build\exe\ >nul 2>&1

echo Copying DLL files to build\exe directory for runtime...
copy dll\*.dll build\exe\ >nul 2>&1

echo Running program...
cd build\exe
start main.exe
cd ..\..

echo Done. 