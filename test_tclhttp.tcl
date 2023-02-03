#! /usr/bin/tclsh

load ./libtclhttp.so

set server [::http::server]
puts $server

set count 0

$server listen 8080
$server set_handler {{server conn method uri query headers body} {
    global count
    puts "server:   $server"
    puts "conn:     $conn"
    puts "method:   $method"
    puts "uri:      $uri"
    puts "query:    $query"
    puts "headers:  $headers"
    puts "body:     $body"

    $server reply $conn 200 {Name {Gibbon Ho}} "hello world [incr count]" 
}}
$server start


vwait forever
rename $server {}