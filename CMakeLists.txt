cmake_minimum_required(VERSION 3.5)
set (CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR} CACHE PATH "")

if (POLICY CMD0146)
  cmake_policy(SET CMP0146 OLD)
endif()

set(PROJNAME shapes)
Project(${PROJNAME})
Message(STATUS "-------------------------------")
Message(STATUS "Processing Project ${PROJNAME}:")

#####################################################################################
# LIBMIN Bootstrap
#
get_filename_component ( LIBMIN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../libmin/" REALPATH )
list( APPEND CMAKE_MODULE_PATH "${LIBMIN_ROOT}/cmake" )
list( APPEND CMAKE_PREFIX_PATH "${LIBMIN_ROOT}/cmake" )

#####################################################################################
# Include LIBMIN
#
find_package(Libmin QUIET)

if (NOT LIBMIN_FOUND)

  Message ( FATAL_ERROR "
  This project requires libmin. 
  Set LIBMIN_ROOT to the libmin repository path for /libmin/cmake.
  " )

else()
  add_definitions(-DUSE_LIBMIN)  
  include_directories(${LIBMIN_INC_DIR})
  include_directories(${LIBRARIES_INC_DIR})  

  if (DEFINED ${BUILD_LIBMIN_STATIC})
    add_definitions(-DLIBMIN_STATIC) 
    file(GLOB LIBMIN_SRC "${LIBMIN_SRC_DIR}/*.cpp" )
    file(GLOB LIBMIN_INC "${LIBMIN_INC_DIR}/*.h" )
    LIST( APPEND LIBMIN_SOURCE_FILES ${LIBMIN_SRC} ${LIBMIN_INC} )
    message ( STATUS "  ---> Using LIBMIN (static)")
  else()    
    LIST( APPEND LIBRARIES_OPTIMIZED "${LIBMIN_REL}")
    LIST( APPEND LIBRARIES_DEBUG "${LIBMIN_DEBUG}")
    _EXPANDLIST( OUTPUT PACKAGE_DLLS SOURCE "" FILES ${LIBMIN_DLLS} )
    message ( STATUS "  Getting Libmin from: ${LIBMIN_DLLS} ")
    message ( STATUS "  ---> Using LIBMIN")
  endif() 
endif()



#####################################################################################
# Options

_REQUIRE_LIBEXT()

_REQUIRE_MAIN()

_REQUIRE_GL ()

_REQUIRE_GLEW ()

_REQUIRE_CUDA (FALSE "src/kernels")

OPTION ( BUILD_JPG "Build with JPG" OFF )
if (BUILD_JPG)
  _REQUIRE_JPG ()
endif()

OPTION (BUILD_GLTF "Build with TinyGLTF" ON)
if (BUILD_GLTF)  
  _REQUIRE_GLTF ()
  add_definitions(-DBUILD_GLTF) 
endif ()

OPTION (BUILD_OPTIX "Build with Optix" OFF)
set ( OPTIX_VERSION "6.5.0" CACHE STRING "OptiX Version" )

OPTION (BUILD_USD "Build with Pixar USD" OFF)

#-- add_definitions ( -DUSE_WIDGETS)

#--- dpi awareness
if ( WIN32 )
  add_definitions ( -DUSE_DPI_AWARE)
  LIST( APPEND LIBRARIES_DEBUG "shcore.lib")	
  LIST( APPEND LIBRARIES_OPTIMIZED "shcore.lib")    
endif()

#--- symbols in release mode
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF" CACHE STRING "" FORCE)


####################################################################################
# Find LASZip 
#
find_package(Laszip QUIET)

if ( LASZIP_FOUND )	
	if ( NOT DEFINED USE_LAS )	       
			SET(USE_LAS ON CACHE BOOL "Use LAS")
	endif()
	if ( USE_LAS )
		add_definitions(-DUSE_LAS)
		add_definitions(-DUSE_LASZIP)		
		message ( STATUS " Using LasZip")
		include_directories ( ${LASZIP_INCLUDE_DIR} )
		link_directories ( ${LASZIP_LIB_DIR} )
		LIST( APPEND LIBRARIES_DEBUG "${LASZIP_DEBUG_LIB}")	
		LIST( APPEND LIBRARIES_OPTIMIZED "${LASZIP_RELEASE_LIB}")    
		_EXPANDLIST( OUTPUT PACKAGE_DLLS SOURCE ${LASZIP_LIB_DIR} FILES ${LASZIP_DLL})
	endif()
else()
    message ( "Warning: Laszip not found/enabled.")
endif()

################################################################################
# Find PixarUSD Package
#
if (BUILD_USD)
  find_package(USD QUIET)

  if (USD_FOUND)
      
     # Python used with USD 
	 if (BUILD_PYTHON)
       if ( NOT DEFINED PYTHON_DIR )
         set (PYTHON_DIR "c:\\Program Files\\Python38" CACHE STRING "Python 3.8.10 path")
	   endif()
	   include_directories( "${PYTHON_DIR}/include" )
	   LIST( APPEND LIBRARIES_RELEASE "${PYTHON_DIR}/libs/python38.lib" )
	   LIST( APPEND LIBRARIES_DEBUG "${PYTHON_DIR}/libs/python38_d.lib" )
	   _EXPANDLIST( OUTPUT PACKAGE_DLLS SOURCE ${PYTHON_DIR} FILES "python38.dll" )
     endif()

     # Pixar USD
     add_definitions(-DUSE_USD)	
     add_definitions(-DBUILD_USD)	
     include_directories( ${USD_INC_DIR} )  
	 include_directories( "${USD_INC_DIR}/boost-1_70" )
	 
	 LIST( APPEND LIBRARIES_DEBUG "${USD_LIB_DIR}/tbb_debug.lib")	
	 LIST( APPEND LIBRARIES_DEBUG "${USD_LIB_DIR}/usd_usd.lib")	
	 LIST( APPEND LIBRARIES_DEBUG "${USD_LIB_DIR}/usd_arch.lib")
	 LIST( APPEND LIBRARIES_DEBUG "${USD_LIB_DIR}/usd_tf.lib")
	 LIST( APPEND LIBRARIES_DEBUG "${USD_LIB_DIR}/usd_sdf.lib")
	 LIST( APPEND LIBRARIES_DEBUG "${USD_LIB_DIR}/usd_ar.lib")
	 LIST( APPEND LIBRARIES_DEBUG "${USD_LIB_DIR}/usd_usdGeom.lib")
	 LIST( APPEND LIBRARIES_DEBUG "${USD_LIB_DIR}/usd_plug.lib")
	 LIST( APPEND LIBRARIES_DEBUG "${USD_LIB_DIR}/usd_gf.lib")
	 LIST( APPEND LIBRARIES_DEBUG "${USD_LIB_DIR}/usd_vt.lib")
     
	
	 # DO NOT copy the USD DLLs into the output folder because USD uses its 
	 # own run-time plugin loader which must access DLLs via the installed USD folder tree
         # _EXPANDLIST( OUTPUT PACKAGE_DLLS SOURCE ${USD_LIB_DIR} FILES ${USD_DLLS} )	 
	 
	 # Instead, set the system PATH env to append the USD folder "d:\codes\usd_22.11\lib".
	 # And restart Visual Studio if necessary to reload the env PATH.

	 # Copy tbb to output folder
	 _EXPANDLIST( OUTPUT PACKAGE_DLLS SOURCE "${USD_ROOT_DIR}/bin" FILES "tbb.dll;tbb_debug.dll;tbbmalloc.dll;tbbmalloc_debug.dll;zlibd.dll" )

     message ( STATUS "--> Using Pixar USD")
  else()
    message ( "Warning: USD Library not found/enabled.")
  endif()
endif()

################################################################################
# Find OptiX Package
#
if (BUILD_OPTIX)
  find_package(LibOptix)

  find_package(Optix)

  if (LIBOPTIX_FOUND AND OPTIX_FOUND)	   
        
        #-- lib_ptix 
		include_directories(${LIBOPTIX_INC})        
		LIST( APPEND LIBRARIES_OPTIMIZED "${LIBOPTIX_LIB}/${LIBOPTIX_REL}")
		LIST( APPEND LIBRARIES_DEBUG "${LIBOPTIX_LIB}/${LIBOPTIX_DEBUG}")	
		_EXPANDLIST( OUTPUT PACKAGE_DLLS SOURCE ${LIBOPTIX_LIB} FILES ${LIBOPTIX_DLLS} )		
		set ( PACKAGE_PTX ${LIBOPTIX_PTX} )	  #-- paths already resolved, dont need _EXPANDLIST
		message ( STATUS "--> Using LIBOPTIX")
		
		#-- Optix itself
		set ( REQUIRE_OPTIX "1" )		     # Load OptiX helper files (\libraries\helpers\optix\)
		add_definitions(-DOPTIX_DENOISE )
		add_definitions(-DUSE_OPTIX)			
		add_definitions(-DBUILD_OPTIX)	
		include_directories( ${OPTIX_INCLUDE_DIR} )
		LIST(APPEND LIBRARIES_OPTIMIZED "${OPTIX_LIB_DIR}/${OPTIX_LIB1}" )
		LIST(APPEND LIBRARIES_OPTIMIZED "${OPTIX_LIB_DIR}/${OPTIX_LIB2}" )
		LIST(APPEND LIBRARIES_OPTIMIZED "${OPTIX_LIB_DIR}/${OPTIX_LIB3}" )
		LIST(APPEND LIBRARIES_DEBUG "${OPTIX_LIB_DIR}/${OPTIX_LIB1}" )
		LIST(APPEND LIBRARIES_DEBUG "${OPTIX_LIB_DIR}/${OPTIX_LIB2}" )
		LIST(APPEND LIBRARIES_DEBUG "${OPTIX_LIB_DIR}/${OPTIX_LIB3}" )

		_EXPANDLIST( OUTPUT PACKAGE_DLLS SOURCE ${OPTIX_BIN_DIR} FILES ${OPTIX_DLLS} )

		#-- Additional PTX files
		# _COMPILEPTX ( SOURCES ${UTIL_OPTIX_KERNELS} TARGET_PATH ${EXECUTABLE_OUTPUT_PATH} GENERATED UTIL_OPTIX_PTX GENPATHS UTIL_OPTIX_PTX_PATHS INCLUDE "${OPTIX_ROOT_DIR}/include/,${GVDB_INCLUDE_DIR},${CMAKE_CURRENT_SOURCE_DIR}" OPTIONS -arch=${CUDA_ARCH_BIN} -code=${CUDA_ARCH_PTX} --ptxas-options=-v -O3 --use_fast_math --maxrregcount=128 )			
	
  else()
	Message(STATUS "  --> Warning: OptiX not found.")
	SET(BUILD_OPTIX OFF CACHE BOOL "Build with Optix" FORCE)
  endif()
endif()



#####################################################################################
# Find GVDB
#
if ( NOT DEFINED BUILD_GVDB )	       
   SET(BUILD_GVDB OFF CACHE BOOL "Build with GVDB")
endif()

if ( BUILD_GVDB )
   find_package(GVDB QUIET)

   if ( GVDB_FOUND )	
	
	  include_directories( ${GVDB_INCLUDE_DIR} )
	  add_definitions(-DUSE_GVDB)
	  add_definitions(-DSIM_CODE)
	  LIST(APPEND LIBRARIES_OPTIMIZED ${GVDB_REL} )
	  LIST(APPEND LIBRARIES_DEBUG ${GVDB_DEBUG} )	
	  LIST(APPEND PACKAGE_SOURCE_FILES ${GVDB_INCLUDE_DIR}/${GVDB_HEADERS} )  	
	  _EXPANDLIST( OUTPUT PACKAGE_DLLS SOURCE "" FILES ${GVDB_DLLS} )
   else()
          message ( FATAL_ERROR "Error: GVDB Library not found.")
   endif()
else()
   message ( "Warning: GVDB Library not enabled.")
endif()


#####################################################################################
# Data path
#
# *NOTE* CMAKE_CURRENT_SOURCE_DIR is the original repository location of assets
# 
if ( NOT DEFINED DATA_PATH ) 
  get_filename_component ( _data "${CMAKE_CURRENT_SOURCE_DIR}/data" REALPATH )
  set ( DATA_PATH ${_data} CACHE PATH "Full path to /data" )  
endif()
message ( STATUS "Using DATA_PATH: ${DATA_PATH}")
add_definitions ( -DDATA_PATH="${DATA_PATH}/" )

# Data Files - all files in DATA_PATH except for .obj
file( GLOB _data_files "${DATA_PATH}/*.*" )
set(DATA_FILES "")
foreach(file ${_data_files})
    if(NOT file MATCHES "\\.obj$")
        list(APPEND DATA_FILES "${file}")
    endif()
endforeach()

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
file(GLOB ASSET_FILES src/assets/*.cpp src/assets/*.h)
file(GLOB BEHAVIOR_FILES src/behaviors/*.cpp src/behaviors/*.h)

include_directories ( src/core )
include_directories ( src/kernels )
include_directories ( src/assets )
include_directories ( src/behaviors )
include_directories ( src/render )

unset ( ALL_SOURCE_FILES )

#-- specific to this app
list( APPEND ALL_SOURCE_FILES ${MAIN_FILES} )
list( APPEND ALL_SOURCE_FILES ${CORE_FILES} )
list( APPEND ALL_SOURCE_FILES ${RENDER_FILES} )
list( APPEND ALL_SOURCE_FILES ${ASSET_FILES} )
list( APPEND ALL_SOURCE_FILES ${BEHAVIOR_FILES} )

#-- generic codes
list( APPEND ALL_SOURCE_FILES ${COMMON_SOURCE_FILES} )
list( APPEND ALL_SOURCE_FILES ${PACKAGE_SOURCE_FILES} )
list( APPEND ALL_SOURCE_FILES ${UTIL_SOURCE_FILES} )
list( APPEND ALL_SOURCE_FILES ${UTIL_OPTIX_FILES} )
list( APPEND ALL_SOURCE_FILES ${UTIL_OPTIX_KERNELS} )

list( APPEND ALL_SOURCE_FILES ${CUDA_FILES} )
list( APPEND ALL_SOURCE_FILES ${PTX_SOURCES} )

if ( NOT DEFINED WIN32 )
  find_library(NVTOOLSEXT nvToolsExt HINTS ${CUDA_TOOLKIT_ROOT_DIR}/lib64)
  find_library(CUDART cudart HINTS ${CUDA_TOOLKIT_ROOT_DIR}/lib64)
  set(libdeps GL GLEW X11 cuda ${NVTOOLSEXT} ${CUDART})
  LIST(APPEND LIBRARIES_OPTIMIZED ${libdeps})
  LIST(APPEND LIBRARIES_DEBUG ${libdeps})
ENDIF()

include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")    
add_definitions(-DGVDB_IMPORTS )  
add_definitions(-DASSET_PATH="${ASSET_PATH}/")
add_definitions(-DSHADER_PATH="${SHADER_PATH}/")

add_executable (${PROJNAME} ${ALL_SOURCE_FILES} ${CUDA_FILES} ${PTX_SOURCES} ${DATA_FILES} ${SHADER_FILES} ) 

if (BUILD_CUDA)
   target_link_libraries( ${PROJNAME} general CUDA::cuda_driver)
endif()

# target_compile_options (${PROJNAME} PRIVATE -Wno-multichar)


#--- debug and release exe
set ( CMAKE_DEBUG_POSTFIX "d" CACHE STRING "" )
set_target_properties( ${PROJNAME} PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

#####################################################################################
# Additional Libraries
#
_LINK ( PROJECT ${PROJNAME} OPT ${LIBRARIES_OPTIMIZED} DEBUG ${LIBRARIES_DEBUG} PLATFORM ${PLATFORM_LIBRARIES} )

#####################################################################################
# Windows specific
#
_MSVC_PROPERTIES()
source_group(Data FILES ${DATA_FILES})
source_group(Core FILES ${CORE_FILES})
source_group(CUDA FILES ${CUDA_FILES})
source_group(Render FILES ${RENDER_FILES})
source_group(Behaviors FILES ${BEHAVIOR_FILES})
source_group(Assets FILES ${ASSET_FILES})
source_group(Shaders FILES ${SHADER_FILES})
source_group(Source FILES ${MAIN_FILES} ${PACKAGE_SOURCE_FILES} ${COMMON_SOURCE_FILES})

source_group(OptiX FILES ${UTIL_OPTIX_FILES} ${UTIL_OPTIX_KERNELS} )
source_group(CUDA FILES ${CUDA_FILES} )
source_group(PTX FILES 	${PTX_FILES} )

#####################################################################################
# Install Binaries
#
#
_DEFAULT_INSTALL_PATH()

# assets & shaders are not copied. they are used from original repo paths
# _COPY_AT_BUILD ( FOLDER ${SRC_ASSET_PATH} DESTINATION ${ASSET_PATH} )			# assets 
# _COPY_AT_BUILD ( FOLDER ${SRC_SHADER_PATH} DESTINATION ${SHADER_PATH} )		# shaders

_INSTALL_PTX ( FILES ${PACKAGE_PTX} DESTINATION ${CMAKE_INSTALL_PREFIX} OUTPUT INSTALL_LIST )	 # optix PTX
_INSTALL_PTX ( FILES ${CUDA_PTX_PATHS} DESTINATION ${CMAKE_INSTALL_PREFIX} OUTPUT INSTALL_LIST ) # shapes PTX

_INSTALL ( FILES ${GLEW_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX} )                             # glew dll
_INSTALL ( FILES ${SHADERS} DESTINATION "${CMAKE_INSTALL_PREFIX}/assets" )					 	 # shaders
_INSTALL ( FILES ${PACKAGE_DLLS} DESTINATION ${CMAKE_INSTALL_PREFIX} )							 # DLLs

if (WIN32)
  install ( FILES $<TARGET_PDB_FILE:${PROJNAME}> DESTINATION ${CMAKE_INSTALL_PREFIX} OPTIONAL )	 # PDB
endif()

install ( FILES ${INSTALL_LIST} DESTINATION ${CMAKE_INSTALL_PREFIX} )							 # exe, pdb

###########################
# Done
message ( STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}" )
message ( STATUS "CMAKE_CURRENT_BINARY_DIR: ${CMAKE_CURRENT_BINARY_DIR}" )
message ( STATUS "DLLS: ${PACKAGE_DLLS}" )
message ( STATUS "------------------------------------")
message ( "SHAPES Install Location: ${CMAKE_INSTALL_PREFIX} ")
message ( STATUS "------------------------------------")



