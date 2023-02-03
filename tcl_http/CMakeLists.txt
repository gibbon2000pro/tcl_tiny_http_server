project(tcl_http)

add_library(tclhttp SHARED
    mongoose.c
    tcl_http.cpp
)
target_link_libraries(tclhttp tcl)

