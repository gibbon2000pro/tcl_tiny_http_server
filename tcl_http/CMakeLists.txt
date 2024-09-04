project(tcl_http)

set(CMAKE_BUILD_TYPE "Debug")

find_package(spdlog REQUIRED)

add_library(tclhttp SHARED
    mongoose.c
    tcl_http.cpp
)
target_link_libraries(tclhttp
    tcl
    spdlog::spdlog
)

