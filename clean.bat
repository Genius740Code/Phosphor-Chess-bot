@echo off
echo Cleaning project...

echo Removing object files...
del *.o >nul 2>&1

echo Cleaning build directory...
del /q build\exe\*.exe >nul 2>&1
del /q build\exe\*.dll >nul 2>&1

echo Project cleaned successfully.
pause 