gcc -Wall -Werror -Wpedantic -std=c11 -O3 overlap.c -o overlap.exe
echo Compiled overlap.exe
if %errorlevel% == 0 (
    @REM overlap.exe > out
    overlap.exe
)