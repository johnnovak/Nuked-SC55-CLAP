cmake_minimum_required(VERSION 3.29)

project(NukedSc55Clap
    VERSION     0.1.0
    DESCRIPTION "Nuked SC-55 CLAP audio plugin"
    LANGUAGES   CXX
)

set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules/")


if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # TODO

else ()
    add_compile_options(-g -Wall -Wextra -Wno-unused-parameter -O3)
endif ()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# TODO
#configure_file(config.h.in config.h)

add_library(NukedSc55Clap MODULE
    src/nuked-sc55/emu.cpp
    src/nuked-sc55/lcd.cpp
    src/nuked-sc55/mcu.cpp
    src/nuked-sc55/mcu_interrupt.cpp
    src/nuked-sc55/mcu_opcodes.cpp
    src/nuked-sc55/mcu_timer.cpp
    src/nuked-sc55/pcm.cpp
    src/nuked-sc55/submcu.cpp

    src/nuked_sc55.cpp
    src/plugin.cpp
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_options(NukedSc55Clap PRIVATE /EXPORT:clap_entry)
    set_target_properties(NukedSc55Clap PROPERTIES SUFFIX ".clap" PREFIX "")

    #add_compile_options(
    #    /FI ${CMAKE_SOURCE_DIR}/example/config.h
    #)

elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set_target_properties(NukedSc55Clap PROPERTIES
        BUNDLE ON
        BUNDLE_EXTENSION clap

        OUTPUT_NAME NukedSC55

        CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/NukedSC55.clap

        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin.plist.in

        MACOSX_BUNDLE_GUI_IDENTIFIER        net.johnnovak.nuked_sc55
        MACOSX_BUNDLE_BUNDLE_NAME           "Nuked SC-55"
        MACOSX_BUNDLE_BUNDLE_VERSION        "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING  "${PROJECT_VERSION}"
        MACOSX_BUNDLE_LONG_VERSION_STRING   "${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT             "John Novak"
    )

    file(TOUCH_NOCREATE ${CMAKE_BINARY_DIR}/NukedSC55.clap/Contents/PkgInfo)
    file(WRITE          ${CMAKE_BINARY_DIR}/NukedSC55.clap/Contents/PkgInfo "BNDL????")

    add_custom_command(TARGET NukedSc55Clap POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Installing ${CMAKE_BINARY_DIR}/NukedSC55.clap to ~/Library/Audio/Plug-Ins/CLAP/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_BINARY_DIR}/NukedSC55.clap" "~/Library/Audio/Plug-Ins/CLAP/NukedSC55.clap"
    )

elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(NukedSc55Clap PRIVATE -Wl,--version-script=${CMAKE_SOURCE_DIR}/resources/linux/plug.version)
    target_link_libraries(NukedSc55Clap PRIVATE -Wl,-z,defs)
    set_target_properties(NukedSc55Clap PROPERTIES SUFFIX ".clap" PREFIX "")

endif ()


find_package(SpeexDSP REQUIRED)

target_link_libraries(NukedSc55Clap  PRIVATE Speex::SpeexDSP)
