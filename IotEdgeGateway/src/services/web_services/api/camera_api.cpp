#include "services/web_services/api/rest_api.hpp"

#include <string>

#include "core/common/utils/json_utils.hpp"
#include "services/system_services/camera/camera_manager.hpp"

namespace iotgw {
namespace services {
namespace web_services {
namespace api {

namespace {

static bool IsMethod(const struct mg_http_message* hm, const char* method) {
    return mg_strcmp(hm->method, mg_str(method)) == 0;
}

// static bool StartsWith(const std::string& s, const std::string& prefix) {
//   return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
// }

}  // namespace

bool HandleCameraApi(struct mg_connection* c, struct mg_http_message* hm, const std::string& rel_path,
                     const ApiContext& ctx) {
    if (c == nullptr || hm == nullptr || ctx.camera_manager == nullptr) return false;

    if (ctx.logger) {
        std::string method(hm->method.buf, hm->method.len);
        ctx.logger->Debug("CameraApi: " + method + " " + rel_path);
    }

    // GET /api/camera/status
    if (IsMethod(hm, "GET") && rel_path == "/camera/status") {
        bool running = ctx.camera_manager->IsRunning();
        std::string url = ctx.camera_manager->GetStreamUrl();
        bool recording = ctx.camera_manager->IsRecording();

        const std::string body =
            iotgw::core::common::json::Object({{"running", iotgw::core::common::json::Bool(running)},
                                               {"recording", iotgw::core::common::json::Bool(recording)},
                                               {"url", iotgw::core::common::json::Quote(url)}});

        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\n", body.c_str());
        return true;
    }

    // POST /api/camera/start
    if (IsMethod(hm, "POST") && rel_path == "/camera/start") {
        // 这里可以使用默认参数，或者从 body 中解析 device, resolution 等
        // 简化起见，先使用默认参数
        bool ok = ctx.camera_manager->StartStream();

        const std::string body = iotgw::core::common::json::Object(
            {{"ok", iotgw::core::common::json::Bool(ok)},
             {"message", iotgw::core::common::json::Quote(ok ? "Stream started" : "Failed to start stream")}});

        mg_http_reply(c, ok ? 200 : 500, "Content-Type: application/json\r\n", "%s\n", body.c_str());
        return true;
    }

    // POST /api/camera/stop
    if (IsMethod(hm, "POST") && rel_path == "/camera/stop") {
        bool ok = ctx.camera_manager->StopStream();

        const std::string body =
            iotgw::core::common::json::Object({{"ok", iotgw::core::common::json::Bool(ok)},
                                               {"message", iotgw::core::common::json::Quote("Stream stopped")}});

        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\n", body.c_str());
        return true;
    }

    // POST /api/camera/snapshot
    if (IsMethod(hm, "POST") && rel_path == "/camera/snapshot") {
        // 生成带时间戳的文件名
        std::time_t now = std::time(nullptr);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", std::localtime(&now));
        std::string filename = "snapshot_" + std::string(buf) + ".jpg";
        // 假设存在一个 public/snapshots 目录
        // 这里需要注意路径权限
        std::string save_path = "/tmp/" + filename;

        bool ok = ctx.camera_manager->TakeSnapshot(save_path);

        const std::string body =
            iotgw::core::common::json::Object({{"ok", iotgw::core::common::json::Bool(ok)},
                                               {"path", iotgw::core::common::json::Quote(save_path)},
                                               {"filename", iotgw::core::common::json::Quote(filename)}});

        mg_http_reply(c, ok ? 200 : 500, "Content-Type: application/json\r\n", "%s\n", body.c_str());
        return true;
    }

    // POST /api/camera/record/start
    if (IsMethod(hm, "POST") && rel_path == "/camera/record/start") {
        std::time_t now = std::time(nullptr);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", std::localtime(&now));
        std::string filename = "record_" + std::string(buf) + ".mp4";
        // 假设在开发板上的存储路径
        std::string save_path = "/root/iotgw_package/data/videos/" + filename;

        // 确保目录存在 (可以通过 shell 命令创建)
        std::system("mkdir -p /root/iotgw_package/data/videos");

        bool ok = ctx.camera_manager->StartRecording(save_path);

        const std::string body =
            iotgw::core::common::json::Object({{"ok", iotgw::core::common::json::Bool(ok)},
                                               {"path", iotgw::core::common::json::Quote(save_path)},
                                               {"filename", iotgw::core::common::json::Quote(filename)}});

        mg_http_reply(c, ok ? 200 : 500, "Content-Type: application/json\r\n", "%s\n", body.c_str());
        return true;
    }

    // POST /api/camera/record/stop
    if (IsMethod(hm, "POST") && rel_path == "/camera/record/stop") {
        bool ok = ctx.camera_manager->StopRecording();

        const std::string body =
            iotgw::core::common::json::Object({{"ok", iotgw::core::common::json::Bool(ok)},
                                               {"message", iotgw::core::common::json::Quote("Recording stopped")}});

        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s\n", body.c_str());
        return true;
    }

    return false;
}

}  // namespace api
}  // namespace web_services
}  // namespace services
}  // namespace iotgw
