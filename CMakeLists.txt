cmake_minimum_required(VERSION 2.8...3.5)

set(PROJNAME shapes )
Project(${PROJNAME} LANGUAGES C CXX)

Message(STATUS "-------------------------------")
Message(STATUS "Project: ${PROJNAME}")

#####################################################################################
# LIBMIN Bootstrap
#
set ( LIBMIN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../libmin" CACHE STRING "Location of Libmin")
list( APPEND CMAKE_MODULE_PATH "${LIBMIN_ROOT}/cmake" )
list( APPEND CMAKE_PREFIX_PATH "${LIBMIN_ROOT}/cmake" )

find_package(Libmin)

#####################################################################################
# Options

_REQUIRE_DATAPTR()
_REQUIRE_IMAGE()
_REQUIRE_GXLIB()
_REQUIRE_3D()

_REQUIRE_MAIN()

_REQUIRE_GL()

_REQUIRE_GLEW()

set(CMAKE_CUDA_ARCHITECTURES 89)   # 70=Volta, 75=Turing, 80=Ampere, 86=RTX 3x, 89=RTX 4x, 120=RTX 5x
_REQUIRE_CUDA ( TRUE "source")

#####################################################################################
# Asset Files - all files in ASSET_PATH except for .obj
message (STATUS "  Finding assets from: ${ASSET_PATH}")
file( GLOB _asset_files "${ASSET_PATH}/*.*" )
set(ASSET_FILES "")
foreach(file ${_asset_files})
    if(NOT file MATCHES "\\.obj$")        
        list(APPEND ASSET_FILES "${file}")
    endif()
endforeach()

#####################################################################################
# Shader path
if ( NOT DEFINED SHADER_PATH ) 
  get_filename_component ( _shaders "${CMAKE_CURRENT_SOURCE_DIR}/shaders" REALPATH )
  set ( SHADER_PATH ${_shaders} CACHE PATH "Full path to /shaders" )
endif()
add_definitions( -DSHADER_PATH="${SHADER_PATH}/" )
file( GLOB SHADER_FILES "${SHADER_PATH}/*.*" )

#####################################################################################
# Executable
#
file(GLOB MAIN_FILES *.cpp *.c *.h )
file(GLOB CORE_FILES src/core/*.cpp src/core/*.c src/core/*.h)
file(GLOB RENDER_FILES src/render/*.cpp src/render/*.h)
file(GLOB PRIM_FILES src/prims/*.cpp src/prims/*.h)
file(GLOB BEHAVIOR_FILES src/behaviors/*.cpp src/behaviors/*.h)

include_directories ( src/core )
include_directories ( src/kernels )
include_directories ( src/prims )
include_directories ( src/behaviors )
include_directories ( src/render )

list( APPEND ALL_FILES ${MAIN_FILES} )
list( APPEND ALL_FILES ${CORE_FILES} )
list( APPEND ALL_FILES ${RENDER_FILES} )
list( APPEND ALL_FILES ${PRIM_FILES} )
list( APPEND ALL_FILES ${BEHAVIOR_FILES} )

_GET_GLOBALS()

add_executable (${PROJNAME} ${ALL_FILES} ${CUDA_FILES} ${PACKAGE_SOURCE_FILES} ${LIBMIN_FILES})

target_sources( ${PROJNAME} PRIVATE ${ASSET_FILES})
set_source_files_properties(${ASSET_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
target_sources( ${PROJNAME} PRIVATE ${SHADER_FILES})
set_source_files_properties(${SHADER_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)


_LINK ( PROJECT ${PROJNAME} OPT ${LIBS_OPTIMIZED} DEBUG ${LIBS_DEBUG} PLATFORM ${LIBS_PLATFORM} )

#####################################################################################
# IDE Setup
#
_STARTUP_PROJECT ()
source_group("Source Files" FILES ${MAIN_FILES} ${PACKAGE_SOURCE_FILES})
source_group("CUDA" FILES ${CUDA_FILES} )
source_group("Assets" FILES ${ASSET_FILES})
source_group("Core" FILES ${CORE_FILES})
source_group("Render" FILES ${RENDER_FILES})
source_group("Behaviors" FILES ${BEHAVIOR_FILES})
source_group("Primitives" FILES ${PRIM_FILES})
source_group("Shaders" FILES ${SHADER_FILES})

#####################################################################################
# Install Binaries
#
install_ptx ( FILES ${PTX_FILES} DESTINATION . )     # PTX
install_files ( FILES ${PACKAGE_DLLS} DESTINATION . )    	# DLLs
install ( FILES $<TARGET_PDB_FILE:${PROJNAME}> OPTIONAL DESTINATION .)   	# PDB
install ( FILES ${INSTALL_LIST} DESTINATION .)				# EXE

###########################
# Done
message ( STATUS "  Source Path: ${CMAKE_CURRENT_SOURCE_DIR}" )
message ( STATUS "  Binary Path: ${CMAKE_CURRENT_BINARY_DIR}" )
message ( STATUS "  Libs Optimized:   ${LIBS_OPTIMIZED}" )
message ( STATUS "  Libs Debug:  ${LIBS_DEBUG}" )
message ( STATUS "  Libs Platform:  ${LIBS_PLATFORM}" )
message ( STATUS "  Packaged DLLs:   ${PACKAGE_DLLS}" )
message ( STATUS "  OUTPUT PATH:     ${CMAKE_INSTALL_PREFIX}" )




