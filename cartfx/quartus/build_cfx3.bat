@echo off
rem cartfx v3 (the command read fixed): CFXTEST 3 (built in WSL with the owner's toolchain, from cartfx_work\cartfx\test)
rem and the Hurrican core read in 5 (fpga_pro\map_cfx_hur, laid out by setup_cfx.sh), then the SD card's folder
rem cartfx_work\sd3\ (CFXTEST.32X and that mega-pro.rbf). One double-click; done_cfx3.txt says it finished.
rem Read cfx_hur.sta.summary before the .rbf goes to the card: never one whose timing failed.
set Q=S:\KoboPort\sega-toolchain-12.1\sega\altera_lite\quartus\bin64
set W=%~dp0
set C=S:\KoboPort\sega-toolchain-12.1\sega\cartfx_work
echo building CFXTEST.32X in WSL ...
wsl -e bash -lc "cd /mnt/s/KoboPort/sega-toolchain-12.1/sega/cartfx_work/cartfx/test && bash build.sh > ../../cfxtest_build.log 2>&1; echo exit $? >> ../../cfxtest_build.log"
cd /d %W%mega-ed-pub\fpga\fpga_pro
echo building map_cfx_hur ...
"%Q%\quartus_sh.exe" --flow compile map_cfx_hur/mega-pro > %W%build_cfx3_hur.log 2>&1
echo map_cfx_hur exit %ERRORLEVEL% > %W%done_cfx_hur.txt
certutil -hashfile map_cfx_hur\output_files\mega-pro.rbf MD5 >> %W%done_cfx_hur.txt
copy /y map_cfx_hur\output_files\mega-pro.fit.summary %W%cfx_hur.fit.summary > nul
copy /y map_cfx_hur\output_files\mega-pro.sta.summary %W%cfx_hur.sta.summary > nul
if not exist "%W%cfx_out\hur" mkdir "%W%cfx_out\hur"
copy /y map_cfx_hur\output_files\mega-pro.rbf "%W%cfx_out\hur\mega-pro.rbf" >> %W%done_cfx_hur.txt
if not exist "%C%\sd3" mkdir "%C%\sd3"
copy /y "%C%\cartfx\test\CFXTEST.32X" "%C%\sd3\CFXTEST.32X" > nul
copy /y map_cfx_hur\output_files\mega-pro.rbf "%C%\sd3\mega-pro.rbf" > nul
echo done > %W%done_cfx3.txt
