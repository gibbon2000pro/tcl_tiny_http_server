project(tcl_http)

set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
message(WARNING "CMAKE_CXX_FLAGS_DEBUG : ${CMAKE_CXX_FLAGS_DEBUG}")

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    set(CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} -Werror -fsanitize=undefined -fsanitize=address"
    )
endif()


find_package(spdlog REQUIRED)

add_library(tclhttp SHARED
    mongoose.c
    tcl_http.cpp
)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    target_link_options(tclhttp
        BEFORE PUBLIC -fsanitize=undefined PUBLIC -fsanitize=address
    )
endif()

target_link_libraries(tclhttp
    /usr/lib/gcc/x86_64-linux-gnu/11/libasan.a
    tcl
    spdlog::spdlog
)

