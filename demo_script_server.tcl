#! /usr/bin/tclsh

source ./tcl_http_server.tcl

set server [HttpServer new 8889]

set initcmd {
    ::oo::class create WriteCache {
        variable write_callback
        constructor {} {
            set write_callback {{data} {}}
        }
        method initialize {ch mode} {
            return "initialize finalize write"
        }
        method finalize {ch} {
            my destroy
        }
        method write {ch data} {
            apply $write_callback $data
            return {}
        }
        method set_callback cb {
            set write_callback $cb
        }
    }

    set cache_stdout [WriteCache new]
    set cache_stderr [WriteCache new]
    chan push stdout $cache_stdout
    chan push stderr $cache_stderr
}

set func {{conn method uri query headers body} {
    global cache_stdout cache_stderr
    set fn [format {{data} {chunk %d $data}} $conn]

    $cache_stdout set_callback $fn
    $cache_stderr set_callback $fn

    begin_chunk $conn
    catch $body result options
    if [dict get $options {-code}] {
        set err_info {}
        lappend err_info "ERROR_INFO:\n[dict get $options {-errorinfo}]\n"
        lappend err_info "ERROR_LINE: [dict get $options {-errorline}]\n"
        lappend err_info "ERROR_STACK:\n[dict get $options {-errorstack}]\n"

        chunk $conn [join $err_info {}]
    }
    end_chunk $conn
}}


$server set_handler "/" $func $initcmd
$server start

vwait forever
