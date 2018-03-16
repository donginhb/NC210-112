@REM This batch file has been generated by the IAR Embedded Workbench
@REM C-SPY Debugger, as an aid to preparing a command line for running
@REM the cspybat command line utility using the appropriate settings.
@REM
@REM Note that this file is generated every time a new debug session
@REM is initialized, so you may want to move or rename the file before
@REM making changes.
@REM
@REM You can launch cspybat by typing the name of this batch file followed
@REM by the name of the debug file (usually an ELF/DWARF or UBROF file).
@REM
@REM Read about available command line parameters in the C-SPY Debugging
@REM Guide. Hints about additional command line parameters that may be
@REM useful in specific cases:
@REM   --download_only   Downloads a code image without starting a debug
@REM                     session afterwards.
@REM   --silent          Omits the sign-on message.
@REM   --timeout         Limits the maximum allowed execution time.
@REM 


@echo off 

if not "%1" == "" goto debugFile 

@echo on 

"D:\Program Files (x86)\IAR Systems\Embedded Workbench 7.2\common\bin\cspybat" -f "V:\我的项目\04 长沙南车\2015年后项目\NC15-004-A 内燃机能耗系统\00 归档\05 嵌入式程序归档\NC136B-100-000 监测装置\NC136B-120-000 测量模块\Library\Project\IAR\settings\NC136B-120-000.Release-RC-A.general.xcl" --backend -f "V:\我的项目\04 长沙南车\2015年后项目\NC15-004-A 内燃机能耗系统\00 归档\05 嵌入式程序归档\NC136B-100-000 监测装置\NC136B-120-000 测量模块\Library\Project\IAR\settings\NC136B-120-000.Release-RC-A.driver.xcl" 

@echo off 
goto end 

:debugFile 

@echo on 

"D:\Program Files (x86)\IAR Systems\Embedded Workbench 7.2\common\bin\cspybat" -f "V:\我的项目\04 长沙南车\2015年后项目\NC15-004-A 内燃机能耗系统\00 归档\05 嵌入式程序归档\NC136B-100-000 监测装置\NC136B-120-000 测量模块\Library\Project\IAR\settings\NC136B-120-000.Release-RC-A.general.xcl" "--debug_file=%1" --backend -f "V:\我的项目\04 长沙南车\2015年后项目\NC15-004-A 内燃机能耗系统\00 归档\05 嵌入式程序归档\NC136B-100-000 监测装置\NC136B-120-000 测量模块\Library\Project\IAR\settings\NC136B-120-000.Release-RC-A.driver.xcl" 

@echo off 
:end