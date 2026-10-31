# ConfigureIntelMKL.cmake
# Reusable function to detect and configure Intel MKL as an INTERFACE library.
#
# Usage:
#   include(ConfigureIntelMKL)
#   configure_intel_mkl(PHU_MKL)
#
# After the call the following cache/parent-scope variables are set:
#   MKL_FOUND     - TRUE if MKL headers and static libs were located
#   USE_INTEL_MKL - set to OFF (in parent scope) when MKL is requested but not found
#   MKLROOT       - updated cache PATH to the resolved root (when found)
#
# An INTERFACE library named TARGET_NAME is created only when MKL is found.

function(configure_intel_mkl TARGET_NAME)
    message(STATUS "Searching for Intel MKL...")

    # Resolve root: cmake cache > environment > NuGet auto-detection (Windows)
    set(_mkl_root "${MKLROOT}")
    if(NOT _mkl_root)
        set(_mkl_root "$ENV{MKLROOT}")
    endif()

    if(NOT _mkl_root AND WIN32 AND DEFINED ENV{USERPROFILE})
        file(GLOB _nuget_mkl_roots "$ENV{USERPROFILE}/.nuget/packages/intelmkl.static.win-x64/*/build/native")
        list(SORT _nuget_mkl_roots COMPARE NATURAL ORDER DESCENDING)
        list(LENGTH _nuget_mkl_roots _nuget_roots_len)
        if(_nuget_roots_len GREATER 0)
            list(GET _nuget_mkl_roots 0 _mkl_root)
            message(STATUS "Using MKL from local NuGet cache: ${_mkl_root}")
        endif()
    endif()

    if(_mkl_root)
        file(TO_CMAKE_PATH "${_mkl_root}" _mkl_root)
    endif()

    # Build platform-specific include/lib paths
    if(WIN32)
        set(_mkl_include_dir "${_mkl_root}/include")
        set(_mkl_lib_dir     "${_mkl_root}/win-x64")
        set(_mkl_required_libs
            "${_mkl_lib_dir}/mkl_intel_lp64.lib"
            "${_mkl_lib_dir}/mkl_sequential.lib"
            "${_mkl_lib_dir}/mkl_core.lib"
        )
    else()
        set(_mkl_include_dir "${_mkl_root}/include")
        set(_mkl_lib_dir     "${_mkl_root}/lib/intel64")
        set(_mkl_required_libs
            "${_mkl_lib_dir}/libmkl_intel_lp64.a"
            "${_mkl_lib_dir}/libmkl_sequential.a"
            "${_mkl_lib_dir}/libmkl_core.a"
        )
    endif()

    # Verify all artefacts exist
    set(_mkl_all_present TRUE)
    if(NOT EXISTS "${_mkl_include_dir}/mkl.h")
        set(_mkl_all_present FALSE)
    endif()
    foreach(_mkl_lib IN LISTS _mkl_required_libs)
        if(NOT EXISTS "${_mkl_lib}")
            set(_mkl_all_present FALSE)
        endif()
    endforeach()

    if(_mkl_all_present)
        add_library(${TARGET_NAME} INTERFACE)
        target_include_directories(${TARGET_NAME} INTERFACE "${_mkl_include_dir}")
        target_link_libraries(${TARGET_NAME} INTERFACE ${_mkl_required_libs})

        set(MKL_FOUND TRUE PARENT_SCOPE)
        set(MKLROOT "${_mkl_root}" CACHE PATH "Path to MKL root (NuGet build/native or oneAPI mkl root)" FORCE)
        message(STATUS "Intel MKL found at: ${_mkl_root}")
        message(STATUS "Intel MKL static libs: ${_mkl_required_libs}")
    else()
        set(MKL_FOUND FALSE PARENT_SCOPE)
        set(USE_INTEL_MKL OFF PARENT_SCOPE)
        message(STATUS "Intel MKL not found or incomplete (set MKLROOT to enable). FFT will use JUCE fallback.")
    endif()
endfunction()
