@echo off

set FXC=fxc.exe /Ges /O3 /WX /nologo /Qstrip_reflect /Qstrip_debug /Qstrip_priv
set CFLAGS=/Zi /Od /EHsc /Gy /Gw /W3 /nologo
set LFLAGS=/incremental:no /opt:ref

if exist *.cso del *.cso
::%FXC% /D VS_TRANSFORM /E VsTransform /Fo VsTransform.cso /T vs_5_1 100kDrawCalls.hlsl & if errorlevel 1 goto :end
::%FXC% /D PS_SHADE /E PsShade /Fo PsShade.cso /T ps_5_1 100kDrawCalls.hlsl & if errorlevel 1 goto :end

if exist eneida.exe del eneida.exe
if not exist eneida_external.pch (cl %CFLAGS% /c /Yceneida_external.h eneida_external.cpp)
cl %CFLAGS% /Yueneida_external.h eneida.cpp /link %LFLAGS% eneida_external.obj kernel32.lib user32.lib gdi32.lib
if exist eneida.obj del eneida.obj
if "%1" == "run" if exist eneida.exe (.\eneida.exe)

:end
