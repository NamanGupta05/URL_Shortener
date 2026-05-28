cmake_minimum_required(VERSION 3.16)
project(url_shortener LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(url_shortener main.cpp)

if (WIN32)
  target_link_libraries(url_shortener ws2_32)
endif()

