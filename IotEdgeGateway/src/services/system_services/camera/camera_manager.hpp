#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include "core/common/logger/logger.hpp"

namespace iotgw {
namespace services {
namespace system_services {
namespace camera {

class CameraManager {
public:
    CameraManager();
    ~CameraManager();

    // 禁止拷贝和赋值
    CameraManager(const CameraManager&) = delete;
    CameraManager& operator=(const CameraManager&) = delete;

    // 启动视频流
    // device: 摄像头设备路径 (例如 /dev/video0)
    // resolution: 分辨率 (例如 640x480)
    // fps: 帧率
    // port: mjpg-streamer 监听端口
    bool StartStream(const std::string& device = "/dev/video0", const std::string& resolution = "640x480", int fps = 30,
                     int port = 8081);

    // 停止视频流
    bool StopStream();

    // 获取运行状态
    bool IsRunning() const;

    // 获取当前流地址
    std::string GetStreamUrl() const;

    // 拍照 (通过 HTTP 接口抓取当前帧)
    // save_path: 保存路径
    bool TakeSnapshot(const std::string& save_path);

    // 开启/停止视频录制
    // save_path: 录像文件保存路径 (例如 /root/iotgw_package/data/videos/record_01.mp4)
    bool StartRecording(const std::string& save_path, const std::string& device = "/dev/video0");
    bool StopRecording();
    bool IsRecording() const;

private:
    // 检查进程是否存活
    bool CheckProcess();
    bool CheckRecordingProcess();

    // 终止进程
    void KillProcess();
    void KillRecordingProcess();

private:
    std::shared_ptr<iotgw::core::common::log::Logger> logger_;
    std::atomic<bool> running_{false};
    std::atomic<pid_t> pid_{-1};
    std::string current_stream_url_;

    std::atomic<bool> recording_{false};
    std::atomic<pid_t> record_pid_{-1};
    std::string current_record_path_;

    mutable std::mutex mutex_;
    mutable std::mutex record_mutex_;
    int port_{8081};
};

}  // namespace camera
}  // namespace system_services
}  // namespace services
}  // namespace iotgw
