@echo off
setlocal enabledelayedexpansion

echo Compiling MNIST Training with ONNX Runtime...

:: Configuration variables - Modify these to match your environment
set "OUTPUT_EXE=calibration_runner.exe"
set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"
set "VS_ARCHITECTURE=x64"
set "ONNXRUNTIME_PATH=C:\ortt" 
set "LIBRARIES=turbojpeg.lib onnxruntime.lib"
set "ICON_FILE=app.ico"
set "TURBOJPEG_PATH=C:\libjpeg-turbo64"

:: Source files
set "CPP_SOURCE_FILES=trainer.cpp numpy_io.cpp capture_reader.cpp"

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
set "COMMON_FLAGS=/nologo /W3 /Od /D_CRT_SECURE_NO_WARNINGS /DWIN32 /D_WINDOWS /std:c++17 /EHsc"
set "INCLUDE_DIRS=/I"%TURBOJPEG_PATH%\include" /I"%ONNXRUNTIME_PATH%\include""
set "LIBRARY_DIRS=/LIBPATH:"%TURBOJPEG_PATH%\lib" /LIBPATH:"%ONNXRUNTIME_PATH%\lib""

:: Check if CUDA is available and add its support if needed
if exist "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA" (
    echo CUDA detected, enabling GPU support...
    set "COMMON_FLAGS=!COMMON_FLAGS! /DUSE_CUDA"
    set "LIBRARIES=!LIBRARIES! cudart.lib"
    set "LIBRARY_DIRS=!LIBRARY_DIRS! /LIBPATH:"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.0\lib\x64""
    set "INCLUDE_DIRS=!INCLUDE_DIRS! /I"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.0\include""
)

cls
if exist "build_helper.c" (
    cl.exe /nologo /W3 /Od /D_CRT_SECURE_NO_WARNINGS build_helper.c /Fe:"bhelp.exe"
    echo.
)

:: Compile C++ source files
echo Compiling C++ source files:
for %%f in (%CPP_SOURCE_FILES%) do (
    if exist "bhelp.exe" bhelp /clformat
    cl.exe !COMMON_FLAGS! !INCLUDE_DIRS! /c %%f /Fo:"build\%%~nf.obj"
)

:: Create a list of object files
set "OBJ_FILES="
for %%f in (%CPP_SOURCE_FILES%) do (
    set "OBJ_FILES=!OBJ_FILES! build\%%~nf.obj"
)

:: Add the resource object to the list of object files
set "OBJ_FILES=!OBJ_FILES! build\app.res"

:: Link the object files
echo.
echo Linking...
link.exe /nologo /OUT:"build\%OUTPUT_EXE%" %OBJ_FILES% %LIBRARY_DIRS% %LIBRARIES%

if exist "bhelp.exe" (
    bhelp /clformat
    echo %OUTPUT_EXE%
    echo.
)

:: Check if compilation was successful
if !ERRORLEVEL! EQU 0 (
    echo Compilation successful!
    echo.
    
    :: Copy the executable to the root directory as well
    copy /Y "build\!OUTPUT_EXE!" "!OUTPUT_EXE!" >nul
    
    :: Copy required DLLs
    if exist "%ONNXRUNTIME_PATH%\bin\onnxruntime.dll" (
        copy /Y "%ONNXRUNTIME_PATH%\bin\onnxruntime.dll" "build\onnxruntime.dll" >nul
        copy /Y "%ONNXRUNTIME_PATH%\bin\onnxruntime.dll" "onnxruntime.dll" >nul
        
        if exist "%ONNXRUNTIME_PATH%\bin\onnxruntime_training.dll" (
            copy /Y "%ONNXRUNTIME_PATH%\bin\onnxruntime_training.dll" "build\onnxruntime_training.dll" >nul
            copy /Y "%ONNXRUNTIME_PATH%\bin\onnxruntime_training.dll" "onnxruntime_training.dll" >nul
        )
    )

    if exist "bhelp.exe" bhelp

    echo.
    echo Required files for running:
    echo !OUTPUT_EXE!
    echo onnxruntime.dll
    echo onnxruntime_training.dll
    echo.
    echo Usage: !OUTPUT_EXE! ^<training_model.onnx^> ^<optimizer_model.onnx^> ^<checkpoint_path^> ^<mnist_images^> ^<mnist_labels^>
    echo.
) else (
    echo Compilation failed with error code !ERRORLEVEL!.
)

endlocal
pause