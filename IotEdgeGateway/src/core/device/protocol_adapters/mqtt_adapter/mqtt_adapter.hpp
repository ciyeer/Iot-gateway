#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "mongoose.h"
#include "core/common/logger/logger.hpp"

namespace iotgw::core::device::protocol_adapters::mqtt {

class MqttClient {
public:
  struct Options {
    std::string url;
    std::string client_id;
    std::string user;
    std::string pass;
    std::uint16_t keepalive_sec = 30;
    bool clean_session = true;
    std::uint8_t version = 4;
  };

  using MessageHandler = std::function<void(const std::string& topic, const std::string& payload)>;

  explicit MqttClient(struct mg_mgr* mgr, std::shared_ptr<iotgw::core::common::log::Logger> logger);

  bool Connect(const Options& opt);
  bool Subscribe(const std::string& topic, std::uint8_t qos = 0);
  bool Publish(const std::string& topic, const std::string& payload, std::uint8_t qos = 0,
               bool retain = false);

  void SetMessageHandler(MessageHandler handler);
  bool IsOpen() const;

private:
  static void EventHandler(struct mg_connection* c, int ev, void* ev_data);
  void HandleEvent(struct mg_connection* c, int ev, void* ev_data);

private:
  struct mg_mgr* mgr_ = nullptr;
  struct mg_connection* conn_ = nullptr;
  Options opt_;
  std::string pending_sub_topic_;
  std::uint8_t pending_sub_qos_ = 0;
  bool open_ = false;
  MessageHandler on_msg_;
  std::shared_ptr<iotgw::core::common::log::Logger> logger_;
};

}  // namespace iotgw::core::device::protocol_adapters::mqtt
