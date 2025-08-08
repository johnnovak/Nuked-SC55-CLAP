# Nuked SC-55 CLAP audio plug-in

The Nuked SC-55 CLAP audio plug-in ([CLAP](https://cleveraudio.org/)) is built upon [J.C. Moyer's fork](https://github.com/jcmoyer/Nuked-SC55) of nukeykt's original [Nuked SC-55](https://github.com/nukeykt/Nuked-SC55) project.

The plug-in aims to preserve an important part of DOS gaming history for all to freely enjoy for posterity. It is only intended for **personal use** (e.g., retro gaming or writing music as a hobby) and **research purposes**. See the [License](#license) section for additional details.

## Installation

Download the latest version for your operating system from the [releases page](https://github.com/johnnovak/Nuked-SC55-CLAP/releases) page, then unzip it into one of these OS-specific locations:

- **Windows**

  - `C:\Program Files\Common Files\CLAP\`
  - `$LOCALAPPDATA\Programs\CLAP\`

- **macOS**

  - `/Library/Audio/Plug-Ins/CLAP/`
  - `$HOME/Library/Audio/Plug-Ins/CLAP/`

- **Linux**

  - `/usr/lib/clap/`
  - `$HOME/.clap/`

### macOS Gatekeeper

If macOS Gatekeeper prevents the plug-in from running, you will need to explicitly whitelist it with the following command from the terminal (replace `<path-to>` with the actual path to `Nuked-SC55.clap`):

```zsh
sudo xattr -rd com.apple.quarantine <path-to>/Nuked-SC55.clap
```

### ROM files

The emulation needs dumps of the original hardware's ROM chips to function. If any of the ROM files for a given model are not present or they are invalid, you will not be able to load the plugin for that particular model. It is the easiest to grab the ROM files from [here](https://archive.org/details/nuked-sc-55-clap-rom-files), but here are the instructions how to set them up if you are getting them from elsewhere.

Create a `Nuked-SC55-Resources` directory in the folder where the CLAP plugin resides with a `ROMs` folder in it. Finally, create subfolders inside the `ROMs` folder for the different models with specific names (e.g., `SC-55-v1.20`).

This is how the folder structure should look:

```text
Nuked-SC55-Resources
  ROMs
    SC-55-v1.00
	  ...

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

On macOS, you can also put the `ROMs` folder in the `Resources` folder inside the `Nuked-SC55.clap` application bundle.

Here is the list of required files for each supported model and their SHA1 hashes. Lookup is performed by filename, so make sure the names match exactly.

```
SC-55-v1.00/sc55_rom1.bin        675ea03634429c49fdeebf14849953bc1dd844aa
SC-55-v1.00/sc55_rom2.bin        f7f8ac018d24be8de6cc37ed4a3b3bfc0953510a
SC-55-v1.00/sc55_waverom1.bin    8cc3c0d7ec0993df81d4ca1970e01a4b0d8d3775
SC-55-v1.00/sc55_waverom2.bin    80e6eb130c18c09955551563f78906163c55cc11
SC-55-v1.00/sc55_waverom3.bin    7454b817778179806f3f9d1985b3a2ef67ace76f

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

The main build method is via CMake and vcpkg. This is what the CI workflow uses.

On Linux you can also build without vcpkg, using [system libraries](#using-system-libs-on-linux-alternative-build-method).

### Prerequisites

#### All platforms

- CMake (3.27.0 or later)
- vcpkg (latest)

#### Windows

- Visual Studio 2022 (17.11.0 or later)

#### macOS or Linux

- Clang (16.0.0 or later)
- Ninja (1.12.0 or later)

### Installing vcpkg

If you don't have vcpkg installed yet:

```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg && bootstrap-vcpkg.sh
```

Then append this to your `.bashrc` or `.zshrc`:

```bash
export VCPKG_ROOT=<vcpkg_repo_location>
export PATH=$VCPKG_ROOT:$PATH
```

On Windows, run `bootstrap-vcpkg.bat` instead and set the `PATH` Windows enviroment variable accordingly:

```pwsh
$env:VCPKG_ROOT="<vcpkg_repo_location>"
$env:PATH="$env:VCPKG_ROOT;$env:PATH"
```

### Building the project

#### Windows

First you'll need to configure the project:

```pwsh
cmake --preset debug-windows-x64
```

Then build the **debug artifact** (this will create the `Nuked-SC55.clap` plugin in `build\debug-windows-x64`):

```pwsh
cmake --build --preset debug-windows-x64
```

To configure and build the **release artifact** (this will create the `Nuked-SC55.clap` plugin in `build\release-windows-x64`):

```pwsh
cmake --preset release-windows-x64
cmake --build --preset release-windows-x64
```

The `msvc-sanitizer` preset is also available for debugging.

#### macOS

Run [`build-macos.sh`](build-macos.sh) to create the universal binary app bundle in the `out` directory. Alternatively, follow the manual steps below.

First you'll need to configure the project. Use the corresponding command to configure the debug or release build:

```zsh
cmake --preset debug-macos-arm64
cmake --preset release-macos-arm64
```

To cross-compile, (e.g., for x86_64):

```zsh
cmake --preset debug-macos-x64
cmake --preset release-macos-x64
```

To build the project (use the same preset):

```zsh
cmake --build --preset <preset-used-to-configure>
```

This will create the `Nuked-SC55.clap` app bundle in the `build/<preset>` directory.

The `clang-sanitizer` preset is also available for debugging.

#### Linux

First you'll need to configure the project. Use the corresponding command to configure the debug or release build:

```zsh
cmake --preset debug-linux-x64
cmake --preset release-linux-x64
```

To build the project (use the same preset):

```zsh
cmake --build --preset <preset-used-to-configure>
```

This will create the `Nuked-SC55.clap` plug-in in the `build/<preset>` directory.

The following presets are also available for debugging:

- `gcc-sanitizer`
- `clang-sanitizer`

#### Using system libs on Linux (alternative build method)

On Linux it is possible to compile using system libraries, without using vcpkg.
Prerequisites are now different:

- Clang (16.0.0 or later)
- Ninja (1.12.0 or later)
- SpeexDSP (1.2.1 or later)

To configure the project, use one of the `syslibs` presets:

```zsh
cmake --preset debug-linux-syslibs-x64
cmake --preset release-syslibs-linux-x64
```

Build the project as normal.

The following presets are also available for debugging:

- `gcc-sanitizer-syslibs`
- `clang-sanitizer-syslibs`

### Cleaning the build directory

To clean the `build/<preset>` directory (e.g., for MSVC):

```bash
cmake --build --preset release-windows-x64 --target clean
```

To start from scratch, delete the `build` directory and run the configure commands again.

## License

Nuked SC-55 CLAP, based on Nuked SC-55, can be distributed and used under the terms of original MAME license (see [LICENSE](LICENSE) file). As per the license, neither the code nor the published binaries may be used directly or indirectly for the creation of commercial Roland SC-55 emulation hardware boxes. Moreover, any use of the software in commercial music production is prohibited and so is including the plug-in in any commercial software package.
