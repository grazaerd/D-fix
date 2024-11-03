cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(lightscanner)

add_library(lightscanner STATIC
            CpuInfo.hpp
            LightningScanner.hpp
            Pattern.hpp
            ScanMode.hpp
            ScanResult.hpp
            allocator/AlignedAllocator.hpp
            src/Cpuinfo.cpp
            src/backends/Avx2.cpp
            src/backends/Scalar.cpp
            src/backends/Sse42.cpp
            backends/Avx2.hpp
            backends/Scalar.hpp
            backends/Sse42.hpp)
set(lsinclude "${CMAKE_CURRENT_SOURCE_DIR}")

target_include_directories(lightscanner SYSTEM PRIVATE ${lsinclude})
target_include_directories(lightscanner INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")