#include "services/web_services/api/rest_api.hpp"

#include <string>

#include "core/common/utils/json_utils.hpp"

namespace iotgw {
namespace services {
namespace web_services {
namespace api {

namespace {

static std::string ToStdString(const struct mg_str& s) {
  return std::string(s.buf, s.len);
}

static std::string NormalizeBasePath(std::string p) {
  if (p.empty()) return std::string("/api");
  if (p[0] != '/') p.insert(p.begin(), '/');
  while (p.size() > 1 && p.back() == '/') p.pop_back();
  return p;
}

static bool StripBasePath(const std::string& uri, const std::string& base_path, std::string& out_rel) {
  if (base_path.empty() || base_path == "/") {
    out_rel = uri;
    return true;
  }
  if (uri == base_path) {
    out_rel = "/";
    return true;
  }
  const std::string prefix = base_path + "/";
  if (uri.size() >= prefix.size() && uri.compare(0, prefix.size(), prefix) == 0) {
    out_rel = uri.substr(base_path.size());
    if (out_rel.empty()) out_rel = "/";
    return true;
  }
  return false;
}

static bool IsMethod(const struct mg_http_message* hm, const char* method) {
  return mg_strcmp(hm->method, mg_str(method)) == 0;
}

}  // namespace

bool HandleHttpRequest(struct mg_connection* c, struct mg_http_message* hm, const ApiContext& ctx) {
  if (c == nullptr || hm == nullptr) return false;

  const std::string uri = ToStdString(hm->uri);
  const std::string base_path = NormalizeBasePath(ctx.base_path);

  std::string rel_path;
  if (!StripBasePath(uri, base_path, rel_path)) {
    if (base_path != "/api") {
      if (!StripBasePath(uri, std::string("/api"), rel_path)) return false;
    } else {
      return false;
    }
  }

  if (HandleSystemApi(c, hm, rel_path, ctx)) return true;
  if (HandleDeviceApi(c, hm, rel_path, ctx)) return true;
  if (HandleRuleApi(c, hm, rel_path, ctx)) return true;

  return false;
}

bool HandleSystemApi(struct mg_connection* c, struct mg_http_message* hm, const std::string& rel_path,
                     const ApiContext& ctx) {
  if (IsMethod(hm, "GET") && rel_path == "/health") {
    mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"status\":\"ok\"}\n");
    return true;
  }

  if (IsMethod(hm, "GET") && rel_path == "/version") {
    const std::string body =
        iotgw::core::common::json::Object({{"version", iotgw::core::common::json::Quote(ctx.version)}});
    mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\n", body.c_str());
    return true;
  }

  return false;
}

}  // namespace api
}  // namespace web_services
}  // namespace services
}  // namespace iotgw
