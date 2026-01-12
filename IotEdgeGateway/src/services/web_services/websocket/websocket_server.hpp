#pragma once

#include <functional>
#include <string>
#include "core/common/utils/std_compat.hpp"
#include <vector>

#include "mongoose.h"
#include "core/common/logger/logger.hpp"

namespace iotgw::services::web_services::websocket {

class MongooseServer {
public:
  struct Options {
    std::string listen_addr = "http://0.0.0.0:8000";
    std::string ws_path = "/ws";
  };

  explicit MongooseServer(Options opt, std::shared_ptr<iotgw::core::common::log::Logger> logger)
      : opt_(std::move(opt)), logger_(std::move(logger)) {
    mg_mgr_init(&mgr_);
  }

  ~MongooseServer() {
    mg_mgr_free(&mgr_);
  }

  bool Start() {
    if (mg_http_listen(&mgr_, opt_.listen_addr.c_str(), EventHandler, this) == nullptr) {
      if (logger_) logger_->Error("Failed to listen on " + opt_.listen_addr);
      return false;
    }
    if (logger_) logger_->Info("Mongoose listening on " + opt_.listen_addr);
    return true;
  }

  void Poll(int timeout_ms) {
    mg_mgr_poll(&mgr_, timeout_ms);
  }

  mg_mgr* GetMgr() { return &mgr_; }

private:
  static void EventHandler(struct mg_connection* c, int ev, void* ev_data) {
    auto* self = static_cast<MongooseServer*>(c->fn_data);
    self->HandleEvent(c, ev, ev_data);
  }

  void HandleEvent(struct mg_connection* c, int ev, void* ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
      struct mg_http_message* hm = (struct mg_http_message*)ev_data;
      std::string_view uri(hm->uri.buf, hm->uri.len);
      
      if (logger_) logger_->Debug("HTTP request: " + std::string(uri));

      if (mg_match(hm->uri, mg_str(opt_.ws_path.c_str()), NULL)) {
        mg_ws_upgrade(c, hm, nullptr);
      } else if (mg_match(hm->uri, mg_str("/api/health"), NULL)) {
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"status\":\"ok\"}\n");
      } else if (mg_match(hm->uri, mg_str("/api/version"), NULL)) {
        // In a real app, version should come from a VersionController or Config
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"version\":\"0.1.0\"}\n");
      } else {
        // Serve static files or 404
        // For now, simple 404
        mg_http_reply(c, 404, "", "Not Found\n");
      }
    } else if (ev == MG_EV_WS_MSG) {
      struct mg_ws_message* wm = (struct mg_ws_message*)ev_data;
      std::string_view msg(wm->data.buf, wm->data.len);
      if (logger_) logger_->Debug("WS message: " + std::string(msg));
      // Echo back
      mg_ws_send(c, wm->data.buf, wm->data.len, WEBSOCKET_OP_TEXT);
    }
  }

private:
  Options opt_;
  std::shared_ptr<iotgw::core::common::log::Logger> logger_;
  struct mg_mgr mgr_;
};

}  // namespace iotgw::services::web_services::websocket
