cmake_minimum_required(VERSION 3.24)

project(DistributedCacheDirectory
  VERSION 1.0.0
  DESCRIPTION "Distributed location/replica directory for reusable AI state"
  HOMEPAGE_URL "https://github.com/pngen/Distributed-Cache-Directory"
  LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(DCD_BUILD_TESTS "Build Distributed Cache Directory tests" ON)
option(DCD_BUILD_EXAMPLES "Build Distributed Cache Directory examples" ON)
option(DCD_BUILD_BENCHMARKS "Build Distributed Cache Directory benchmarks" OFF)
option(DCD_ENABLE_SYNTHETIC_DISTRIBUTED "Enable synthetic distributed scenarios" ON)
option(DCD_ENABLE_CUDA_PROOF "Enable CUDA location proof (opt-in)" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(distributedcachedirectory STATIC
  src/directory.cpp
  src/persistence.cpp
  src/protocol.cpp
  src/transport.cpp
  src/client.cpp
  src/server.cpp
)

add_library(DistributedCacheDirectory::distributedcachedirectory ALIAS distributedcachedirectory)

target_include_directories(distributedcachedirectory
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX /permissive- /utf-8")
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
endif()

if(WIN32)
  target_link_libraries(distributedcachedirectory PUBLIC ws2_32)
endif()

set_target_properties(distributedcachedirectory PROPERTIES
  EXPORT_NAME distributedcachedirectory
  POSITION_INDEPENDENT_CODE ON)

if(DCD_ENABLE_CUDA_PROOF)
  find_package(CUDAToolkit QUIET)
  if(CUDAToolkit_FOUND)
    enable_language(CUDA)
    message(STATUS "Distributed Cache Directory: CUDA proof enabled (found CUDA Toolkit)")
    add_executable(distributed-cache-directory-cuda-proof cuda/cuda_proof.cu)
    target_include_directories(distributed-cache-directory-cuda-proof PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(distributed-cache-directory-cuda-proof PRIVATE distributedcachedirectory CUDA::cudart)
    set_target_properties(distributed-cache-directory-cuda-proof PROPERTIES CUDA_ARCHITECTURES 120)
    target_compile_options(distributed-cache-directory-cuda-proof PRIVATE
      $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/W4 -Xcompiler=/WX -Wno-deprecated-gpu-targets>)
  else()
    message(STATUS "Distributed Cache Directory: CUDA proof requested but CUDA Toolkit not found; disabling CUDA proof")
  endif()
endif()

install(TARGETS distributedcachedirectory
  EXPORT DistributedCacheDirectoryTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

set(DCD_CMAKE_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/DistributedCacheDirectory)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DistributedCacheDirectoryConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/DistributedCacheDirectoryConfig.cmake"
  INSTALL_DESTINATION ${DCD_CMAKE_INSTALL_DIR})

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/DistributedCacheDirectoryConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(EXPORT DistributedCacheDirectoryTargets
  NAMESPACE DistributedCacheDirectory::
  DESTINATION ${DCD_CMAKE_INSTALL_DIR})

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/DistributedCacheDirectoryConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/DistributedCacheDirectoryConfigVersion.cmake"
  DESTINATION ${DCD_CMAKE_INSTALL_DIR})

if(DCD_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
if(DCD_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
if(DCD_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()

add_subdirectory(tools)

set(CPACK_PACKAGE_NAME "DistributedCacheDirectory")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "Summon Software Labs")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Distributed location/replica directory for reusable AI state")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_GENERATOR "ZIP;TGZ")
include(CPack)
