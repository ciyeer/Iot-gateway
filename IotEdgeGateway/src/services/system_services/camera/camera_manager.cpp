#include "services/system_services/camera/camera_manager.hpp"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <thread>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "core/common/logger/logger.hpp"

namespace iotgw {
namespace services {
namespace system_services {
namespace camera {

CameraManager::CameraManager() {}

CameraManager::~CameraManager() {
  StopStream();
}

bool CameraManager::StartStream(const std::string& device, const std::string& resolution, int fps, int port) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (running_ && CheckProcess()) {
    if (logger_) logger_->Info("Camera stream is already running");
    return true;
  }

  // 确保之前的进程已清理
  KillProcess();

  pid_t pid = fork();
  if (pid < 0) {
    if (logger_) logger_->Error("Failed to fork process for camera");
    return false;
  }

  if (pid == 0) {
    // 子进程
    // 构建 mjpg_streamer 命令
    // mjpg_streamer -i "input_uvc.so -d /dev/video0 -r 640x480 -f 30" -o "output_http.so -p 8081 -w /usr/local/share/mjpg-streamer/www"
    
    // 注意：实际路径可能需要根据系统环境调整
    // 这里假设 mjpg_streamer 在 PATH 中，插件在标准路径或 LD_LIBRARY_PATH 中
    
    std::string input_args = "input_uvc.so -d " + device + " -r " + resolution + " -f " + std::to_string(fps);
    std::string output_args = "output_http.so -p " + std::to_string(port) + " -w /usr/local/share/mjpg-streamer/www";
    
    // 为了简化，这里尝试直接 execvp
    // 注意：execvp 需要 char* 数组
    // 为避免复杂的参数解析，这里使用 shell 方式启动可能更稳健，但为了更好地控制 PID，exec 是首选
    // 让我们尝试直接调用 mjpg_streamer
    
    const char* cmd = "mjpg_streamer";
    // 构造参数数组比较繁琐，这里简化处理，使用 execl 调用 sh -c
    // 这样可以使用完整的命令行字符串，虽然多了一层 shell，但兼容性更好
    std::string full_cmd = cmd + std::string(" -i \"") + input_args + "\" -o \"" + output_args + "\"";
    
    // 重定向输出到 /dev/null 以避免污染日志
    int dev_null = open("/dev/null", O_WRONLY);
    if (dev_null != -1) {
      dup2(dev_null, STDOUT_FILENO);
      dup2(dev_null, STDERR_FILENO);
      close(dev_null);
    }

    execl("/bin/sh", "sh", "-c", full_cmd.c_str(), nullptr);
    
    // 如果执行到这里，说明失败了
    exit(127);
  } else {
    // 父进程
    pid_ = pid;
    running_ = true;
    port_ = port;
    current_stream_url_ = "http://localhost:" + std::to_string(port) + "/?action=stream";
    
    // 等待一小段时间让子进程启动
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    if (CheckProcess()) {
      if (logger_) logger_->Info("Camera stream started with PID: " + std::to_string(pid));
      return true;
    } else {
      if (logger_) logger_->Error("Camera stream process exited immediately");
      running_ = false;
      pid_ = -1;
      return false;
    }
  }
}

bool CameraManager::StopStream() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!running_) return true;

  KillProcess();
  running_ = false;
  pid_ = -1;
  current_stream_url_.clear();
  
  if (logger_) logger_->Info("Camera stream stopped");
  return true;
}

bool CameraManager::IsRunning() const {
  // const_cast 用于在 const 函数中调用非 const 的 CheckProcess (虽然 CheckProcess 逻辑上不应修改状态，但为了线程安全可能需要锁)
  // 这里简化处理，直接返回 running_ 标志，配合定期检查
  return running_; 
}

std::string CameraManager::GetStreamUrl() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return current_stream_url_;
}

bool CameraManager::TakeSnapshot(const std::string& save_path) {
  // 使用 wget 或 curl 从 mjpg-streamer 获取快照
  // URL: http://localhost:port/?action=snapshot
  
  if (!running_) return false;
  
  std::string url = "http://localhost:" + std::to_string(port_) + "/?action=snapshot";
  std::string cmd = "wget -q -O " + save_path + " \"" + url + "\"";
  
  int ret = std::system(cmd.c_str());
  return (ret == 0);
}

bool CameraManager::CheckProcess() {
  if (pid_ <= 0) return false;
  // 发送信号 0 检查进程是否存在
  return (kill(pid_, 0) == 0);
}

void CameraManager::KillProcess() {
  if (pid_ > 0) {
    kill(pid_, SIGTERM);
    
    // 等待进程退出
    int status;
    int ret = waitpid(pid_, &status, WNOHANG);
    
    if (ret == 0) {
      // 还在运行，强制杀死
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      kill(pid_, SIGKILL);
      waitpid(pid_, &status, 0);
    }
  }
}

}  // namespace camera
}  // namespace system_services
}  // namespace services
}  // namespace iotgw
