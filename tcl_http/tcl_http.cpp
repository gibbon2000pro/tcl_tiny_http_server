#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>

#include "mongoose.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"

extern "C" {
#include <tcl8.6/tcl.h>
extern DLLEXPORT int Tclhttp_Init(Tcl_Interp *interp);
}

struct ReqContext {
    mg_connection *conn = nullptr;
    mg_http_message *hm = nullptr;
};

static inline std::string mg_addr_to_string(const mg_addr &addr) {
    std::stringstream ss;
    if (addr.is_ip6) {
        for (size_t i = 0; i < sizeof(addr.ip6); ++i) {
            if (i > 0) {
                ss << ".";
            }
            ss << std::to_string(addr.ip6[i]);
        }
    } else {
        const uint8_t *start = reinterpret_cast<const uint8_t *>(&addr.ip);
        for (uint32_t i = 0; i < 4; ++i) {
            if (i > 0) {
                ss << ".";
            }
            ss << std::to_string(start[i]);
        }
    }
    ss << ":" << addr.port;
    return ss.str();
}

struct HttpServer {
    std::string name;
    mg_mgr mgr;
    Tcl_Interp *interp;
    Tcl_Obj *handler;
    std::unordered_map<unsigned long, ReqContext> connections;
    std::shared_ptr<spdlog::logger> logger;

    HttpServer(Tcl_Interp *interp_)
        : interp(interp_),
          handler(nullptr) {
        mg_mgr_init(&mgr);
        spdlog::set_level(spdlog::level::debug);
        logger = spdlog::basic_logger_mt("HttpServer", "http_server.log", true);
        mg_timer_add(&mgr, 30000, MG_TIMER_REPEAT,
                     &HttpServer::interval_check, this);
    }

    ~HttpServer() {
        mg_mgr_free(&mgr);
        if (handler) {
            Tcl_DecrRefCount(handler);
            handler = nullptr;
        }
    }

    mg_connection *listen(int port) {
        std::string listenStr = "http://0.0.0.0:" + std::to_string(port);
        return mg_http_listen(&mgr, listenStr.c_str(), &HttpServer::event_handler, this);
    }

    void set_handler(Tcl_Obj *handler_) {
        if (handler) {
            Tcl_DecrRefCount(handler);
        }
        handler = handler_;
        Tcl_IncrRefCount(handler);
    }

    void reply(unsigned long connId, int code, Tcl_Obj *headers, Tcl_Obj *body) {
        auto find = connections.find(connId);
        if (find == connections.end()) {
            return;
        }
        ReqContext &ctx = find->second;
        // headers
        std::stringstream ss;
        Tcl_DictSearch search;
        Tcl_Obj *key, *value;
        int done;
        if (Tcl_DictObjFirst(interp, headers, &search, &key, &value, &done) != TCL_OK) {
            mg_http_reply(ctx.conn, 500, nullptr, "");
            return;
        }
        for (; !done; Tcl_DictObjNext(&search, &key, &value, &done)) {
            ss << Tcl_GetString(key) << ": " << Tcl_GetString(value) << "\r\n";
        }
        Tcl_DictObjDone(&search);
        // resp
        mg_http_reply(ctx.conn, code, ss.str().c_str(), Tcl_GetString(body));
    }

