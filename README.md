# Nuked SC-55 CLAP audio plugin

The plugin is built upon [J.C. Moyer's fork](https://github.com/jcmoyer/Nuked-SC55)
of nukeykt's original [Nuked SC-55](https://github.com/nukeykt/Nuked-SC55)
project.

The plugin aims to preserve an important part of DOS gaming history for all to
freely enjoy; it's only intended for personal use (e.g., retro gaming or
writing music as a hobby) and research purposes.

As per the original Nuked SC-55 license, neither the code nor the published
binaries may be used directly or indirectly for the creation of commercial
Roland SC-55 emulation hardware boxes. Moreover, any use of the software in
commercial music production is prohibited.


## Usage

TODO


## Building

### Prerequisites

#### All platforms

- CMake 3.29+
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

First you need to configure the project:

    cmake -G "Visual Studio 17 2022" --preset=default


To build the **debug artifacts** (output will be in the `build/Debug` subdirectory):

    cmake --build build --config Debug

Or just:

    cmake --build build


To build the **release artifacts** (output will be in the `build/Release` subdirectory):

    cmake --build build --config Release


#### macOS & Linux

First you need to configure the project:

    cmake --preset=default


To build the project (output will be in the `build/` subdirectory):

    cmake --build build


### Clean the project directory

To clean the `build` directory:

    cmake --build build --target clean

To start from scratch, delete the `build` directory and run the configure
command again.


## License

The Nuked SC-55 CLAP plugin based on Nuked SC-55 can be distributed and used
under the original MAME license (see [LICENSE](/LICENSE) file). Non-commercial
license was chosen to prevent making and selling SC-55 emulation boxes using
(or around) this code, as well as preventing from using it in commercial music
production.
