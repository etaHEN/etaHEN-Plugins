###################################################################################################
# PS5SDK - Example: pipe pirate
# Uses the read/write primitive to read and write some kernel data.
# @authors ChendoChap, Specter, Znullptr, LM for etaHEN SDK
###################################################################################################

cmake_minimum_required (VERSION 3.20)

set(basename "PS5Debug")
project(${basename} C CXX)

set(PLUGIN_TITLE_ID "DEBU00000")
set(PLUGIN_VERSION "1.00")

# Language Standard Defaults
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

# Check for sub-project as part of main build or external build
if (NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	set(IS_SUBPROJECT TRUE)
else()
	set(IS_SUBPROJECT FALSE)
endif()

message("IS_SUBPROJECT: ${IS_SUBPROJECT}")

set(D_CWD "${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${D_CWD}/../bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${D_CWD}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${D_CWD}/bin)	# static libs are archive

add_executable(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}.elf")

# Must build with clang
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "[Cc]lang")
	set(IS_CLANG 1)
else()
	message(FATAL_ERROR "${PROJECT_NAME} is meant to be built with clang! CompilerID: ${CMAKE_CXX_COMPILER_ID}")
endif()

# Finalize main target sources
target_compile_options(${PROJECT_NAME} PUBLIC
	$<$<COMPILE_LANGUAGE:C>:${C_DEFS} ${C_FLAGS}>
	$<$<COMPILE_LANGUAGE:CXX>:${CXX_DEFS} ${CXX_FLAGS}>
	$<$<COMPILE_LANGUAGE:ASM>:${ASM_FLAGS}>
)

message("========== build: ${PROJECT_NAME} ==========")
set(D_SRC ${D_CWD}/source)
file(GLOB SrcFiles ${D_SRC}/*.c ${D_SRC}/*.cpp ${D_SRC}/*.h ${D_SRC}/*.hpp ${D_SRC}/*.s ${D_SRC}/../../../extern/pfd_sfo_tools/sfopatcher/src/*.c ${D_SRC}/../../libelfloader/source/ ${D_SRC}/../../../extern/pfd_sfo_tools/sfopatcher/src/*.h ${D_SRC}/../../../extern/tiny-json/*.c ${D_SRC}/../../../extern/tiny-json/*.h ${D_SRC}/../../../extern/mxml/*.c ${D_SRC}/../../../extern/mxml/*.h)

set(CMAKE_C_FLAGS "--target=x86_64-sie-ps5 -DPPR -DPS5 -DPS5_FW_VERSION=${V_FW} ")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-keyword-macro -fPIC -fPIE -march=znver2 -Wall -Werror -fstack-protector-all")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -DDEBUG -gfull -gdwarf-2 -O0 -pedantic -pedantic-errors")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 ")

add_custom_command(TARGET ${basename}
                   POST_BUILD
                   COMMAND python3 "${CMAKE_SOURCE_DIR}/lib/make_plugin.py" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${basename}.elf" "${PLUGIN_TITLE_ID}" "${PLUGIN_VERSION}"
                   COMMAND ${CMAKE_COMMAND} -E remove "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${basename}.elf"                  
                   COMMENT "Finishing Plugin"
                   WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

target_sources(${PROJECT_NAME} PRIVATE ${SrcFiles})
target_include_directories(${PROJECT_NAME} PRIVATE "${D_CWD}/../include")
target_include_directories(${PROJECT_NAME} PRIVATE "${D_CWD}/include")
target_include_directories(${PROJECT_NAME} PRIVATE "${D_CWD}/../shared/include")
target_link_directories	(${PROJECT_NAME} PUBLIC "${PROJECT_ROOT}/lib")
target_link_libraries	(${PROJECT_NAME} PUBLIC SceLibcInternal kernel_sys SceNet)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
