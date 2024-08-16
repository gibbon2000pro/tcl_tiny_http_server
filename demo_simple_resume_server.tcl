#! /usr/bin/tclsh

load ./libtclhttp.so

set server [::http::server]
puts $server

proc now {} {
    return "\033\[33m[clock format [clock seconds] -format {%Y-%m-%d %H:%M:%S}]\033\[0m"
}

set count 0

$server listen 8080
$server set_handler {{server conn method uri query headers body} {
    global count
    puts "count:    [incr count]"
    puts "server:   $server"
    puts "conn:     $conn"
    puts "method:   $method"
    puts "uri:      $uri"
    puts "query:    $query"
    puts "headers:  $headers"
    puts "body:     $body"

    if [string match -nocase "/gibbon" $uri] {
        $server reply_file $conn {resume/贺渊凌1.pdf}
        return
    } elseif [string match -nocase "/rita" $uri] {
        $server reply_file $conn {resume/阳娟个人简历1.16.pdf}
        return
    }
    
    $server reply $conn 404 {} {WTF???}
}}
$server start

vwait forever
rename $server {}