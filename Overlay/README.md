# Babble Gaze Eye Tracking

A VR eye tracking system using OpenVR that captures eye movement data and trains machine learning models for gaze prediction.

## Features

- **VR Overlay**: Real-time eye tracking overlay for SteamVR
- **Data Capture**: Records eye tracking calibration data with various blendshapes
- **ML Training**: Trains ONNX models using captured data
- **Cross-platform**: Supports macOS and Linux

## System Requirements

- **Operating System**: macOS 10.14+ or Linux (Ubuntu 18.04+, similar distributions)
- **VR Hardware**: SteamVR-compatible headset
- **Python**: 3.8+ (for trainer)
- **Compiler**: GCC 7+ or Clang 10+

## Dependencies

### Core Libraries

1. **OpenVR**: SteamVR SDK for VR integration
2. **TurboJPEG**: Fast JPEG compression/decompression
3. **ONNX Runtime**: Machine learning inference and training

### Python Dependencies (for trainer)

- PyTorch
- ONNX
- NumPy
- ONNXRuntime Training

## Installation

### macOS

#### Using Homebrew (Recommended)

```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install pkg-config cmake
brew install jpeg-turbo
brew install python@3.9

# Install OpenVR
# Download from: https://github.com/ValveSoftware/openvr/releases
# Extract to /opt/homebrew/include/openvr and /opt/homebrew/lib

# Install ONNX Runtime
wget https://github.com/microsoft/onnxruntime/releases/download/v1.16.3/onnxruntime-osx-x86_64-1.16.3.tgz
tar -xzf onnxruntime-osx-x86_64-1.16.3.tgz
sudo cp -r onnxruntime-osx-x86_64-1.16.3/include/* /opt/homebrew/include/
sudo cp -r onnxruntime-osx-x86_64-1.16.3/lib/* /opt/homebrew/lib/

# For Apple Silicon Macs, use:
# wget https://github.com/microsoft/onnxruntime/releases/download/v1.16.3/onnxruntime-osx-arm64-1.16.3.tgz
```

#### Manual Installation

1. **OpenVR**:
   ```bash
   # Download OpenVR from GitHub releases
   curl -L https://github.com/ValveSoftware/openvr/archive/refs/tags/v1.23.7.tar.gz | tar -xz
   cd openvr-1.23.7
   mkdir build && cd build
   cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
   make -j$(nproc)
   sudo make install
   ```

2. **TurboJPEG**:
   ```bash
   curl -L https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/2.1.4.tar.gz | tar -xz
   cd libjpeg-turbo-2.1.4
   mkdir build && cd build
   cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
   make -j$(nproc)
   sudo make install
   ```

