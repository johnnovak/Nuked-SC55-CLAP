# FindSpeexDSP
# ------------
#
# Find SpeexDSP headers and libraries.
#
# Imported targets:
#
#   - Speex::SpeexSDP -- The SpeexDSP library, if found.
#
# Adapted from: https://github.com/microsoft/vcpkg/issues/37412
#

set(_VCPKG_ARCH_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")

find_path(SPEEXDSP_INCLUDE_DIR
    NAMES speex/speexdsp_types.h
    PATHS "${_VCPKG_ARCH_DIR}/include"
    NO_DEFAULT_PATH
    REQUIRED
)

find_library(SPEEXDSP_LIB_RELEASE
    NAMES speexdsp
    PATHS "${_VCPKG_ARCH_DIR}/lib"
    NO_DEFAULT_PATH
)

find_library(SPEEXDSP_LIB_DEBUG
    NAMES speexdsp
    PATHS "${_VCPKG_ARCH_DIR}/debug/lib"
    NO_DEFAULT_PATH
)

if (NOT SPEEXDSP_LIB_RELEASE AND NOT SPEEXDSP_LIB_DEBUG)
    message(FATAL_ERROR "SpeexDSP library not found")
endif ()

add_library(Speex::SpeexDSP STATIC IMPORTED)

set_target_properties(Speex::SpeexDSP PROPERTIES
    IMPORTED_CONFIGURATIONS       "Debug;Release"
    IMPORTED_LOCATION_RELEASE     "${SPEEXDSP_LIB_RELEASE}"
    IMPORTED_LOCATION_DEBUG       "${SPEEXDSP_LIB_DEBUG}"
    INTERFACE_INCLUDE_DIRECTORIES "${SPEEXDSP_INCLUDE_DIR}"
)
