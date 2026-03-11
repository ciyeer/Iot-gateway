#include "core/device/manager/device_manager.hpp"
#include "services/web_services/api/rest_api.hpp"

#include <string>

namespace iotgw {
namespace services {
namespace web_services {
namespace api {

namespace {

static bool IsMethod(const struct mg_http_message* hm, const char* method) {
    return mg_strcmp(hm->method, mg_str(method)) == 0;
}

#include <ctime>

// Helper to construct envelope payload
static std::string MakeEnvelope(const std::string& id, const std::string& type, const std::string& data_json) {
    std::time_t now = std::time(nullptr);
    return "{\"device_id\":\"" + id + "\",\"type\":\"" + type + "\",\"data\":" + data_json +
           ",\"ts\":" + std::to_string(now) + "}";
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

            // Unified Device Model Parsing: Look into $.data.*
            // Also support fallback to flat JSON for backward compatibility if needed,
            // but here we strictly follow the new schema for standard compliance.

            if (d.id == "temp" || d.id == "humi" || d.id == "light" || d.id == "ir") {
                double val = 0;
                // Try envelope format first: $.data.value
                if (mg_json_get_num(json, "$.data.value", &val)) {
                    add_field(d.id, std::to_string(val));
                } else if (mg_json_get_num(json, "$.value", &val)) {  // Fallback
                    add_field(d.id, std::to_string(val));
                }
            } else if (d.id == "led") {
                double on = 0;
                bool has_on = mg_json_get_num(json, "$.data.on", &on) || mg_json_get_num(json, "$.on", &on);
                if (has_on) add_field("led_on", std::to_string((int)on));

                double br = 0;
                bool has_br = mg_json_get_num(json, "$.data.br", &br) || mg_json_get_num(json, "$.br", &br);
                if (has_br) add_field("led_br", std::to_string((int)br));
            } else if (d.id == "motor") {
                double on = 0;
                if (mg_json_get_num(json, "$.data.on", &on) || mg_json_get_num(json, "$.on", &on))
                    add_field("motor_on", std::to_string((int)on));

                double sp = 0;
                if (mg_json_get_num(json, "$.data.sp", &sp) || mg_json_get_num(json, "$.sp", &sp))
                    add_field("motor_sp", std::to_string((int)sp));

                double dir = 0;
                if (mg_json_get_num(json, "$.data.dir", &dir) || mg_json_get_num(json, "$.dir", &dir))
                    add_field("motor_dir", std::to_string((int)dir));
            } else if (d.id == "buzzer") {
                double on = 0;
                if (mg_json_get_num(json, "$.data.on", &on) || mg_json_get_num(json, "$.on", &on))
                    add_field("buzzer", std::to_string((int)on));
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
                std::string data = "{";
                if (led_on != -1) data += "\"on\": " + std::to_string((int)led_on);
                if (led_on != -1 && led_br != -1) data += ", ";
                if (led_br != -1) data += "\"br\": " + std::to_string((int)led_br);
                data += "}";

                std::string payload = MakeEnvelope("led", "actuator", data);

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
                std::string data = "{";
                bool first = true;
                if (motor_on != -1) {
                    data += "\"on\": " + std::to_string((int)motor_on);
                    first = false;
                }
                if (motor_sp != -1) {
                    if (!first) data += ", ";
                    data += "\"sp\": " + std::to_string((int)motor_sp);
                    first = false;
                }
                if (motor_dir != -1) {
                    if (!first) data += ", ";
                    data += "\"dir\": " + std::to_string((int)motor_dir);
                }
                data += "}";

                std::string payload = MakeEnvelope("motor", "actuator", data);

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
                std::string data = "{\"on\": " + std::to_string((int)buzzer) + "}";
                std::string payload = MakeEnvelope("buzzer", "actuator", data);

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
