cmake_minimum_required(VERSION 3.0.0)
project (gl-frame-bender)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG_PIPE_FLOW -D_DEBUG")
add_definitions(-DHAVE_ZLIB -DBOOST_FILESYSTEM_NO_DEPRECATED)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")

	IF(NOT ${CMAKE_GENERATOR} MATCHES "Visual Studio 12 2013 Win64")
		message( FATAL_ERROR "Generator ${CMAKE_GENERATOR} is not supported, you have to use 'Visual Studio 12 2013 Win64'." )
	ENDIF(NOT ${CMAKE_GENERATOR} MATCHES "Visual Studio 12 2013 Win64")

	set(GLM_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/external/glm/include")
	
	set(GLFW_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/external/glfw/include")
	set(GLFW_LIBRARY_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/external/glfw/lib/msvc12_x64")
	set(GLFW_LIBRARIES "opengl32.lib" "glfw3.lib")

	set(DEVIL_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/external/devil/include")
	set(DEVIL_LIBRARY_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/external/devil/lib/msvc_x64")
	set(DEVIL_LIBRARIES "DevIL.lib" "ILU.lib")

	set(Protobuf_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/external/protobuf/include" "${CMAKE_CURRENT_SOURCE_DIR}/external/zlib/include")
	set(Protobuf_LIBRARY_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/external/protobuf/lib/msvc12_x64" "${CMAKE_CURRENT_SOURCE_DIR}/external/zlib/lib/msvc_x64")
	set(Protobuf_LIBRARIES "libprotobuf.lib" "zlibstat.lib")
	set(Protobuf_LIBRARIES_DEBUG "libprotobufd.lib" "zlibstat.lib")

	set(IPP_PLATFORM_LIBRARIES "ippimt.lib" "ippcoremt.lib")

	set(USER_BOOST_ROOT "C:\\local\\boost_1_57_0" CACHE PATH "Root directory of Boost installation")
	set(BOOST_ROOT ${USER_BOOST_ROOT})
	set(BOOST_LIBRARYDIR "${BOOST_ROOT}/lib64-msvc-12.0")

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
	add_definitions(-D_SCL_SECURE_NO_WARNINGS)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	add_definitions(-DNOMINMAX)

ELSE(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	
	find_package(PkgConfig REQUIRED)

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")

	pkg_search_module(GLFW REQUIRED glfw3)

	pkg_search_module(DEVIL REQUIRED IL)

	find_package(Protobuf 2.6.0 REQUIRED)

	set(CMAKE_EXPORT_COMPILE_COMMANDS "ON")

  set(IPP_PLATFORM_LIBRARIES "-lippi")

ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")

set(USE_INTEL_IPP FALSE CACHE BOOL "Use Intel IPP library")
set(IPP_USER_INCLUDE_DIR "" CACHE PATH "Header directoy of Intel IPP library")
set(IPP_USER_LIBRARY_DIR "" CACHE PATH "Library directory of Intel IPP library")

IF(${USE_INTEL_IPP})
  add_definitions(-DFB_USE_INTEL_IPP_FOR_HOST_COPY)
  set(IPP_LIBRARIES ${IPP_PLATFORM_LIBRARIES})
  message("Will link Intel IPP libraries '" ${IPP_LIBRARIES} "', include "
    "directory '" ${IPP_USER_INCLUDE_DIR} "', library directory '" 
    ${IPP_USER_LIBRARY_DIR} "'.")
  set (IPP_INCLUDE_DIRS ${IPP_USER_INCLUDE_DIR})
  set (IPP_LIBRARY_DIRS ${IPP_USER_LIBRARY_DIR})
  include_directories(${IPP_INCLUDE_DIRS})
  link_directories(${IPP_LIBRARY_DIRS})
ELSE(${USE_INTEL_IPP})
  message("Will not use Intel IPP library")
ENDIF(${USE_INTEL_IPP})

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)

find_package(Boost 1.54.0 REQUIRED system filesystem program_options unit_test_framework date_time log thread chrono)
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

include_directories(${GLM_INCLUDE_DIRS})
include_directories(${GLFW_INCLUDE_DIRS})
link_directories(${GLFW_LIBRARY_DIRS})
include_directories(${DEVIL_INCLUDE_DIRS})
link_directories(${DEVIL_LIBRARY_DIRS})
include_directories(${Protobuf_INCLUDE_DIRS})
link_directories(${Protobuf_LIBRARY_DIRS})

file(GLOB ShaderFiles RELATIVE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/glsl/*.glsl
                      ${CMAKE_SOURCE_DIR}/glsl/*.frag
					  ${CMAKE_SOURCE_DIR}/glsl/*.vert)

foreach(ShaderFile ${ShaderFiles})
	list(APPEND ShaderFilesAbsolute "${CMAKE_SOURCE_DIR}/${ShaderFile}")
endforeach()

file(GLOB ResourceFiles RELATIVE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/resources/*)

foreach(ResourceFile ${ResourceFiles})
	list(APPEND ResourceFilesAbsolute "${CMAKE_SOURCE_DIR}/${ResourceFile}")
endforeach()

add_subdirectory(lib)
add_subdirectory(gl-frame-bender)
add_subdirectory(gl-frame-bender-tests)
add_subdirectory(utils)

