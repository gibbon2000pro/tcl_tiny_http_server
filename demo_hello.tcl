#! /usr/bin/tclsh

source ./tcl_http_server.tcl

set server [HttpServer new 8000]
$server set_handler "*" {{conn method uri query headers body} {
    global count
    puts "conn:     $conn"
    puts "method:   $method"
    puts "uri:      $uri"
    puts "query:    $query"
    puts "headers:  $headers"
    puts "body:     $body"

    reply $conn 200 {Name {Gibbon Ho}} "hello world [incr count]"
}} {
    set count 0
}
$server start

vwait forever
