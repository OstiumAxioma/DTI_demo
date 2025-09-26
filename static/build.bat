@echo off
chcp 65001 > nul

echo ========================================
echo DTIFiberLib Static Library Build Script
echo ========================================
echo.

:: 设置错误时继续执行
setlocal enabledelayedexpansion

:: 让用户选择编译模式
echo Please select build configuration:
echo   1. Debug
echo   2. Release  
echo   3. Both (Debug and Release)
echo.
set /p choice="Enter your choice (1-3): "

:: 验证用户输入
if "%choice%"=="" (
    echo No choice entered. Defaulting to Both.
    set choice=3
)

if %choice% LSS 1 (
    echo Invalid choice. Defaulting to Both.
    set choice=3
)

if %choice% GTR 3 (
    echo Invalid choice. Defaulting to Both.
    set choice=3
)

:: 显示选择的配置
echo.
if %choice%==1 (
    echo Selected: Debug configuration only
    set BUILD_DEBUG=1
    set BUILD_RELEASE=0
) else if %choice%==2 (
    echo Selected: Release configuration only
    set BUILD_DEBUG=0
    set BUILD_RELEASE=1
) else (
    echo Selected: Both Debug and Release configurations
    set BUILD_DEBUG=1
    set BUILD_RELEASE=1
)

echo.
echo ========================================
echo.

:: 创建构建目录
echo Creating build directory...
if not exist build mkdir build
cd build

:: 完全清理之前的构建（强制重新生成所有文件）
echo Cleaning previous build completely...
if exist CMakeCache.txt del /q CMakeCache.txt 2>nul
if exist CMakeFiles rmdir /s /q CMakeFiles 2>nul
if exist *.vcxproj del /q *.vcxproj 2>nul
if exist *.vcxproj.filters del /q *.vcxproj.filters 2>nul
if exist *.sln del /q *.sln 2>nul
if exist x64 rmdir /s /q x64 2>nul
if exist Release rmdir /s /q Release 2>nul
if exist Debug rmdir /s /q Debug 2>nul
if exist bin rmdir /s /q bin 2>nul
if exist lib rmdir /s /q lib 2>nul

:: 清理Visual Studio和CMake的中间文件
if exist TemplateLib.dir rmdir /s /q TemplateLib.dir 2>nul
if exist *.exp del /q *.exp 2>nul
if exist *.ilk del /q *.ilk 2>nul
if exist *.pdb del /q *.pdb 2>nul
if exist .vs rmdir /s /q .vs 2>nul

echo Clean complete.
echo.

:: 检查是否需要传递Qt5路径参数
set CMAKE_ARGS=-G "Visual Studio 17 2022" -A x64

:: 如果用户提供了Qt5路径作为参数
if not "%~1"=="" (
    echo Using Qt5 path: %~1
    set CMAKE_ARGS=%CMAKE_ARGS% -DQt5_DIR="%~1/lib/cmake/Qt5"
)

:: 如果用户提供了VTK路径作为第二个参数
if not "%~2"=="" (
    echo Using VTK path: %~2
    set CMAKE_ARGS=%CMAKE_ARGS% -DVTK_DIR="%~2"
)

:: 配置项目
echo Configuring DTIFiberLib project...
echo Running: cmake .. %CMAKE_ARGS%
cmake .. %CMAKE_ARGS%

if errorlevel 1 (
    echo.
    echo ERROR: CMake configuration failed!
    cd ..
    goto :end
)

echo CMake configuration successful!
echo.

:: 编译Release版本（如果选择）
if %BUILD_RELEASE%==1 (
    echo Building Static Library ^(Release mode^)...
    cmake --build . --config Release
    
    if errorlevel 1 (
        echo.
        echo ERROR: Release build failed!
        cd ..
        goto :end
    )
    
    echo Release build successful!
    echo.
)

:: 编译Debug版本（如果选择）
if %BUILD_DEBUG%==1 (
    echo Building Static Library ^(Debug mode^)...
    cmake --build . --config Debug
    
    if errorlevel 1 (
        echo.
        echo ERROR: Debug build failed!
        cd ..
        goto :end
    )
    
    echo Debug build successful!
    echo.
)

cd ..

echo.
echo ========================================
echo Build completed successfully!
echo ========================================
echo.

:: 确保根目录的lib文件夹存在
if not exist "..\lib" (
    echo Creating ..\lib directory...
    mkdir "..\lib"
)

:: 复制静态库文件到根目录的lib文件夹
echo Copying static library files to ..\lib...

:: 复制Release版本（如果编译了）
if %BUILD_RELEASE%==1 (
    if exist "build\lib\Release\DTIFiberLib.lib" (
        copy /Y "build\lib\Release\DTIFiberLib.lib" "..\lib\DTIFiberLib.lib" >nul
        if not errorlevel 1 (
            echo  - DTIFiberLib.lib ^(Release^) copied successfully
        ) else (
            echo  - ERROR: Failed to copy DTIFiberLib.lib
        )
    ) else (
        echo  - Warning: Release version of DTIFiberLib.lib not found at build\lib\Release\
    )
)

:: 复制Debug版本（如果编译了）
if %BUILD_DEBUG%==1 (
    if exist "build\lib\Debug\DTIFiberLib.lib" (
        copy /Y "build\lib\Debug\DTIFiberLib.lib" "..\lib\DTIFiberLib_d.lib" >nul
        if not errorlevel 1 (
            echo  - DTIFiberLib_d.lib ^(Debug^) copied successfully
        ) else (
            echo  - ERROR: Failed to copy DTIFiberLib_d.lib
        )
    ) else (
        echo  - Warning: Debug version of DTIFiberLib.lib not found at build\lib\Debug\
    )
)

:: 确保根目录的header文件夹存在
if not exist "..\header" (
    echo.
    echo Creating ..\header directory...
    mkdir "..\header"
)

:: 复制头文件到根目录的header文件夹
echo.
echo Copying header files to ..\header...
if exist "header\DTIFiberLib.h" (
    copy /Y "header\DTIFiberLib.h" "..\header\DTIFiberLib.h" >nul
    if not errorlevel 1 (
        echo  - DTIFiberLib.h copied successfully
    ) else (
        echo  - ERROR: Failed to copy DTIFiberLib.h
    )
) else (
    echo  - Warning: DTIFiberLib.h not found at header\
)

:: 可选：复制所有头文件（如果有多个）
:: xcopy /Y /S "header\*.h" "..\header\" >nul 2>&1

echo.
echo ========================================
echo Operation completed!
if %BUILD_DEBUG%==1 if %BUILD_RELEASE%==1 (
    echo Built: Debug and Release configurations
) else if %BUILD_DEBUG%==1 (
    echo Built: Debug configuration only
) else if %BUILD_RELEASE%==1 (
    echo Built: Release configuration only
)
echo Static libraries location: ..\lib\
echo Header files location: ..\header\
echo ========================================

:end
echo.
echo Press any key to exit...
pause >nul
endlocal