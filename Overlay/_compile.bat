@echo off
setlocal enabledelayedexpansion

echo Compiling Babble Gaze Test Overlay...

:: Configuration variables - Modify these to match your environment
set "OUTPUT_EXE=gaze_overlay.exe"
set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"
set "VS_ARCHITECTURE=x64"
set "OPENVR_PATH=C:\openvr" 
set "TURBOJPEG_PATH=C:\libjpeg-turbo64"
set "ONNXRUNTIME_PATH=C:\ortt"
set "LIBRARIES=openvr_api.lib user32.lib gdi32.lib kernel32.lib shell32.lib opengl32.lib ws2_32.lib turbojpeg.lib onnxruntime.lib"
set "ICON_FILE=app.ico"

:: Source files - separate C and C++ files
set "CPP_SOURCE_FILES=main.cpp overlay_manager.cpp math_utils.cpp dashboard_ui.cpp numpy_io.cpp frame_buffer.cpp routine.cpp rest_server.cpp subprocess.cpp"
set "C_SOURCE_FILES=jpeg_stream.c"

:: Check if cl.exe is in PATH
where cl.exe >nul 2>nul
if !ERRORLEVEL! NEQ 0 (
    echo MSVC compiler not found in PATH. Attempting to set up environment...
    
    :: Check if the VS_PATH file exists
    if exist "!VS_PATH!" (
        echo Setting up Visual Studio environment from: !VS_PATH!
        call "!VS_PATH!" !VS_ARCHITECTURE!
    ) else (
        echo Could not find Visual Studio at: !VS_PATH!
        echo Please modify the VS_PATH in this batch file or run from a Developer Command Prompt.
        pause
        exit /b 1
    )
)

:: Check again if cl.exe is available after setup
where cl.exe >nul 2>nul
if !ERRORLEVEL! NEQ 0 (
    echo Failed to set up MSVC compiler. Please check your Visual Studio installation.
    pause
    exit /b 1
)

:: Create build directory if it doesn't exist
if not exist "build" mkdir build

:: Create resource file for the icon
echo Creating resource file for the icon...
echo 1 ICON "%ICON_FILE%" > build\app.rc

:: Compile the resource file
echo Compiling resource file...
rc.exe /nologo build\app.rc

:: Define compiler and linker flags
set "COMMON_FLAGS=/nologo /W3 /O2 /D_CRT_SECURE_NO_WARNINGS /DWIN32 /D_WINDOWS"
set "CPP_FLAGS=/EHsc"
set "INCLUDE_DIRS=/I"%OPENVR_PATH%\headers" /I"%TURBOJPEG_PATH%\include" /I"%ONNXRUNTIME_PATH%\include""
set "LIBRARY_DIRS=/LIBPATH:"%OPENVR_PATH%\lib\win64" /LIBPATH:"%TURBOJPEG_PATH%\lib" /LIBPATH:"%ONNXRUNTIME_PATH%\lib""

cls
cl.exe /nologo /W3 /O2 /D_CRT_SECURE_NO_WARNINGS build_helper.c /Fe:"bhelp.exe"
echo.

:: Compile C source files (without /TP flag)
echo Compiling C source files:
for %%f in (%C_SOURCE_FILES%) do (
    bhelp /clformat
    cl.exe /nologo %COMMON_FLAGS% %INCLUDE_DIRS% /c %%f /Fo:"build\%%~nf.obj"
)

echo.

:: Compile C++ source files (with /TP and /EHsc flags)
echo Compiling C++ source files:
for %%f in (%CPP_SOURCE_FILES%) do (
    bhelp /clformat
    cl.exe /nologo %COMMON_FLAGS% %CPP_FLAGS% /TP %INCLUDE_DIRS% /c %%f /Fo:"build\%%~nf.obj"
)

:: Create a list of object files
set "OBJ_FILES="
for %%f in (%C_SOURCE_FILES% %CPP_SOURCE_FILES%) do (
    set "OBJ_FILES=!OBJ_FILES! build\%%~nf.obj"
)

:: Add the resource object to the list of object files
set "OBJ_FILES=!OBJ_FILES! build\app.res"

:: Link the object files
echo.
echo Linking...
link.exe /nologo /OUT:"build\%OUTPUT_EXE%" %OBJ_FILES% %LIBRARY_DIRS% %LIBRARIES%

bhelp /clformat
echo gaze_overlay.exe
echo.

:: Check if compilation was successful
if !ERRORLEVEL! EQU 0 (
    echo Compilation successful!
    echo.
    
    :: Copy the executable to the root directory as well
    copy /Y "build\!OUTPUT_EXE!" "!OUTPUT_EXE!" >nul
    
    :: Copy required DLLs
    if exist "%OPENVR_PATH%\bin\win64\openvr_api.dll" (
        copy /Y "%OPENVR_PATH%\bin\win64\openvr_api.dll" "build\openvr_api.dll" >nul
        copy /Y "%OPENVR_PATH%\bin\win64\openvr_api.dll" "openvr_api.dll" >nul
        copy /Y "%TURBOJPEG_PATH%\bin\turbojpeg.dll" "build\turbojpeg.dll" >nul
        copy /Y "%TURBOJPEG_PATH%\bin\turbojpeg.dll" "turbojpeg.dll" >nul
    )

    bhelp

    echo.
    echo Required files for release:
    echo !OUTPUT_EXE!
    echo openvr_api.dll
    echo turbojpeg.dll
    echo.
) else (
    echo Compilation failed with error code !ERRORLEVEL!.
)

endlocal
