# Nuked SC-55 CLAP audio plugin

The plugin is built upon [J.C. Moyer's fork](https://github.com/jcmoyer/Nuked-SC55)
of nukeykt's original [Nuked SC-55](https://github.com/nukeykt/Nuked-SC55)
project.

The plugin aims to preserve an important part of DOS gaming history for all to
freely enjoy for posterity. It's only intended for **personal use** (e.g.,
retro gaming or writing music as a hobby) and **research purposes**.

As per the original Nuked SC-55 license, neither the code nor the published
binaries may be used directly or indirectly for the creation of commercial
Roland SC-55 emulation hardware boxes. Moreover, any use of the software in
commercial music production is prohibited, and so is including the plugin in
any commercial software package.


## Installation

Download the latest version for your operating system from the [releases
page](https://github.com/johnnovak/Nuked-SC55-CLAP/releases/) page, then unzip
it into one of these OS-specific locations:

- **Windows**
  - `C:\Program Files\Common Files\CLAP\`
  - `$LOCALAPPDATA/Programs/CLAP`

- **macOS**
  - `/Library/Audio/Plug-Ins/CLAP/`
  - `$HOME/Library/Audio/Plug-Ins/CLAP/`

- **Linux**
  - `/usr/lib/clap/`
  - `$HOME/.clap/`


### ROM files

The emulation needs dumps of the ROM chips of the original hardware to
function. If any of the ROM files for a given model are not present or they're
invalid, you won't be able to load the plugin for that particular model. It's
the easiest to grab the ROM files from
[here](https://archive.org/details/nuked-sc-55-clap-rom-files), but here are
the instructions how to set them up if you're getting them from elsewhere.

Create a `Nuked-SC55-Resources` directory in the folder where the CLAP plugin
resides, then a `ROMs` folder in it. You'll need to create subfolders in the
`ROMs` folder for the different models with specific names (e.g.,
`SC-55-v1.20`).

This is how the folder structure should look like:

```
Nuked-SC55-Resources
  ROMs
    SC-55-v1.20
      sc55_rom1.bin
      sc55_rom2.bin
      sc55_waverom1.bin
      sc55_waverom2.bin
      sc55_waverom3.bin

    SC-55-v1.21
	  ...

    SC-55-v2.00
	  ...

    SC-55mk2-v1.01
	  ...
```

On macOS, you can also put the `ROMs` folder in the `Resources` folder inside
the `Nuked-SC55.clap` application bundle.

Here is the list of required files for each supported model and their SHA1
hashes. Lookup is performed by filename, so make sure the names match exactly.

```
SC-55-v1.20/sc55_rom1.bin        dd01ec54027751c2f2f2e47bbb7a0bf3d1ca8ae2
SC-55-v1.20/sc55_rom2.bin        ffa8e3d5b3ec45485de4c13029bb4406c08ac9c3
SC-55-v1.20/sc55_waverom1.bin    8cc3c0d7ec0993df81d4ca1970e01a4b0d8d3775
SC-55-v1.20/sc55_waverom2.bin    80e6eb130c18c09955551563f78906163c55cc11
SC-55-v1.20/sc55_waverom3.bin    7454b817778179806f3f9d1985b3a2ef67ace76f

SC-55-v1.21/sc55_rom1.bin        dd01ec54027751c2f2f2e47bbb7a0bf3d1ca8ae2
SC-55-v1.21/sc55_rom2.bin        9c17f85e784dc1549ac1f98d457b353393331f6b
SC-55-v1.21/sc55_waverom1.bin    8cc3c0d7ec0993df81d4ca1970e01a4b0d8d3775
SC-55-v1.21/sc55_waverom2.bin    80e6eb130c18c09955551563f78906163c55cc11
SC-55-v1.21/sc55_waverom3.bin    7454b817778179806f3f9d1985b3a2ef67ace76f

SC-55-v2.00/sc55_rom1.bin        76f646bc03f66dbee7606f2181d4ea76f05ece7d
SC-55-v2.00/sc55_rom2.bin        6d6346b35c2379e9e6adc182214580e3d164b0c7
SC-55-v2.00/sc55_waverom1.bin    8cc3c0d7ec0993df81d4ca1970e01a4b0d8d3775
SC-55-v2.00/sc55_waverom2.bin    80e6eb130c18c09955551563f78906163c55cc11
SC-55-v2.00/sc55_waverom3.bin    7454b817778179806f3f9d1985b3a2ef67ace76f

SC-55mk2-v1.01/rom1.bin          b91bb1d9dccffe831b7cfde7800a3fe32b2fbda6
SC-55mk2-v1.01/rom2.bin          078cb5feea05e80bb9a1bb857a2163ee434fd053
SC-55mk2-v1.01/rom_sm.bin        4d48578d811a762a8e7bfaf18989bcac70ae1ba4
SC-55mk2-v1.01/waverom1.bin      96708cb21381c2fd03de4babbf7aea301c7594a6
SC-55mk2-v1.01/waverom2.bin      4d91cdeaed048d653dbf846a221003c3a3f08279
```

## Building

The official build method is via CMake and vcpkg (this is what the CI workflow
uses).

Alternatively, you can use [Meson](#Meson-alternative-build-method), but that
build method is less frequently tested.


### Prerequisites

#### All platforms

- CMake 3.27+
- vcpkg (latest)

#### Windows

- Visual Studio 2022 17.11.4+

#### macOS & Linux

- clang 16.0.0+
- ninja 1.12.1+


### Installing vcpkg

If you don't have vcpkg installed yet:

    git clone https://github.com/microsoft/vcpkg.git
    cd vcpkg && bootstrap-vcpkg.sh

Then append this to your `.bashrc` or `.zshrc`:

    export VCPKG_ROOT=<vcpkg_repo_location>
    export PATH=$VCPKG_ROOT:$PATH

On Windows, run `bootstrap-vcpkg.bat` instead and set the `PATH` Windows
enviroment variable accordingly.


### Building the project

#### Windows

First you'll need to configure the project:

    cmake -G "Visual Studio 17 2022" --preset=ninja

Use either command to build the **debug artifact** (this will create the
`Nuked-SC55.clap` plugin in `build\Debug`):

    cmake --build build
    cmake --build build --config Debug

To build the **release artifact** (this will create the `Nuked-SC55.clap`
plugin in `build\Release`):

    cmake --build build --config Release

The `msvc-sanitizer` preset is also available for debugging.


#### macOS

Run `build-macos.sh` to create the universal binary app bundle in the `out`
directory. Alternatively, follow the below manual steps:

First you'll need to configure the project. Use either command to configure the
debug or release build:

    cmake --preset=debug
    cmake --preset=release

Set `CMAKE_OSX_ARCHITECTURES` as well if you want to cross-compile, e.g.:

    cmake --preset=release -DCMAKE_OSX_ARCHITECTURES=arm64
    cmake --preset=release -DCMAKE_OSX_ARCHITECTURES=x86_64

To build the project (use the same preset):

    cmake --build build --preset=release

This will create the `Nuked-SC55.clap` app bundle in the `build` directory.

The `clang-sanitizer` preset is also available for debugging.


#### Linux

First you'll need to configure the project. Use either command to configure the
debug or release build:

    cmake --preset=debug
    cmake --preset=release

To build the project (use the same preset):

    cmake --build build --preset=release

This will create the `Nuked-SC55.clap` plugin in the `build` directory.

The following presets are also available for debugging:

- `gcc-sanitizer`
- `clang-sanitizer`


### Clean the project directory

To clean the `build` directory:

    cmake --build build --preset=release --target clean

Clean for a specific config (for MSVC):

    cmake --build build --preset=release --config Release --target clean

To start from scratch, delete the `build` directory and run the configure
command again.


### Meson (alternative build method)

Release build using statically-linked SpeexDSP (for portable releases):

    meson setup --prefer-static build/release
    ninja -C build/release

Release build using dynamically-linked system SpeexDSP (might break on other
people's systems):

    meson setup build/release
    ninja -C build/release

Debug build:

    meson setup --buildtype=debug build/debug
    ninja -C build/debug

Sanitizer build:

    meson setup --buildtype=debug -Db_sanitize=address,undefined build/sanitizer
    ninja -C build/sanitizer


## License

The Nuked SC-55 CLAP plugin based on Nuked SC-55 can be distributed and used
under the original MAME license (see [LICENSE](/LICENSE) file). Non-commercial
license was chosen to prevent making and selling SC-55 emulation boxes using
(or around) this code, as well as preventing from using it in commercial music
production.
