# Compilar WorshipStudio

El código está en `app/` (fork de OBS Studio). Las dependencias precompiladas (Qt, FFmpeg, CEF...) las descarga
CMake solo la primera vez en `app/.deps/`.

## macOS (Apple Silicon o Intel)

Requisitos: Xcode 16 o posterior, CMake 3.28+.

```sh
cd app
cmake --preset macos -B ~/WorshipStudio-build \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DOBS_CODESIGN_IDENTITY=- \
  -DENABLE_BROWSER=ON -DENABLE_WHATSNEW=OFF -DCMAKE_COMPILE_WARNING_AS_ERROR=OFF
cmake --build ~/WorshipStudio-build --config RelWithDebInfo
open ~/WorshipStudio-build/frontend/RelWithDebInfo/WorshipStudio.app
```

Usar `x86_64` en lugar de `arm64` para Mac con Intel.

> **Importante:** no guardes el proyecto ni la carpeta de compilación en el Escritorio o Documentos si están
> sincronizados con iCloud. iCloud "libera" archivos del código (quedan vacíos hasta que se descargan) y la
> compilación se queda colgada; además `codesign` falla con "resource fork, Finder information, or similar
> detritus not allowed". Usá por ejemplo `~/Proyectos/WorshipStudio`.

## Windows 10/11 (x64)

Requisitos: Visual Studio 2026 (o 2022 cambiando el generador) con "Desarrollo para el escritorio con C++",
Windows SDK 10.0.22621 o posterior, CMake 3.28+, Git.

```powershell
cd app
cmake --preset windows-x64 -DENABLE_BROWSER=ON -DCMAKE_COMPILE_WARNING_AS_ERROR=OFF
cmake --build --preset windows-x64 --config RelWithDebInfo
# El ejecutable queda en build_x64\rundir\RelWithDebInfo\bin\64bit\
```

Los PDF se dibujan con `Windows.Data.Pdf` (incluido en Windows). Para PowerPoint se usa PowerPoint instalado o
LibreOffice.

## Linux (Ubuntu 24.04)

```sh
sudo apt install build-essential cmake ninja-build pkg-config qt6-base-dev qt6-base-private-dev libqt6svg6-dev \
  libavcodec-dev libavdevice-dev libavfilter-dev libavformat-dev libavutil-dev libswresample-dev libswscale-dev \
  libcurl4-openssl-dev libjansson-dev libx11-xcb-dev libgl1-mesa-dev libwayland-dev libpipewire-0.3-dev \
  libpulse-dev libx264-dev libmbedtls-dev uthash-dev libsimde-dev libluajit-5.1-dev python3-dev swig \
  libxcb-randr0-dev libxcb-shm0-dev libxcb-xinerama0-dev libxcb-composite0-dev libxinerama-dev libxcb-xfixes0-dev \
  libxss-dev libglvnd-dev libxkbcommon-dev libasound2-dev libfontconfig-dev libfreetype6-dev libjack-jackd2-dev \
  libsndio-dev libspeexdsp-dev libudev-dev libv4l-dev libva-dev libvlc-dev libpci-dev libdrm-dev \
  nlohmann-json3-dev libwebsocketpp-dev libasio-dev libqrcodegencpp-dev librist-dev libsrt-openssl-dev \
  extra-cmake-modules poppler-utils
cd app
cmake --preset ubuntu -DENABLE_BROWSER=OFF -DCMAKE_COMPILE_WARNING_AS_ERROR=OFF
cmake --build build_ubuntu
```

En Linux los PDF usan `pdftoppm`/`pdfinfo` (paquete `poppler-utils`) y PowerPoint se convierte con LibreOffice.

## Autoprueba

```sh
WORSHIPSTUDIO_SELFTEST=/tmp/ws-prueba WorshipStudio.app/Contents/MacOS/WorshipStudio
# Solo presentaciones:
WORSHIPSTUDIO_SELFTEST=/tmp/ws-prueba WORSHIPSTUDIO_SELFTEST_ONLY=presentaciones \
WORSHIPSTUDIO_SELFTEST_PRESENTATIONS="/ruta/libro.pdf;/ruta/culto.pptx" WorshipStudio.app/Contents/MacOS/WorshipStudio
```

Guarda capturas reales de la salida de OBS y `reporte.txt` en la carpeta indicada.
