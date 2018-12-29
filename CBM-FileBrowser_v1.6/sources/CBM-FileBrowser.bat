@echo off
:menu
cls
echo.
echo          ษอออออออออออออออออออออออออออออออออป
echo          บ  CBM FileBrowser compile        บ
echo          ฬอออออออออออออออออออออออออออออออออน
echo          บ                                 บ 
echo          บ Select system:                  บ 
echo          บ -----------------               บ 
echo          บ 1 - C64                         บ 
echo          บ 2 - C64 DTV                     บ   
echo          บ 3 - Vic20 unexpanded            บ
echo          บ 4 - Vic20 +3k ram               บ
echo          บ 5 - Vic20 +8k ram or plus       บ
echo          บ 6 - Vic20 with Mega-Cart        บ
echo          บ 7 - C16/C116/Plus4 (264 series) บ
echo          บ 8 - C128                        บ
echo          บ                                 บ
echo          บ 0 - quit                        บ
echo          บ                                 บ 
echo          ศอออออออออออออออออออออออออออออออออผ   
echo.

set /p system=           System: 

if "%system%"=="1" goto system1
if "%system%"=="2" goto system2
if "%system%"=="3" goto system3
if "%system%"=="4" goto system4
if "%system%"=="5" goto system5
if "%system%"=="6" goto system6
if "%system%"=="7" goto system7
if "%system%"=="8" goto system8
if "%system%"=="0" goto quit
goto menu



:system1        

rem **************************
rem * Commodore 64 (c64.asm) *
rem **************************

cls
echo.
echo Compiling CBM FileBrowser for: 
echo 1 - C64
echo.
echo acme --cpu 6502 -f cbm -o fb64.prg c64.asm
echo.

acme --cpu 6502 -f cbm -o fb64.prg c64.asm

if not %errorlevel%==0 goto :error

echo done, launching program (fb64.prg).
echo.
fb64.prg 

pause
exit






:system2 

rem *********************************
rem * Commodore 64 DTV (c64dtv.asm) *
rem *********************************

cls
echo.
echo Compiling CBM FileBrowser for: 
echo 2 - C64 DTV
echo.
echo acme --cpu 6502 -f cbm -o fb64dtv.prg c64dtv.asm
echo.

acme --cpu 6502 -f cbm -o fb64dtv.prg c64dtv.asm

if not %errorlevel%==0 goto :error

echo done, launching program (fb64dtv.prg).
echo.
fb64dtv.prg 

pause
exit






:system3

rem ************************************************
rem * Commodore Vic20 unexpanded (vic20-unexp.asm) *
rem ************************************************

cls
echo.
echo Compiling CBM FileBrowser for: 
echo 3 - Vic20 unexpanded
echo.
echo acme --cpu 6502 -f cbm -o fb20.prg vic20-unexp.asm
echo.

acme --cpu 6502 -f cbm -o fb20.prg vic20-unexp.asm

if not %errorlevel%==0 goto :error

echo done, launching program (fb20.prg).
echo.
fb20.prg 

pause
exit






:system4

rem ******************************************
rem * Commodore Vic20 +3k ram (vic20-3k.asm) *
rem ******************************************

cls
echo.
echo Compiling CBM FileBrowser for: 
echo 4 - Vic20 +3k ram
echo.
echo acme --cpu 6502 -f cbm -o fb20-3k.prg vic20-3k.asm
echo.

acme --cpu 6502 -f cbm -o fb20-3k.prg vic20-3k.asm

if not %errorlevel%==0 goto :error

echo done, launching program (fb20-3k.prg).
echo.
fb20-3k.prg

pause
exit






:system5

rem **************************************************
rem * Commodore Vic20 +8k ram or plus (vic20-8k.asm) *
rem **************************************************

cls
echo.
echo Compiling CBM FileBrowser for: 
echo 5 - Vic20 +8k ram or plus
echo.
echo acme --cpu 6502 -f cbm -o fb20-8k.prg vic20-8k.asm
echo.

acme --cpu 6502 -f cbm -o fb20-8k.prg vic20-8k.asm

if not %errorlevel%==0 goto :error

echo done, launching program (fb20-8k.prg).
echo.
fb20-8k.prg

pause
exit





:system6

rem *************************************************
rem * Commodore Vic20 with Mega-Cart (vic20-mc.asm) *
rem *************************************************

cls
echo.
echo Compiling CBM FileBrowser for: 
echo 6 - Vic20 with Mega-Cart
echo.
echo acme --cpu 6502 -f cbm -o fb20-mc.prg vic20-mc.asm
echo.

acme --cpu 6502 -f cbm -o fb20-mc.prg vic20-mc.asm

if not %errorlevel%==0 goto :error

echo done, launching program (fb20-mc.prg).
echo.
fb20-mc.prg

pause
exit


:system7

rem ************************************************
rem * Commodore 264 series C16/C116/Plus4 (c16.asm)*
rem ************************************************

cls
echo.
echo Compiling CBM FileBrowser for: 
echo 7 - C16/C116/+4 (264 series)
echo.
echo acme --cpu 6502 -f cbm -o fb16.prg c16.asm
echo.

acme --cpu 6502 -f cbm -o fb16.prg c16.asm

if not %errorlevel%==0 goto :error

echo done, launching program (fb16.prg).
echo.
fb16.prg 

pause
exit



:system8

rem ****************************
rem * Commodore 128 (c128.asm) *
rem ****************************

cls
echo.
echo Compiling CBM FileBrowser for: 
echo 8 - C128
echo.
echo acme --cpu 6502 -f cbm -o fb128.prg c128.asm
echo.

acme --cpu 6502 -f cbm -o fb128.prg c128.asm

if not %errorlevel%==0 goto :error

echo done, launching program (fb128.prg).
echo.
fb128.prg

pause
exit



:error
echo.
echo WARNING, ERROR ENCOUNTERED !!!
echo.
pause
:quit
exit
