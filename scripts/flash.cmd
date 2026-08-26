@echo off
rem Doppelklick-Wrapper fuer flash.ps1 - umgeht die Execution-Policy-Sperre
rem fuer unsignierte .ps1-Dateien nur fuer diesen einen Aufruf. Fragt beim
rem Start interaktiv, welches der vier Sensormeter-Projekte geflasht werden
rem soll (siehe flash.ps1).
setlocal
set SCRIPT_DIR=%~dp0
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%flash.ps1" %*
echo.
pause
