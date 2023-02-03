#! /usr/bin/tclsh

source ./tcl_http_server.tcl

set server [HttpServer new 8888]
$server set_handler "*" {{conn method uri query headers body} {
    puts "[now] $uri"
    reply_file $conn {resume/贺渊凌1.pdf}
}} {
    proc now {} {
        return "\033\[33m[clock format [clock seconds] -format {%Y-%m-%d %H:%M:%S}]\033\[0m"
    }
} 1 4 30
$server start

vwait forever