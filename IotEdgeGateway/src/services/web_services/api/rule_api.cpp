#include "services/web_services/api/rest_api.hpp"

#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "core/common/config/config_manager.hpp"
#include "core/common/utils/json_utils.hpp"

namespace iotgw {
namespace services {
namespace web_services {
namespace api {

namespace {

static bool IsMethod(const struct mg_http_message* hm, const char* method) {
  return mg_strcmp(hm->method, mg_str(method)) == 0;
}

static bool StartsWith(const std::string& s, const std::string& prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

static bool EndsWith(const std::string& s, const std::string& suffix) {
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static bool TryParseDoubleStrict(const std::string& s, double& out) {
  if (s.empty()) return false;
  char* end = nullptr;
  const double v = std::strtod(s.c_str(), &end);
  if (end == s.c_str()) return false;
  while (end != nullptr && *end != '\0') {
    if (*end != ' ' && *end != '\n' && *end != '\r' && *end != '\t') return false;
    ++end;
  }
  out = v;
  return true;
}

static bool LoadRulesFromFile(const std::string& file_path, const std::string& category,
                              std::vector<iotgw::core::control::rule_engine::Rule>& out_rules) {
  iotgw::core::common::config::ConfigManager rcfg;
  if (!rcfg.LoadYamlFile(file_path)) return false;

  const std::string key = category + "_rules";
  std::size_t i = 0;
  while (true) {
    const std::string base = key + "[" + std::to_string(i) + "].";
    std::string id;
    if (!(rcfg.GetString(base + "id", id) && !id.empty())) break;

    iotgw::core::control::rule_engine::Rule r;
    r.id = id;
    r.category = category;

    bool enabled = true;
    if (rcfg.GetBool(base + "enabled", enabled)) r.enabled = enabled;

    (void)rcfg.GetString(base + "when.sensor_id", r.when.sensor_id);
    (void)rcfg.GetString(base + "when.op", r.when.op);

    std::string value_s;
    if (rcfg.GetString(base + "when.value", value_s)) {
      double dv = 0.0;
      if (TryParseDoubleStrict(value_s, dv)) r.when.value = dv;
    }

    std::size_t j = 0;
    while (true) {
      const std::string abase = base + "then[" + std::to_string(j) + "].";
      std::string type;
      if (!(rcfg.GetString(abase + "type", type) && !type.empty())) break;
      iotgw::core::control::rule_engine::Action a;
      a.type = type;
      (void)rcfg.GetString(abase + "actuator_id", a.actuator_id);
      (void)rcfg.GetString(abase + "value", a.value);
      (void)rcfg.GetString(abase + "level", a.level);
      (void)rcfg.GetString(abase + "message", a.message);
      r.then.push_back(std::move(a));
      ++j;
    }

    out_rules.push_back(std::move(r));
    ++i;
  }

  return true;
}

}  // namespace

bool HandleRuleApi(struct mg_connection* c, struct mg_http_message* hm, const std::string& rel_path,
                   const ApiContext& ctx) {
  if (c == nullptr || hm == nullptr) return false;
  if (ctx.rule_engine == nullptr) {
    mg_http_reply(c, 500, "Content-Type: application/json\r\n", "{\"error\":\"rule_engine_null\"}\n");
    return true;
  }

  if (IsMethod(hm, "GET") && rel_path == "/rules") {
    const auto& rs = ctx.rule_engine->Rules();
    std::string body;
    body.reserve(256 + rs.size() * 128);
    body.push_back('[');
    bool first = true;
    for (const auto& r : rs) {
      if (!first) body.push_back(',');
      first = false;
      body += iotgw::core::common::json::Object({
          {"id", iotgw::core::common::json::Quote(r.id)},
          {"category", iotgw::core::common::json::Quote(r.category)},
          {"enabled", iotgw::core::common::json::Bool(r.enabled)},
          {"sensor_id", iotgw::core::common::json::Quote(r.when.sensor_id)},
          {"op", iotgw::core::common::json::Quote(r.when.op)},
          {"value", iotgw::core::common::json::Number(r.when.value)},
      });
    }
    body.push_back(']');
    mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\n", body.c_str());
    return true;
  }

  if (IsMethod(hm, "POST") && rel_path == "/rules/reload") {
    std::vector<iotgw::core::control::rule_engine::Rule> rules;
    (void)LoadRulesFromFile(ctx.rules_automation_file, "automation", rules);
    (void)LoadRulesFromFile(ctx.rules_alarm_file, "alarm", rules);
    ctx.rule_engine->Clear();
    ctx.rule_engine->AddRules(std::move(rules));
    mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"ok\":true}\n");
    return true;
  }

  if (IsMethod(hm, "POST") && StartsWith(rel_path, "/rules/") &&
      (EndsWith(rel_path, "/enable") || EndsWith(rel_path, "/disable"))) {
    const bool enable = EndsWith(rel_path, "/enable");
    const std::string base = std::string("/rules/");
    const std::string tail = enable ? std::string("/enable") : std::string("/disable");
    const std::string id = rel_path.substr(base.size(), rel_path.size() - base.size() - tail.size());
    const bool ok = !id.empty() && ctx.rule_engine->SetEnabled(id, enable);
    const std::string body =
        iotgw::core::common::json::Object({{"ok", iotgw::core::common::json::Bool(ok)}});
    mg_http_reply(c, ok ? 200 : 404, "Content-Type: application/json\r\n", "%s\n", body.c_str());
    return true;
  }

  return false;
}

}  // namespace api
}  // namespace web_services
}  // namespace services
}  // namespace iotgw
