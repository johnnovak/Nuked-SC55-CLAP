cmake_minimum_required(VERSION 3.27)

project(Nuked-SC55-CLAP
    VERSION     0.10.0
    DESCRIPTION "Nuked SC-55 CLAP audio plugin"
    LANGUAGES   C CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif ()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# TODO
#configure_file(config.h.in config.h)

add_library(Nuked-SC55-CLAP MODULE
    src/nuked-sc55/backend/emu.cpp
    src/nuked-sc55/backend/lcd.cpp
    src/nuked-sc55/backend/mcu.cpp
    src/nuked-sc55/backend/mcu_interrupt.cpp
    src/nuked-sc55/backend/mcu_opcodes.cpp
    src/nuked-sc55/backend/mcu_timer.cpp
    src/nuked-sc55/backend/pcm.cpp
    src/nuked-sc55/backend/rom.cpp
    src/nuked-sc55/backend/rom_io.cpp
    src/nuked-sc55/backend/submcu.cpp

    src/nuked-sc55/backend/sha/sha224-256.c
    src/nuked-sc55/common/rom_loader.cpp

    src/nuked_sc55.cpp
    src/plugin.cpp
)

set_target_properties(Nuked-SC55-CLAP PROPERTIES OUTPUT_NAME Nuked-SC55)

set_property(
    TARGET Nuked-SC55-CLAP
    APPEND
    PROPERTY ADDITIONAL_CLEAN_FILES ${CMAKE_BINARY_DIR}/Nuked-SC55.clap
)

# Get external dependencies
if (CMAKE_TOOLCHAIN_FILE MATCHES ".*vcpkg\.cmake$")
    # Build using vcpkg
    find_package(SpeexDSP REQUIRED)
    target_link_libraries(Nuked-SC55-CLAP PRIVATE Speex::SpeexDSP)
else()
    # Build using system libraries
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SPEEXDSP REQUIRED IMPORTED_TARGET speexdsp)
    target_link_libraries(Nuked-SC55-CLAP PRIVATE PkgConfig::SPEEXDSP)
endif()

#----------------------------------------------------------------------------
# Windows
#----------------------------------------------------------------------------
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    #target_link_options(Nuked-SC55-CLAP PRIVATE /EXPORT:clap_entry)

    target_sources(Nuked-SC55-CLAP PRIVATE src/nuked_sc55_win32.manifest)
    set_target_properties(Nuked-SC55-CLAP PROPERTIES
        SUFFIX ".clap"
        PDB_NAME Nuked-SC55-CLAP)

    #add_compile_options(
    #    /FI ${CMAKE_SOURCE_DIR}/example/config.h
    #)

#----------------------------------------------------------------------------
# macOS
#----------------------------------------------------------------------------
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set_target_properties(Nuked-SC55-CLAP PROPERTIES
        BUNDLE ON
        BUNDLE_EXTENSION clap

        CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Nuked-SC55.clap

        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/resources/macos/plugin.plist.in

        MACOSX_BUNDLE_GUI_IDENTIFIER        net.johnnovak.nuked_sc55_clap
        MACOSX_BUNDLE_BUNDLE_NAME           "Nuked-SC55-CLAP"
        MACOSX_BUNDLE_BUNDLE_VERSION        "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING  "${PROJECT_VERSION}"
        MACOSX_BUNDLE_LONG_VERSION_STRING   "${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT             "John Novak"
    )

    file(TOUCH_NOCREATE ${CMAKE_BINARY_DIR}/Nuked-SC55.clap/Contents/PkgInfo)
    file(WRITE          ${CMAKE_BINARY_DIR}/Nuked-SC55.clap/Contents/PkgInfo "BNDL????")

    add_custom_command(TARGET Nuked-SC55-CLAP POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Installing ${CMAKE_BINARY_DIR}/Nuked-SC55.clap to ~/Library/Audio/Plug-Ins/CLAP/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_BINARY_DIR}/Nuked-SC55.clap" "~/Library/Audio/Plug-Ins/CLAP/Nuked-SC55.clap"
    )

#----------------------------------------------------------------------------
# Linux
#----------------------------------------------------------------------------
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(Nuked-SC55-CLAP PRIVATE -Wl,--version-script=${CMAKE_SOURCE_DIR}/resources/linux/plugin.version)
    target_link_libraries(Nuked-SC55-CLAP PRIVATE -Wl,-z,defs)

    set_target_properties(Nuked-SC55-CLAP PROPERTIES
        SUFFIX ".clap"
        PREFIX "")

    # strip debug info
    #set(CMAKE_EXE_LINKER_FLAGS "-s")
endif ()