    void reply_chunk_begin(unsigned long connId) {
        auto find = connections.find(connId);
        if (find == connections.end()) {
            return;
        }
        ReqContext &ctx = find->second;
        mg_printf(ctx.conn, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
    }

    void reply_chunk(unsigned long connId, Tcl_Obj *chunk) {
        auto find = connections.find(connId);
        if (find == connections.end()) {
            return;
        }
        ReqContext &ctx = find->second;
        const char *str = nullptr;
        int strLen;
        str = Tcl_GetStringFromObj(chunk, &strLen);
        mg_http_write_chunk(ctx.conn, str, strLen);
    }

    void reply_chunk_end(unsigned long connId) {
        auto find = connections.find(connId);
        if (find == connections.end()) {
            return;
        }
        ReqContext &ctx = find->second;
        mg_http_printf_chunk(ctx.conn, "");
    }

    void reply_file(unsigned long connId, const std::string &file) {
        auto find = connections.find(connId);
        if (find == connections.end()) {
            return;
        }
        ReqContext &ctx = find->second;
        mg_http_serve_opts opts;
        memset(&opts, 0, sizeof(mg_http_serve_opts));
        mg_http_serve_file(ctx.conn, ctx.hm, file.c_str(), &opts);
    }

    static void handle_once(ClientData clientData) {
        HttpServer *self = (HttpServer *)clientData;
        mg_mgr_poll(&self->mgr, 10);
        Tcl_DoWhenIdle(&HttpServer::handle_once, self);
    }

    void start() {
        Tcl_DoWhenIdle(&HttpServer::handle_once, this);
    }

    static void interval_check(void *fn_data) {
        HttpServer *self = (HttpServer *)fn_data;
        // uint64_t now = mg_millis();
        self->logger->debug("interval_check() connections.size->{}", self->connections.size());
        for (auto &iter : self->connections) {
            auto &conn = *(iter.second.conn);
            self->logger->debug("interval_check() conn_id->{} loc->{} rem->{}",
                                conn.id,
                                mg_addr_to_string(conn.loc),
                                mg_addr_to_string(conn.rem));
        }
        self->logger->flush();
    }

    static void event_handler(mg_connection *conn, int ev, void *ev_data, void *fn_data) {
        HttpServer *self = (HttpServer *)fn_data;
        auto &logger = self->logger;
        // logger->debug("event_handler begin. conn_id->{} ev->{} conn_is_closing->{} conn_is_readable->{} conn_is_writable->{}",
        //               conn->id, ev, bool(conn->is_closing), bool(conn->is_readable), bool(conn->is_writable));

        if (ev == MG_EV_OPEN) {
            auto &ctx = self->connections[conn->id];
            ctx.conn = conn;
            logger->debug("event_handler MG_EV_OPEN. conn_id->{}", conn->id);
            logger->flush();
            return;
        }

        if (ev == MG_EV_CLOSE) {
            self->connections.erase(conn->id);
            logger->debug("event_handler MG_EV_CLOSE. conn_id->{}", conn->id);
            logger->flush();
            return;
        }

        if (ev == MG_EV_HTTP_MSG) {
            if (!self->handler) {
                mg_http_reply(conn, 404, nullptr, "");
                return;
            }

            mg_http_message *hm = (struct mg_http_message *)ev_data;
            std::string uri(hm->uri.ptr, hm->uri.len);

            auto find = self->connections.find(conn->id);
            if (find == self->connections.end()) {
                return;
            }

            auto &ctx = find->second;
            ctx.hm = hm;

            int result = TCL_ERROR;
            {
                Tcl_Obj *callback[9];

                callback[0] = Tcl_NewStringObj("apply", -1);
                // Tcl_IncrRefCount(callback[0]);
                callback[1] = self->handler;
                // server_name
                callback[2] = Tcl_NewStringObj(self->name.c_str(), -1);
                logger->debug("event_handler MG_EV_HTTP_MSG. conn_id->{} server_name->{}",
                              conn->id, self->name);
                // conn_id
                callback[3] = Tcl_NewIntObj(conn->id);
                // method
                callback[4] = Tcl_NewStringObj(hm->method.ptr, hm->method.len);
                logger->debug("event_handler MG_EV_HTTP_MSG. conn_id->{} method->{}",
                              conn->id, std::string(hm->method.ptr, hm->method.len));
                // uri
                callback[5] = Tcl_NewStringObj(hm->uri.ptr, hm->uri.len);
                logger->debug("event_handler MG_EV_HTTP_MSG. conn_id->{} uri->{}",
                              conn->id, std::string(hm->uri.ptr, hm->uri.len));
                // query
                callback[6] = Tcl_NewStringObj(hm->query.ptr, hm->query.len);
                logger->debug("event_handler MG_EV_HTTP_MSG. conn_id->{} query->{}",
                              conn->id, std::string(hm->query.ptr, hm->query.len));
                // headers
                Tcl_Obj *headers = Tcl_NewDictObj();
                std::stringstream ss;
                for (int i = 0; i < MG_MAX_HTTP_HEADERS; ++i) {
                    const mg_http_header &h = hm->headers[i];
                    if (h.name.len == 0 || h.name.ptr == nullptr || h.value.len == 0 || h.value.ptr == nullptr) {
                        break;
                    }

                    Tcl_Obj *key = Tcl_NewStringObj(h.name.ptr, h.name.len);
                    ss << std::string(h.name.ptr, h.name.len) << ":";
                    Tcl_Obj *value = Tcl_NewStringObj(h.value.ptr, h.value.len);
                    ss << std::string(h.value.ptr, h.value.len) << ";";
                    Tcl_DictObjPut(self->interp, headers, key, value);
                }
                callback[7] = headers;
                // body
                callback[8] = Tcl_NewStringObj(hm->body.ptr, hm->body.len);

                logger->debug("event_handler MG_EV_HTTP_MSG. conn_id->{} headers->{}",
                              conn->id, ss.str());

                // logger->debug("event_handler MG_EV_HTTP_MSG. conn_id->{} server_name->{} method->{} uri->{} query->{} headers->{}",
                //               conn->id, self->name,
                //               std::string(hm->method.ptr, hm->method.len),
                //               std::string(hm->uri.ptr, hm->uri.len),
                //               std::string(hm->query.ptr, hm->query.len),
                //               ss.str());

                for (int i = 0; i < 9; ++i) {
                    Tcl_IncrRefCount(callback[i]);
                }

                result = Tcl_EvalObjv(self->interp, 9, callback, 0);
                for (int i = 0; i < 9; ++i) {
                    Tcl_DecrRefCount(callback[i]);
                }
            }
            if (result != TCL_OK) {
                mg_http_reply(conn, 500, nullptr, "");
            }
            logger->flush();
            return;
        }
        // logger->debug("event_handler end. conn_id->{}", conn->id);
    }
};

static inline std::string tcl_obj_to_string(Tcl_Obj *obj) {
    const char *str = nullptr;
    int strLen;
    str = Tcl_GetStringFromObj(obj, &strLen);
    return std::string(str, strLen);
}

static int server_command(ClientData clientData, Tcl_Interp *interp, int objc, struct Tcl_Obj *const *objv) {
    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "listen | set_handler | start | reply | reply_chunk");
        return TCL_ERROR;
    }
    HttpServer *server = (HttpServer *)clientData;
    std::string command = tcl_obj_to_string(objv[1]);

    if (command == "listen") {
        if (objc < 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "port");
            return TCL_ERROR;
        }
        int port;
        if (Tcl_GetIntFromObj(interp, objv[2], &port) != TCL_OK) {
            Tcl_AddErrorInfo(interp, "port is a number");
            return TCL_ERROR;
        }
        mg_connection *conn = server->listen(port);
        if (conn == nullptr) {
            Tcl_AddErrorInfo(interp, "listen error");
            return TCL_ERROR;
        }
        return TCL_OK;
    } else if (command == "set_handler") {
        if (objc < 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "fun");
            return TCL_ERROR;
        }
        server->set_handler(objv[2]);
        return TCL_OK;
    } else if (command == "start") {
        server->start();
        std::cout << "start()" << std::endl;
        return TCL_OK;
    } else if (command == "reply") {
        if (objc < 6) {
            Tcl_WrongNumArgs(interp, 2, objv, "id code headers body");
            return TCL_ERROR;
        }
        long connId;
        if (Tcl_GetLongFromObj(interp, objv[2], &connId) != TCL_OK) {
            Tcl_AppendResult(interp, "id is a number");
            return TCL_ERROR;
        }
        int code;
        if (Tcl_GetIntFromObj(interp, objv[3], &code) != TCL_OK) {
            Tcl_AppendResult(interp, "code is a number");
            return TCL_ERROR;
        }
        server->reply(connId, code, objv[4], objv[5]);
        return TCL_OK;
    } else if (command == "reply_chunk") {
        if (objc < 4) {
            Tcl_WrongNumArgs(interp, 2, objv, "id begin code | id send chunk | id end");
            return TCL_ERROR;
        }
        long connId;
        if (Tcl_GetLongFromObj(interp, objv[2], &connId) != TCL_OK) {
            Tcl_AppendResult(interp, "id is a number");
            return TCL_ERROR;
        }
        std::string op = tcl_obj_to_string(objv[3]);
        if (op == "begin") {
            server->reply_chunk_begin(connId);
            return TCL_OK;
        } else if (op == "send") {
            if (objc < 5) {
                Tcl_WrongNumArgs(interp, 4, objv, "chunk");
                return TCL_ERROR;
            }
            server->reply_chunk(connId, objv[4]);
            return TCL_OK;
        } else if (op == "end") {
            server->reply_chunk_end(connId);
            return TCL_OK;
        } else {
            Tcl_AppendResult(interp, "begin | send | end");
            return TCL_ERROR;
        }
    } else if (command == "reply_file") {
        if (objc < 4) {
            Tcl_WrongNumArgs(interp, 2, objv, "id file_path");
            return TCL_ERROR;
        }
        long connId;
        if (Tcl_GetLongFromObj(interp, objv[2], &connId) != TCL_OK) {
            Tcl_AppendResult(interp, "id is a number");
            return TCL_ERROR;
        }
        std::string file = tcl_obj_to_string(objv[3]);
        server->reply_file(connId, file);
        return TCL_OK;
    }

    Tcl_WrongNumArgs(interp, 1, objv, "listen | set_handler | start | reply | reply_chunk");
    return TCL_ERROR;
}

static void destroy_server(ClientData clientData) {
    HttpServer *server = (HttpServer *)clientData;
    delete server;
    std::cout << "destroy_server()" << std::endl;
}

static int create_server(ClientData clientData, Tcl_Interp *interp, int objc, struct Tcl_Obj *const *objv) {
    static int seq = 0;
    HttpServer *server = new HttpServer(interp);
    server->name = "::http::server" + std::to_string(++seq);
    Tcl_CreateObjCommand(interp, server->name.c_str(), &server_command, server, &destroy_server);
    Tcl_SetResult(interp, (char *)server->name.c_str(), TCL_VOLATILE);
    return TCL_OK;
}

int Tclhttp_Init(Tcl_Interp *interp) {
    if (Tcl_InitStubs(interp, "8.6", 0) == nullptr) {
        return TCL_ERROR;
    }

    Tcl_CreateObjCommand(interp, "::http::server", &create_server, nullptr, nullptr);
    return TCL_OK;
}