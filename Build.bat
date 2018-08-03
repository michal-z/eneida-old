@echo off

set FXC=fxc.exe /Ges /O3 /WX /nologo /Qstrip_reflect /Qstrip_debug /Qstrip_priv
set CFLAGS=/Zi /O2 /EHsc

if exist *.cso del *.cso
::%FXC% /D VS_TRANSFORM /E VsTransform /Fo VsTransform.cso /T vs_5_1 100kDrawCalls.hlsl & if errorlevel 1 goto :end
::%FXC% /D PS_SHADE /E PsShade /Fo PsShade.cso /T ps_5_1 100kDrawCalls.hlsl & if errorlevel 1 goto :end

if exist eneida.exe del eneida.exe
if not exist PreCompiled.pch (cl %CFLAGS% /c /YcPreCompiled.h PreCompiled.cpp)
cl %CFLAGS% /YuPreCompiled.h Main.cpp /link PreCompiled.obj kernel32.lib user32.lib gdi32.lib /incremental:no /opt:ref /out:eneida.exe
if exist Main.obj del Main.obj
if "%1" == "run" if exist eneida.exe (.\eneida.exe)

:end
