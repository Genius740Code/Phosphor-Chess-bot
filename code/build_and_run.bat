@echo off
echo Setting up SFML and building the project...

REM Run the setup script first
call setup_sfml.bat

REM Change to the code directory and build the project
cd code
mingw32-make

REM Check if build was successful
if %ERRORLEVEL% EQU 0 (
    echo Build successful! Running the application...
    cd ..
    main.exe
) else (
    echo Build failed with error code %ERRORLEVEL%.
    cd ..
    pause
) 