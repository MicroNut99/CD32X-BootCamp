@echo off
rem The CD32X cart compositor (CD32X-BootCamp cartfx/): the three cores - Hurrican, Jazz, SuperTux - each krikzz's
rem map_ssf unchanged with the engine beside it (fpga_pro\map_cfx_hur / _oj / _stx, laid out by setup_cfx.sh).
rem For each: the exit code and the .rbf's md5 in done_cfx_<game>.txt, the fit and timing summaries next to this file,
rem the .rbf in cfx_out\<game>\mega-pro.rbf. Read the timing summary before using an .rbf: never one whose timing failed.
set Q=S:\KoboPort\sega-toolchain-12.1\sega\altera_lite\quartus\bin64
set W=%~dp0
cd /d %W%mega-ed-pub\fpga\fpga_pro
call :one hur
call :one oj
call :one stx
echo all three done > %W%done_cfx_all.txt
goto :eof

:one
echo building map_cfx_%1 ...
"%Q%\quartus_sh.exe" --flow compile map_cfx_%1/mega-pro > %W%build_cfx_%1.log 2>&1
echo map_cfx_%1 exit %ERRORLEVEL% > %W%done_cfx_%1.txt
certutil -hashfile map_cfx_%1\output_files\mega-pro.rbf MD5 >> %W%done_cfx_%1.txt
copy /y map_cfx_%1\output_files\mega-pro.fit.summary %W%cfx_%1.fit.summary > nul
copy /y map_cfx_%1\output_files\mega-pro.sta.summary %W%cfx_%1.sta.summary > nul
if not exist "%W%cfx_out\%1" mkdir "%W%cfx_out\%1"
copy /y map_cfx_%1\output_files\mega-pro.rbf "%W%cfx_out\%1\mega-pro.rbf" >> %W%done_cfx_%1.txt
goto :eof
