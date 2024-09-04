project(tcl_http)

set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
message(WARNING "CMAKE_CXX_FLAGS_DEBUG : ${CMAKE_CXX_FLAGS_DEBUG}")


find_package(spdlog REQUIRED)

add_library(tclhttp SHARED
    mongoose.c
    tcl_http.cpp
)
target_link_libraries(tclhttp
    tcl
    spdlog::spdlog
)

