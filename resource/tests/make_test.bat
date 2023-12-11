@echo off
Rem
Rem     Assembles Z80 source file into object code and generates Intel HEX format
Rem     to be loaded into Z80Explorer.
Rem
Rem     Give it an argument of an ASM file you want to use, or simply drag and drop
Rem     an asm file into it. If you drop an ASM file and there were errors while
Rem     assembling, this script will keep the DOS window open so you can see them.
Rem
zmac.exe --zmac --oo hex,bds,lst %1
if errorlevel 1 goto error
copy zout\%~n1.hex .
goto end
:error
@echo ------------------------------------------------------
@echo Errors processing %1
@echo ------------------------------------------------------
pause
:end
