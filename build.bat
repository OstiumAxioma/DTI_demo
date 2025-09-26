@echo off
chcp 65001 > nul
echo.
echo ========================================
echo VTK Qt Project Build Script
echo ========================================
echo.

REM 让用户选择编译模式
echo Please select build configuration:
echo   1. Debug
echo   2. Release  
echo   3. Both (Debug and Release)
echo.
set /p choice="Enter your choice (1-3): "

REM 验证用户输入
if "%choice%"=="" (
    echo No choice entered. Defaulting to Release.
    set choice=2
)

if %choice% LSS 1 (
    echo Invalid choice. Defaulting to Release.
    set choice=2
)

if %choice% GTR 3 (
    echo Invalid choice. Defaulting to Release.
    set choice=2
)

REM 显示选择的配置
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

REM Create build directory
if not exist build mkdir build
cd build

REM Clean previous CMake cache (if exists)
if exist CMakeCache.txt (
    echo Cleaning previous CMake cache...
    del /q CMakeCache.txt 2>nul
)
if exist CMakeFiles (
    echo Cleaning CMakeFiles directory...
    rmdir /s /q CMakeFiles 2>nul
)

REM 检查是否需要传递Qt5路径参数
set CMAKE_ARGS=-G "Visual Studio 17 2022" -A x64

REM 如果用户提供了Qt5路径作为参数
if not "%~1"=="" (
    echo Using Qt5 path: %~1
    set CMAKE_ARGS=%CMAKE_ARGS% -DQt5_DIR="%~1/lib/cmake/Qt5" -DCMAKE_PREFIX_PATH="%~1"
)

REM 如果用户提供了VTK路径作为第二个参数
if not "%~2"=="" (
    echo Using VTK path: %~2
    set CMAKE_ARGS=%CMAKE_ARGS% -DVTK_DIR="%~2"
)

REM Configure project
echo Configuring project...
echo Running: cmake .. %CMAKE_ARGS%
cmake .. %CMAKE_ARGS%

REM Check if configuration was successful
if errorlevel 1 (
    echo.
    echo ERROR: CMake configuration failed!
    cd ..
    goto :end
)

echo CMake configuration successful!
echo.

REM 编译Release版本（如果选择）
if %BUILD_RELEASE%==1 (
    echo Building project ^(Release mode^)...
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

REM 编译Debug版本（如果选择）
if %BUILD_DEBUG%==1 (
    echo Building project ^(Debug mode^)...
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

if %BUILD_DEBUG%==1 if %BUILD_RELEASE%==1 (
    echo Built: Debug and Release configurations
    echo Debug executable: build\Exe\Debug\VTK_Qt_Project.exe
    echo Release executable: build\Exe\Release\VTK_Qt_Project.exe
) else if %BUILD_DEBUG%==1 (
    echo Built: Debug configuration only
    echo Debug executable: build\Exe\Debug\VTK_Qt_Project.exe
) else if %BUILD_RELEASE%==1 (
    echo Built: Release configuration only  
    echo Release executable: build\Exe\Release\VTK_Qt_Project.exe
)

echo ========================================

REM 提供运行选项
echo.
set /p run="Do you want to run the application now? (y/n): "
if /i "%run%"=="y" (
    echo.
    echo Setting VTK DLL path and running application...
    
    REM 添加VTK的bin目录到PATH
    set "PATH=%PATH%;D:\code\vtk8.2.0\VTK-8.2.0\bin"
    
    REM 根据编译的版本运行
    if %BUILD_RELEASE%==1 if exist "build\Exe\Release\VTK_Qt_Project.exe" (
        echo Starting Release version...
        start "" "build\Exe\Release\VTK_Qt_Project.exe"
    ) else if %BUILD_DEBUG%==1 if exist "build\Exe\Debug\VTK_Qt_Project.exe" (
        echo Starting Debug version...
        start "" "build\Exe\Debug\VTK_Qt_Project.exe"
    )
)

:end
echo.
echo Press any key to exit...
pause >nul 