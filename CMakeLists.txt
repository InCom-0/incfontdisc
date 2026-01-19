cmake_minimum_required(VERSION 3.30...4.2)

project(incfontdisc
  HOMEPAGE_URL "https://github.com/InCom-0/incfontdisc"
  VERSION "0.0.0"
  DESCRIPTION
  "Crossplatform library for discovering, finding and grabbing fonts installed on a system."
  LANGUAGES CXX C)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(incfontdisc_BUILD_SHARED_LIB "Build a shared version of incfontdisc" ${BUILD_SHARED_LIBS})
option(incfontdisc_STAGE_OUTPUTS "Stage build output artifacts into proper directory structure" ON)

option(incfontdisc_BUILD_DEMOS "Build demos for incfontdisc" ${PROJECT_IS_TOP_LEVEL})
option(incfontdisc_BUILD_TESTS "Build test executables for incplot-lib" ${PROJECT_IS_TOP_LEVEL})


#####################################################################
### Staging ###
#####################################################################
include(cmake/StageOutputs.cmake)
if(incfontdisc_STAGE_OUTPUTS)
    stageOutputDirsFor(incfontdisc)
endif()


########################################################
### Target specification ###
########################################################
include(CMake_dependencies.cmake) ### Loads and declares the required external libraries using CPM or CMake's FetchContent.

if(incfontdisc_BUILD_SHARED_LIB)
  add_library(incfontdisc SHARED)
else()
  add_library(incfontdisc STATIC)
endif()
add_library(incfontdisc::incfontdisc ALIAS incfontdisc)

target_sources(incfontdisc PRIVATE src/incfontdisc.cpp)
target_sources(incfontdisc
  PUBLIC
  FILE_SET pub_headers
  TYPE HEADERS
  BASE_DIRS
  include)

target_sources(incfontdisc
  PRIVATE
  FILE_SET priv_headers
  TYPE HEADERS
  BASE_DIRS
  src/private_inc)

# target_link_libraries(incfontdisc PRIVATE ........)

target_compile_features(incfontdisc PRIVATE cxx_std_23)
set_target_properties(incfontdisc PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

if(incfontdisc_BUILD_SHARED_LIB)
  set_target_properties(incfontdisc PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
  )
  target_compile_definitions(incfontdisc
    PRIVATE incfontdisc_EXPORTS
    PUBLIC incfontdisc_SHARED
  )
endif()


########################################################
### Demos specification ###
########################################################
if(incfontdisc_BUILD_DEMOS)
  add_executable(scratch demos/demo_1.cpp)
  target_compile_features(scratch PRIVATE cxx_std_23)
  target_link_libraries(scratch PRIVATE incfontdisc)
  if(USING_LIBSTDCXX)
    target_link_libraries(scratch PRIVATE "-lstdc++exp")
  endif()
endif()


#####################################################################
### Platform specific hacks ###
#####################################################################
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT MATCHES "MSVC")
  add_compile_options(/utf-8)
  add_compile_definitions(NOMINMAX)
endif()


#####################################################################
### Installing, packaging ###
#####################################################################
if(BUILD_SHARED_LIBS)
  install(
    TARGETS incfontdisc
    EXPORT incfontdisc_target
    RUNTIME_DEPENDENCY_SET incfontdisc_depset
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FILE_SET pub_headers
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  # When compling with MinGW we need to include the runtime DLLs along
  if(MINGW)
    get_filename_component(incfontdisc_CXX_COMPILER_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
    install(
      RUNTIME_DEPENDENCY_SET incfontdisc_depset
      PRE_EXCLUDE_REGEXES
      [[api-ms-.*]]
      [[ext-ms-.*]]
      POST_INCLUDE_REGEXES
      [[libstdc\+\+*]]
      [[libgcc_s*]]
      POST_EXCLUDE_REGEXES
      [[\\*.dll]]
      DIRECTORIES
      ${incfontdisc_CXX_COMPILER_DIR}
    )
  endif()

  # Creating a script for being able to use 'find_package')
  install(
    EXPORT incfontdisc_target
    NAMESPACE incfontdisc::
    DESTINATION lib/cmake/incfontdisc
  )

else()
  message(STATUS "Installing incfontdisc is not available because it is being built as STATIC library")
endif(BUILD_SHARED_LIBS)

