#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "core/common/logger/logger.hpp"
#include "core/device/protocol_adapters/adapter_base.hpp"

namespace iotgw {
namespace core {
namespace device {
namespace protocol_adapters {
namespace zigbee {

class ZigbeeAdapter : public iotgw::core::device::protocol_adapters::AdapterBase {
public:
  struct Options {
    std::string port;
    std::uint32_t baudrate = 115200;
  };

  explicit ZigbeeAdapter(Options opt, std::shared_ptr<iotgw::core::common::log::Logger> logger);

  std::string Name() const override;
  bool Start() override;
  void Stop() override;

private:
  Options opt_;
  std::shared_ptr<iotgw::core::common::log::Logger> logger_;
};

}  // namespace zigbee
}  // namespace protocol_adapters
}  // namespace device
}  // namespace core
}  // namespace iotgw

