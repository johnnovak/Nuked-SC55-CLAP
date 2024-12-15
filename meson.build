project(
    'Nuked-SC55-CLAP',
    'cpp',
    version: '0.7.0',
    default_options: [
        'cpp_std=c++20',
        'cpp_visibility=hidden',
        'buildtype=release',
        'b_lto=true',
        'b_ndebug=if-release',
        'b_asneeded=true',
    ],
)

sources = files(
    'src/nuked-sc55/emu.cpp',
    'src/nuked-sc55/lcd.cpp',
    'src/nuked-sc55/mcu.cpp',
    'src/nuked-sc55/mcu_interrupt.cpp',
    'src/nuked-sc55/mcu_opcodes.cpp',
    'src/nuked-sc55/mcu_timer.cpp',
    'src/nuked-sc55/pcm.cpp',
    'src/nuked-sc55/submcu.cpp',
    'src/nuked_sc55.cpp',
    'src/plugin.cpp',
)

speexdsp_dep = dependency(
    'speexdsp',
    fallback: 'speexdsp:speexdsp_dep',
    default_options: ['default_library=static'],
)

shared_library(
    'Nuked-SC55',
    sources,
    include_directories: 'include',
    dependencies: speexdsp_dep,
    name_prefix: '',
    name_suffix: 'clap',
)
