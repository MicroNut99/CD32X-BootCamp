@echo off
rem cartfx v4 (the SRAM's pins from flip-flops, a PSRAM read any console access touched done again): the Hurrican core
rem read in 5 (fpga_pro\map_cfx_hur, laid out by setup_cfx.sh), then the SD card's folder cartfx_work\sd5\ with it and
rem CFXTEST 4 (sd4's). done_cfx4.txt says it finished. Read cfx_hur.sta.summary first: never an .rbf whose timing failed.
set Q=S:\KoboPort\sega-toolchain-12.1\sega\altera_lite\quartus\bin64
set W=%~dp0
set C=S:\KoboPort\sega-toolchain-12.1\sega\cartfx_work
cd /d %W%mega-ed-pub\fpga\fpga_pro
echo building map_cfx_hur ...
"%Q%\quartus_sh.exe" --flow compile map_cfx_hur/mega-pro > %W%build_cfx4_hur.log 2>&1
echo map_cfx_hur exit %ERRORLEVEL% > %W%done_cfx_hur.txt
certutil -hashfile map_cfx_hur\output_files\mega-pro.rbf MD5 >> %W%done_cfx_hur.txt
copy /y map_cfx_hur\output_files\mega-pro.fit.summary %W%cfx_hur.fit.summary > nul
copy /y map_cfx_hur\output_files\mega-pro.sta.summary %W%cfx_hur.sta.summary > nul
if not exist "%W%cfx_out\hur" mkdir "%W%cfx_out\hur"
copy /y map_cfx_hur\output_files\mega-pro.rbf "%W%cfx_out\hur\mega-pro.rbf" >> %W%done_cfx_hur.txt
if not exist "%C%\sd5" mkdir "%C%\sd5"
copy /y "%C%\sd4\CFXTEST.32X" "%C%\sd5\CFXTEST.32X" > nul
copy /y map_cfx_hur\output_files\mega-pro.rbf "%C%\sd5\mega-pro.rbf" > nul
echo done > %W%done_cfx4.txt
