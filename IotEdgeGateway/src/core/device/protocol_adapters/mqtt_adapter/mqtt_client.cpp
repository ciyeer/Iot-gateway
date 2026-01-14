#include "core/device/protocol_adapters/mqtt_adapter/mqtt_adapter.hpp"

#include <utility>

namespace iotgw::core::device::protocol_adapters::mqtt {

MqttClient::MqttClient(struct mg_mgr* mgr, std::shared_ptr<iotgw::core::common::log::Logger> logger)
    : mgr_(mgr), logger_(std::move(logger)) {}

bool MqttClient::Connect(const Options& opt) {
  if (mgr_ == nullptr) return false;
  opt_ = opt;

  mg_mqtt_opts mo{};
  mo.user = mg_str(opt_.user.c_str());
  mo.pass = mg_str(opt_.pass.c_str());
  mo.client_id = mg_str(opt_.client_id.c_str());
  mo.keepalive = opt_.keepalive_sec;
  mo.clean = opt_.clean_session;
  mo.version = opt_.version;

  conn_ = mg_mqtt_connect(mgr_, opt_.url.c_str(), &mo, EventHandler, this);
  if (conn_ == nullptr) {
    if (logger_) logger_->Error("MQTT connect failed: " + opt_.url);
    return false;
  }
  if (logger_) logger_->Info("MQTT connecting: " + opt_.url);
  return true;
}

bool MqttClient::Subscribe(const std::string& topic, std::uint8_t qos) {
  if (topic.empty()) return false;
  pending_sub_topic_ = topic;
  pending_sub_qos_ = qos;

  if (conn_ == nullptr || !open_) return true;

  mg_mqtt_opts sub{};
  sub.topic = mg_str(pending_sub_topic_.c_str());
  sub.qos = pending_sub_qos_;
  mg_mqtt_sub(conn_, &sub);
  if (logger_) logger_->Info("MQTT subscribed: " + pending_sub_topic_);
  return true;
}

bool MqttClient::Publish(const std::string& topic, const std::string& payload, std::uint8_t qos,
                         bool retain) {
  if (conn_ == nullptr || !open_) return false;
  if (topic.empty()) return false;

  mg_mqtt_opts pub{};
  pub.topic = mg_str(topic.c_str());
  pub.message = mg_str(payload.c_str());
  pub.qos = qos;
  pub.retain = retain;
  (void) mg_mqtt_pub(conn_, &pub);
  return true;
}

void MqttClient::SetMessageHandler(MessageHandler handler) {
  on_msg_ = std::move(handler);
}

bool MqttClient::IsOpen() const { return open_; }

void MqttClient::EventHandler(struct mg_connection* c, int ev, void* ev_data) {
  auto* self = static_cast<MqttClient*>(c->fn_data);
  if (self != nullptr) self->HandleEvent(c, ev, ev_data);
}

void MqttClient::HandleEvent(struct mg_connection* c, int ev, void* ev_data) {
  if (ev == MG_EV_MQTT_OPEN) {
    const int* code = static_cast<const int*>(ev_data);
    open_ = (code != nullptr && *code == 0);
    if (!open_) {
      if (logger_) logger_->Error("MQTT connack error");
      return;
    }
    if (logger_) logger_->Info("MQTT connected");
    if (!pending_sub_topic_.empty()) {
      mg_mqtt_opts sub{};
      sub.topic = mg_str(pending_sub_topic_.c_str());
      sub.qos = pending_sub_qos_;
      mg_mqtt_sub(c, &sub);
      if (logger_) logger_->Info("MQTT subscribed: " + pending_sub_topic_);
    }
  } else if (ev == MG_EV_MQTT_MSG) {
    const auto* mm = static_cast<const mg_mqtt_message*>(ev_data);
    if (mm == nullptr) return;
    const std::string topic(mm->topic.buf, mm->topic.len);
    const std::string payload(mm->data.buf, mm->data.len);
    if (on_msg_) on_msg_(topic, payload);
  } else if (ev == MG_EV_CLOSE) {
    if (c == conn_) {
      open_ = false;
      conn_ = nullptr;
      if (logger_) logger_->Warn("MQTT disconnected");
    }
  } else if (ev == MG_EV_ERROR) {
    const char* err = static_cast<const char*>(ev_data);
    if (logger_) logger_->Error(std::string("MQTT error: ") + (err ? err : "unknown"));
  }
}

}  // namespace iotgw::core::device::protocol_adapters::mqtt
