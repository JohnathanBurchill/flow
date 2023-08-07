# flow: CMakeLists.txt

# Copyright (C) 2023  Johnathan K Burchill

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.0)

project(flow)

# list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake")
# See https://github.com/aminosbh/sdl2-cmake-modules
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMake/sdl2)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    message( "-- Debugging enabled")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -std=gnu99 ")
else ()
    message( "-- Optimization enabled")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -std=gnu99 ")
endif()

# requires -lm on linux
find_library(MATH m REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_gfx REQUIRED)
find_package(SDL2_ttf REQUIRED)

# LIB AVUTIL, from https://newbedev.com/cmake-configuration-for-ffmpeg-in-c-project
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
    libavformat
    libavfilter
    libavcodec
    libswresample
    libswscale
    libavutil
)

set(LIBS ${LIBS} ${MATH} ${LIBXML2_LIBRARIES} ${ZLIB_LIBRARIES} SDL2::Main SDL2::GFX SDL2::Image SDL2::TTF PkgConfig::LIBAV)

#set(LIBS ${LIBS} ${MATH} ${LIBXML2_LIBRARIES} ${ZLIB_LIBRARIES} SDL2::Main SDL2::GFX SDL2::Image SDL2_ttf PkgConfig::LIBAV)
include_directories(${INCLUDE_DIRS} ${LIBXML2_INCLUDE_DIR})

add_executable(flow flow.c midi.c video.c audio.c colour.c physics.c options.c)
target_link_libraries(flow ${LIBS})

install(TARGETS flow DESTINATION $ENV{HOME}/bin)


