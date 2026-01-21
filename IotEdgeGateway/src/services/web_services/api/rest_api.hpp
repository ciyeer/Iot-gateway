#pragma once

#include <memory>
#include <string>

#include "mongoose.h"

#include "core/common/logger/logger.hpp"
#include "core/control/rule_engine.hpp"
#include "core/device/manager/device_manager.hpp"
#include "core/device/protocol_adapters/mqtt_adapter/mqtt_adapter.hpp"

namespace iotgw {
namespace services {
namespace web_services {
namespace api {

struct ApiContext {
    std::string base_path = "/api";
    std::string version;
    std::string rules_automation_file;
    std::string rules_alarm_file;
    std::string mqtt_topic_prefix;

    iotgw::core::device::manager::DeviceRegistry* device_registry = nullptr;
    iotgw::core::control::rule_engine::RuleEngine* rule_engine = nullptr;
    iotgw::core::device::protocol_adapters::mqtt::MqttClient* mqtt_client = nullptr;

    std::shared_ptr<iotgw::core::common::log::Logger> logger;
};

bool HandleHttpRequest(struct mg_connection* c, struct mg_http_message* hm, const ApiContext& ctx);

bool HandleSystemApi(struct mg_connection* c, struct mg_http_message* hm, const std::string& rel_path,
                     const ApiContext& ctx);

bool HandleDeviceApi(struct mg_connection* c, struct mg_http_message* hm, const std::string& rel_path,
                     const ApiContext& ctx);

bool HandleRuleApi(struct mg_connection* c, struct mg_http_message* hm, const std::string& rel_path,
                   const ApiContext& ctx);

}  // namespace api
}  // namespace web_services
}  // namespace services
}  // namespace iotgw
