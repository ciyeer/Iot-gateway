#include "services/web_services/api/rest_api.hpp"
#include "core/device/manager/device_manager.hpp"

#include <iostream>
#include <cmath>
#include <string>

#include "core/common/utils/json_utils.hpp"

namespace iotgw {
namespace services {
namespace web_services {
namespace api {

namespace {

static bool IsMethod(const struct mg_http_message* hm, const char* method) {
  return mg_strcmp(hm->method, mg_str(method)) == 0;
}

}  // namespace

bool HandleControlApi(struct mg_connection* c, struct mg_http_message* hm, const std::string& rel_path,
                      const ApiContext& ctx) {
  if (c == nullptr || hm == nullptr) return false;

  // GET /status
  if (IsMethod(hm, "GET") && rel_path == "/status") {
    if (!ctx.device_registry) {
        mg_http_reply(c, 500, "Content-Type: application/json\r\n", "{\"error\":\"no_registry\"}\n");
        return true;
    }
    
    auto devices = ctx.device_registry->List();
    std::string json_body = "{";
    bool first = true;
    
    auto add_field = [&](const std::string& key, const std::string& val) {
        if (!first) json_body += ",";
        first = false;
        json_body += "\"" + key + "\":" + val;
    };

    for (const auto& d : devices) {
        if (d.status.last_payload.empty()) continue;
        
        struct mg_str json = mg_str(d.status.last_payload.c_str());
        
        if (d.id == "temp" || d.id == "humi" || d.id == "light" || d.id == "ir") {
            double val = 0;
            if (mg_json_get_num(json, "$.value", &val)) {
                add_field(d.id, std::to_string(val));
            }
        } else if (d.id == "led") {
             double on = 0;
             if (mg_json_get_num(json, "$.on", &on)) add_field("led_on", std::to_string((int)on));
             double br = 0;
             if (mg_json_get_num(json, "$.br", &br)) add_field("led_br", std::to_string((int)br));
        } else if (d.id == "motor") {
             double on = 0;
             if (mg_json_get_num(json, "$.on", &on)) add_field("motor_on", std::to_string((int)on));
             double sp = 0;
             if (mg_json_get_num(json, "$.sp", &sp)) add_field("motor_sp", std::to_string((int)sp));
             double dir = 0;
             if (mg_json_get_num(json, "$.dir", &dir)) add_field("motor_dir", std::to_string((int)dir));
        } else if (d.id == "buzzer") {
             double on = 0;
             if (mg_json_get_num(json, "$.on", &on)) add_field("buzzer", std::to_string((int)on));
        }
    }
    json_body += "}";
    mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\n", json_body.c_str());
    return true;
  }

  // POST /control
  if (IsMethod(hm, "POST") && rel_path == "/control") {
      std::string body(hm->body.buf, hm->body.len);
      struct mg_str json = mg_str(body.c_str());
      
      if (ctx.mqtt_client && ctx.mqtt_client->IsOpen()) {
          // LED Control
          double led_on = -1, led_br = -1;
          mg_json_get_num(json, "$.payload.led_on", &led_on);
          mg_json_get_num(json, "$.payload.led_br", &led_br);
          
          if (led_on != -1 || led_br != -1) {
              std::string payload = "{";
              if (led_on != -1) payload += "\"on\": " + std::to_string((int)led_on);
              if (led_on != -1 && led_br != -1) payload += ", ";
              if (led_br != -1) payload += "\"br\": " + std::to_string((int)led_br);
              payload += "}";
              
              std::string topic = ctx.mqtt_topic_prefix + "cmd/led";
              if (ctx.mqtt_client->Publish(topic, payload, 0, false)) {
                  if (ctx.device_registry) {
                      std::string out_id;
                      iotgw::core::device::model::DeviceEntity d;
                      if (ctx.device_registry->Get("led", d) && !d.telemetry_topic.empty()) {
                          ctx.device_registry->UpdateFromTelemetryTopic(d.telemetry_topic, payload, 0, out_id);
                      }
                  }
              }
          }

          // Motor Control
          double motor_on = -1, motor_sp = -1, motor_dir = -1;
          mg_json_get_num(json, "$.payload.motor_on", &motor_on);
          mg_json_get_num(json, "$.payload.motor_sp", &motor_sp);
          mg_json_get_num(json, "$.payload.motor_dir", &motor_dir);

          if (motor_on != -1 || motor_sp != -1 || motor_dir != -1) {
              std::string payload = "{";
              bool first = true;
              if (motor_on != -1) { payload += "\"on\": " + std::to_string((int)motor_on); first = false; }
              if (motor_sp != -1) { if(!first) payload+=", "; payload += "\"sp\": " + std::to_string((int)motor_sp); first=false; }
              if (motor_dir != -1) { if(!first) payload+=", "; payload += "\"dir\": " + std::to_string((int)motor_dir); }
              payload += "}";
              
              std::string topic = ctx.mqtt_topic_prefix + "cmd/motor";
              if (ctx.mqtt_client->Publish(topic, payload, 0, false)) {
                  if (ctx.device_registry) {
                      std::string out_id;
                      iotgw::core::device::model::DeviceEntity d;
                      if (ctx.device_registry->Get("motor", d) && !d.telemetry_topic.empty()) {
                          ctx.device_registry->UpdateFromTelemetryTopic(d.telemetry_topic, payload, 0, out_id);
                      }
                  }
              }
          }

          // Buzzer Control
          double buzzer = -1;
          mg_json_get_num(json, "$.payload.buzzer", &buzzer);
          if (buzzer != -1) {
              std::string payload = "{\"on\": " + std::to_string((int)buzzer) + "}";
              
              std::string topic = ctx.mqtt_topic_prefix + "cmd/buzzer";
              if (ctx.mqtt_client->Publish(topic, payload, 0, false)) {
                  if (ctx.device_registry) {
                      std::string out_id;
                      iotgw::core::device::model::DeviceEntity d;
                      if (ctx.device_registry->Get("buzzer", d) && !d.telemetry_topic.empty()) {
                          ctx.device_registry->UpdateFromTelemetryTopic(d.telemetry_topic, payload, 0, out_id);
                      }
                  }
              }
          }
      } else {
          // MQTT client not ready
      }

      mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"status\":\"ok\"}\n");
      return true;
  }

  return false;
}

}  // namespace api
}  // namespace web_services
}  // namespace services
}  // namespace iotgw
