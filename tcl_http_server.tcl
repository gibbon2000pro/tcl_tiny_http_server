#! /usr/bin/tclsh

load ./libtclhttp.so

package require Thread

::oo::class create HttpServer {
    variable server
    variable handlers {}
    constructor {port} {
        set server [::http::server]
        $server listen $port
        $server set_handler "args {[self] handle_req {*}\$args}"
    }
    destructor {
        foreach handler $handlers {
            lassign $handler pattern func pool
            ::tpool::release $pool 
        }
        set handlers {}
        rename $server {}
    }

    method set_handler {uri_pattern func {initcmd {}} {minworkers 1} {maxworkers 100} {idletime 60}} {
        set main_td [::thread::id]
        set pool_init_script {
            package require Thread
            set main_td @main_td@
            set server @server@
            proc reply {conn code headers body} {
                global main_td server
                ::thread::send -async $main_td [format {%s reply %d %d {%s} {%s}} $server $conn $code $headers $body]
            }
            proc begin_chunk {conn} {
                global main_td server
                ::thread::send -async $main_td [format {%s begin_chunk %d} $server $conn]
            }
            proc chunk {conn data} {
                global main_td server
                ::thread::send -async $main_td [format {%s chunk %d {%s}} $server $conn $data]
            }
            proc end_chunk {conn} {
                global main_td server
                ::thread::send -async $main_td [format {%s end_chunk %d} $server $conn]
            }
            proc reply_file {conn name} {
                global main_td server
                ::thread::send -async $main_td [format {%s reply_file %d {%s}} $server $conn $name]
            }
            proc handle_req {conn method uri query headers body} {
                # puts "func:     $func"
                # puts "conn:     $conn"
                # puts "method:   $method"
                # puts "uri:      $uri"
                # puts "query:    $query"
                # puts "headers:  $headers"
                # puts "body:     $body"
                set query_dict {}
                set fields [split $query {&}]
                foreach field $fields {
                    set kv [split $field {=}]
                    if {[llength $kv] == 2} {
                        lassign $kv k v
                        dict append query_dict $k $v
                    }
                }
                catch {handler $conn $method $uri $query_dict $headers $body} result options
                if [dict get $options {-code}] {
                    puts stderr $options
                }
            }

            #handler
            proc handler @handler@

            # initcmd
            @initcmd@
        }
        set pool_init_script [string map [dict create @main_td@ $main_td @server@ [self] @handler@ $func @initcmd@ $initcmd] $pool_init_script]
        set pool [::tpool::create \
            -minworkers $minworkers \
            -maxworkers $maxworkers \
            -idletime $idletime \
            -initcmd $pool_init_script]
        ::tpool::preserve $pool
        lappend handlers [list $uri_pattern $func $pool]
    }

    method start {} {
        $server start
    }

    method handle_req {s conn method uri query headers body} {
        # puts "server:   $server"
        # puts "conn:     $conn"
        # puts "method:   $method"
        # puts "uri:      $uri"
        # puts "query:    $query"
        # puts "headers:  $headers"
        # puts "body:     $body"
        foreach handler $handlers {
            lassign $handler pattern func pool
            if [string match -nocase $pattern $uri] {
                set post_cmd [format {handle_req %d %s {%s} {%s} {%s} {%s}} $conn $method $uri $query $headers $body]
                ::tpool::post -detached $pool $post_cmd
                return
            }
        }
        my reply $conn 404 {} {}
    }

    method reply {conn code headers body} {
        $server reply $conn $code $headers $body
    }

    method begin_chunk {conn} {
        $server reply_chunk $conn begin
    }

    method chunk {conn data} {
        $server reply_chunk $conn send $data
    }

    method end_chunk {conn} {
        $server reply_chunk $conn end
    }

    method reply_file {conn name} {
        $server reply_file $conn $name
    }


}
