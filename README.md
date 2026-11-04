# Fast Five HD - Nintendo Switch Port (32-bit/armeabi-v7a)

This is a port of **Fast Five HD** (Gameloft) for Nintendo Switch using the [android32](https://github.com/aks796/android32) runtime. The game uses a custom **StormGLOFT** engine.

## Game Info
- **Package**: com.gameloft.android.GAND.GloftF5HP.ML
- **Architecture**: 32-bit only (armeabi-v7a)
- **Main libraries**: libFast5.so (8.3 MB), libStormGLOFT.so (900 KB)
- **Main Activity**: com.gameloft.android.GAND.GloftF5HP.ML.GLGame
- **Engine**: Custom StormGLOFT (Gameloft)

## Requirements
- Nintendo Switch with Atmosphère CFW
- [sphaira](https://github.com/official-ah/sphaira) for forwarder installation
- Original APK (player must provide their own copy)
- Docker with the ita2hos toolchain image

## Building

### Prerequisites
1. **Docker** installed and running
2. **libnx32** built from source:
   ```ash
   git clone https://github.com/aks796/libnx32 ../libnx32
   cd ../libnx32 && ./build.sh
   ```
3. **mesa32** (OpenGL ES/EGL) - download release and extract to portlibs32/
4. **android32 runtime** - cloned as 
untime/ submodule:
   ```ash
   git submodule add https://github.com/aks796/android32 runtime
   ```

### Build Steps
```ash
# Pull the toolchain image
docker pull ghcr.io/vita2hos/devcontainer/vita2hos:latest

# Build the port
./build.sh

# Or with custom flags
./build.sh DCR_GL_MESA=0  # Use null renderer (no mesa32 needed)
```

Output: ast5.nsp (installable via sphaira)

## Installation on Switch
1. Create folder: sd:/switch/fast5/
2. Copy ast5.nsp and your **original APK** to that folder
3. Open sphaira → Homebrew → Fast Five HD → **Install Forwarder**
4. Launch from Home Menu

## Project Structure
```
FAST5-NX/
├── source/                 # Port-specific code
│   ├── lib/               # Game .so files (libFast5.so, libStormGLOFT.so)
│   ├── imports.c          # JNI/bionic symbol table (generated)
│   ├── java.c             # JNI method implementations
│   ├── fast5_boot.c       # Game loading, EGL setup, main loop
│   ├── fast5_java.c       # JNI hooks for GameRenderer, GLGame, AudioTrack
│   ├── fast5_input.c      # Touch/controller mapping
│   ├── fast5_audio.c      # OpenSL ES audio hooks
│   ├── fast5_setup.c      # Setup plan for launcher
│   ├── port_config.h      # Port metadata & runtime config
│   └── config.h           # Build config
├── runtime/               # android32 runtime (git submodule)
├── portlibs32/            # libnx32 + mesa32 libs & headers
├── launcher/              # Forwarder NRO build
├── Makefile               # Build config
├── build.sh               # Docker build wrapper
└── .gitignore
```

## JNI Methods Implemented
From libFast5.so analysis (24 methods):
- **GameRenderer**: 
ativeResize, 
ativeInit, 
ativeDestroy, 
ativeRender, onKeyboardFinish
- **GameGLSurfaceView**: 
ativeResume, 
ativePause, 
ativePauseAllSounds
- **GLMediaPlayer**: 
ativeSetStopOnMusic, 
ativeInit, 
ativeGetTotalSounds*
- **GLGame**: 
ativeMPOnlineLoginSet, 
ativePauseInGame, 
ativeSetOnKeyUp/Down, 
ativeAccelerometer, 
ativeOrientation, 
ativeInit, 
ativeTouchPressed/Moved/Released
- **GLResLoader**: 
ativeInit
- **GLUtils.Device**: 
ativeInit

## Current Status
✅ **Build successful** - ast5.nsp (3.7 MB)  
✅ JNI bindings complete (24 methods)  
✅ EGL/GLES2 window (1024×600 render target)  
✅ Game loop with 
ativeRender calls  
✅ Input hooks (touch → controller mapping)  
✅ Audio hooks (virtual AudioTrack)  
✅ igli.bin loading from APK  

## Debugging
On Switch, logs are written to:
- sd:/switch/fast5/debug.log - Runtime debug output
- sd:/switch/fast5/crash.log - Crash reports

## Credits
- android32 runtime by [aks796](https://github.com/aks796)
- libnx32 by [aks796](https://github.com/aks796/libnx32)
- mesa32 by [aks796](https://github.com/aks796/mesa32)
- vita2hos toolchain by [vita2hos](https://github.com/vita2hos)

## Legal
**No game files are included.** Players must use their own legally obtained APK. This port only provides the wrapper/runtime to run the game on Switch.

---
*Reference structure based on tintin-nx and other android32 ports (Shadow Guardian, etc.)*
'@
