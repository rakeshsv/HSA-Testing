################################################################################
##
## Copyright (c) 2018 2018 ROCm Developer Tools
##
## MIT LICENSE:
## Permission is hereby granted, free of charge, to any person obtaining a copy of
## this software and associated documentation files (the "Software"), to deal in
## the Software without restriction, including without limitation the rights to
## use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
## of the Software, and to permit persons to whom the Software is furnished to do
## so, subject to the following conditions:
##
## The above copyright notice and this permission notice shall be included in all
## copies or substantial portions of the Software.
##
## THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
## IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
## FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
## AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
## LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
## OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
## SOFTWARE.
##
################################################################################

cmake_minimum_required ( VERSION 3.5.0 )
if ( ${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
  message(FATAL "In-source build is not allowed")
endif ()
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set ( RVS "amat" )
set ( RVS_PACKAGE "amat" )
set ( RVS_COMPONENT "lib${RVS}" )
set ( RVS_TARGET "${RVS}" )

project ( ${RVS_TARGET} )

message(STATUS "MODULE: ${RVS}")

add_compile_options(-std=c++11)
add_compile_options(-pthread)
add_compile_options(-Wall)
add_compile_options(-DRVS_OS_TYPE_NUM=${RVS_OS_TYPE_NUM})

## Set default module path if not already set
if ( NOT DEFINED CMAKE_MODULE_PATH )
    set ( CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/" )
endif ()

set(ROCM_PATH "/opt/rocm" CACHE STRING "ROCM install path")

set(ROCR_INC_DIR "${ROCM_PATH}/include" CACHE PATH "Contains header files exported by ROC Runtime")

## define include directories
include_directories(./ ../ pci ${ROCR_INC_DIR})


## define lib directories
link_directories(${CMAKE_CURRENT_BINARY_DIR} ${RVS_LIB_DIR})

## define source files
set(SOURCES
  src/amat_init.cpp
)

## define target
add_executable(${RVS_TARGET} src/amat_init.cpp)
target_link_libraries(${RVS_TARGET} /opt/rocm/lib/libhsakmt.so /opt/rocm/lib/libhsa-runtime64.so)