3. **ONNX Runtime**: Download pre-built binaries from [releases](https://github.com/microsoft/onnxruntime/releases)

#### Python Dependencies

```bash
# Install Python dependencies for trainer
pip3 install torch torchvision onnx onnxruntime-training numpy

# Or using conda
conda install pytorch torchvision -c pytorch
conda install -c conda-forge onnx onnxruntime-training numpy
```

### Linux (Ubuntu/Debian)

#### Using Package Manager

```bash
# Update package list
sudo apt update

# Install build tools
sudo apt install build-essential cmake pkg-config git curl

# Install TurboJPEG
sudo apt install libturbojpeg0-dev

# Install Python and pip
sudo apt install python3 python3-pip python3-dev

# Install OpenGL development libraries
sudo apt install libgl1-mesa-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

#### Manual Installation

1. **OpenVR**:
   ```bash
   # Download and build OpenVR
   git clone https://github.com/ValveSoftware/openvr.git
   cd openvr
   mkdir build && cd build
   cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
   make -j$(nproc)
   sudo make install
   
   # Update library cache
   sudo ldconfig
   ```

2. **ONNX Runtime**:
   ```bash
   # Download pre-built Linux binaries
   wget https://github.com/microsoft/onnxruntime/releases/download/v1.16.3/onnxruntime-linux-x64-1.16.3.tgz
   tar -xzf onnxruntime-linux-x64-1.16.3.tgz
   sudo cp -r onnxruntime-linux-x64-1.16.3/include/* /usr/local/include/
   sudo cp -r onnxruntime-linux-x64-1.16.3/lib/* /usr/local/lib/
   sudo ldconfig
   ```

#### Python Dependencies

```bash
# Install Python dependencies
pip3 install torch torchvision onnx onnxruntime-training numpy

# For system-wide installation (not recommended)
sudo pip3 install torch torchvision onnx onnxruntime-training numpy
```

### Linux (CentOS/RHEL/Fedora)

```bash
# CentOS/RHEL 8+
sudo dnf install gcc gcc-c++ cmake pkg-config git curl
sudo dnf install libjpeg-turbo-devel python3 python3-pip python3-devel

# Fedora
sudo dnf install gcc gcc-c++ cmake pkg-config git curl
sudo dnf install libjpeg-turbo-devel python3 python3-pip python3-devel

# Follow manual installation steps for OpenVR and ONNX Runtime
```

### Linux (Arch)

```bash
# Install dependencies
sudo pacman -S base-devel cmake pkg-config git curl
sudo pacman -S libjpeg-turbo python python-pip

# Follow manual installation steps for OpenVR and ONNX Runtime
```

## Building

### Quick Start

```bash
# Configure the build
./configure

# Build all components
make

# Install (optional)
sudo make install
```

### Advanced Configuration

```bash
# Configure with custom options
./configure --prefix=/opt/babble --build-type=Debug --python=/usr/bin/python3.9

# Build only the overlay (no trainer)
./configure --disable-trainer
make

# Build only the trainer (no overlay)
./configure --disable-overlay
make

# Build with custom compiler
CC=clang CXX=clang++ ./configure
make
```

### Build Options

The configure script supports the following options:

- `--prefix=PATH`: Installation directory (default: `/usr/local`)
- `--build-type=TYPE`: `Debug` or `Release` (default: `Release`)
- `--disable-trainer`: Don't build the trainer component
- `--disable-overlay`: Don't build the overlay component
- `--python=PATH`: Path to Python executable
- `--help`: Show help message

### Environment Variables

You can customize the build using environment variables:

```bash
# Custom compiler flags
CFLAGS="-march=native" CXXFLAGS="-march=native" ./configure

# Custom library paths
PKG_CONFIG_PATH="/opt/local/lib/pkgconfig" ./configure

# Use different compilers
CC=gcc-11 CXX=g++-11 ./configure
```

## Usage

### Running the Overlay

1. Start SteamVR
2. Run the overlay:
   ```bash
   ./gaze_overlay
   ```

### Training Models

1. Generate the ONNX model:
   ```bash
   make model
   ```

2. Run training with captured data:
   ```bash
   ./trainer
   ```

### Calibration Process

The overlay provides a multi-stage calibration routine:

1. **Gaze Calibration**: Follow crosshair movements
2. **Eyelid Calibration**: Perform various eye expressions:
   - Eyes closed
   - Half-closed eyes (squinting)
   - Left eye wink
   - Right eye wink
   - Wide eyes (surprise)
   - Angry eyebrows

## Development

### Project Structure

```
├── main.cpp              # Main overlay application
├── trainer.cpp           # ML training application
├── overlay_manager.*     # VR overlay management
├── frame_buffer.*        # Frame capture and buffering
├── capture_data.h        # Data structures for capture
├── routine.*             # Calibration routine logic
├── math_utils.*          # Mathematical utilities
├── dashboard_ui.*        # Dashboard interface
├── rest_server.*         # REST API server
├── mkmodel7.py           # ONNX model generation
└── configure             # Build configuration script
```

### Building for Development

```bash
# Debug build with all checks enabled
./configure --build-type=Debug
make

# Development with custom flags
CXXFLAGS="-g -O0 -fsanitize=address" ./configure --build-type=Debug
make
```

### Contributing

1. Ensure your changes pass compilation on both macOS and Linux
2. Test the calibration routine thoroughly
3. Update documentation for any new dependencies

## Troubleshooting

### Common Issues

1. **OpenVR not found**:
   - Ensure SteamVR is installed and running
   - Check that OpenVR headers and libraries are in system paths
   - Try setting `PKG_CONFIG_PATH` to include OpenVR's .pc file location

2. **TurboJPEG linking errors**:
   - Install development package: `libjpeg-turbo-devel` (RHEL) or `libturbojpeg0-dev` (Debian)
   - Check library path with `ldconfig -p | grep turbojpeg`

3. **ONNX Runtime not found**:
   - Download the correct version for your platform
   - Ensure libraries are in `/usr/local/lib` or `/opt/homebrew/lib`
   - Run `ldconfig` on Linux after installation

4. **Python import errors**:
   - Install PyTorch with CUDA support if using GPU training
   - Use virtual environment to avoid conflicts:
     ```bash
     python3 -m venv venv
     source venv/bin/activate
     pip install torch torchvision onnx onnxruntime-training numpy
     ```

5. **Permission errors during build**:
   - Don't run `make` with `sudo`
   - Only use `sudo` for `make install`
   - Check file permissions in the source directory

### Platform-Specific Notes

#### macOS
- Xcode Command Line Tools are required
- Use Homebrew for easier dependency management
- Apple Silicon Macs need ARM64 versions of libraries

#### Linux
- Install development packages (`-dev` or `-devel` suffixes)
- Update library cache with `ldconfig` after installing libraries
- Some distributions may need additional OpenGL development packages

### Getting Help

- Check the GitHub Issues for known problems
- Ensure all dependencies are correctly installed
- Verify SteamVR is running and functional
- Test with a minimal VR application first
