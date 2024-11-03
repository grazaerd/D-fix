cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)

project(dfix)
set(CMAKE_C_FLAGS "-O3 -fuse-ld=lld -fno-common -fno-record-gcc-switches -DNDEBUG -static -fomit-frame-pointer -fno-asynchronous-unwind-tables -fno-unwind-tables -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,-s")
set(CMAKE_CXX_FLAGS "-O3 -fuse-ld=lld -lstdc++ -fno-common -fno-record-gcc-switches -DNDEBUG -static -fomit-frame-pointer -fno-unwind-tables -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,-s")

add_library(dfix SHARED
            src/main.cpp
            src/impl.cpp
            src/impl.h
            src/log.h
            src/d3d11.def
            src/util.h
            src/shaders/Default.h
            src/shaders/DiffSpheric.h
            src/shaders/Grass.h
            src/shaders/Particle1.h
            src/shaders/Player.h
            src/shaders/RadialBlur.h
            src/shaders/Shadow.h
            src/shaders/SkyBox.h
            src/shaders/Spherical.h
            src/shaders/Terrain.h
            src/shaders/Tex.h
            src/shaders/VolumeFog.h
            src/shaders/Default.h)

set(minhook "${CMAKE_CURRENT_SOURCE_DIR}/lib/minhook")
set(lightscanner "${CMAKE_CURRENT_SOURCE_DIR}/lib/LightningScanner")

add_subdirectory(${minhook})
add_subdirectory(${lightscanner})
# todo msvc
target_compile_options(dfix PRIVATE -Wall -Wuse-after-free=3 -Wextra -Wshadow -Wnon-virtual-dtor -Wpedantic -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wpedantic -Wconversion -Wsign-conversion -Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wnull-dereference -Wuseless-cast -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough -Weffc++)
# remove warnings for third party libraries
target_compile_options(minhook PRIVATE -w)
target_compile_options(lightscanner PRIVATE -w)

target_include_directories(dfix SYSTEM PRIVATE ${minhook})
target_include_directories(dfix SYSTEM PRIVATE ${lightscanner})

target_link_libraries(dfix PRIVATE minhook)
target_link_libraries(dfix PRIVATE lightscanner)

# rename dll
set_target_properties(dfix PROPERTIES PREFIX "")
set_target_properties(dfix PROPERTIES OUTPUT_NAME "d3d11")
