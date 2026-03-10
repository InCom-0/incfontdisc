#[=======================================================================[.rst:
FindLZO2
--------------

Find lzo2 headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``lzo2::lzo2``
  The liblzo2 library, if found.


Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project.
Note that one typically does not need to use these variables because lzo2::lzo2 is first class CMake target that encapsulates all of them.

``LZO2_FOUND``
  true if (the requested version of) LZO2 is available.
``LZO2_VERSION``
  the version of LZO2.
``LZO2_LIBRARY``
  the libraries to link against to use LZO2.
``LZO2_INCLUDE_DIR``
  where to find the LZ headers.
``LZO2_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(LZO2_PC QUIET IMPORTED_TARGET GLOBAL lzo2)

if(LZO2_PC_FOUND)
    if(NOT TARGET lzo2::lzo2)
        add_library(lzo2::lzo2 ALIAS PkgConfig::LZO2_PC)
    endif()

    set(LZO2_LIBRARY lzo2::lzo2 CACHE STRING "liblzo2 target")
    set(LZO2_COMPILE_OPTIONS ${LZO2_PC_CFLAGS_OTHER} CACHE STRING "liblzo2 target compile options")
    set(LZO2_VERSION ${LZO2_PC_VERSION} CACHE STRING "liblzo2 target version")

    find_path(LZO2_INCLUDE_DIR
        NAMES lzo2.h lzo2a.h
        HINTS ${LZO2_PC_INCLUDEDIR} ${LZO2_PC_INCLUDE_DIRS}
    )
    find_library(LZO2_LIBRARY
        NAMES ${LZO2_NAMES} lzo2
        HINTS ${LZO2_PC_LIBDIR} ${LZO2_PC_LIBRARY_DIRS}
    )

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(
        LZO2
        REQUIRED_VARS LZO2_LIBRARY LZO2_INCLUDE_DIR
        VERSION_VAR LZO2_VERSION
    )
    set(LZO2_LIBRARIES ${LZO2_LIBRARY})
    set(LZO2_INCLUDE_DIRS ${LZO2_INCLUDE_DIR})

else()
    message(STATUS "lzo2: liblzo2 was not found via find_package() via FindLZO2.cmake module via PkgConfig.")
endif()

mark_as_advanced(
    LZO2_LIBRARY
    LZO2_INCLUDE_DIR
)
