#! /usr/bin/tclsh

source ./tcl_http_server.tcl

set server [HttpServer new 8888]
$server set_handler "/gibbon" {{conn method uri query headers body} {
    reply_file $conn {resume/贺渊凌1.pdf}
}} {} 1 3 30

$server set_handler "/rita" {{conn method uri query headers body} {
    reply_file $conn {resume/阳娟个人简历1.16.pdf}
}} {} 1 3 30

$server set_handler "*" {{conn method uri query headers body} {
    puts "[now] $uri"
    reply $conn 404 {} {WTF???}
}} {
    proc now {} {
        return "\033\[33m[clock format [clock seconds] -format {%Y-%m-%d %H:%M:%S}]\033\[0m"
    }
} 1 2 30

$server start
vwait forever