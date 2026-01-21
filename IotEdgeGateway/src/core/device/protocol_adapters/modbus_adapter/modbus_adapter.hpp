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
namespace modbus {

class ModbusRtuAdapter : public iotgw::core::device::protocol_adapters::AdapterBase {
public:
  struct Options {
    std::string port;
    std::uint32_t baudrate = 9600;
    std::uint8_t data_bits = 8;
    std::uint8_t stop_bits = 1;
    char parity = 'N';
  };

  explicit ModbusRtuAdapter(Options opt, std::shared_ptr<iotgw::core::common::log::Logger> logger);

  std::string Name() const override;
  bool Start() override;
  void Stop() override;

private:
  Options opt_;
  std::shared_ptr<iotgw::core::common::log::Logger> logger_;
};

class ModbusTcpAdapter : public iotgw::core::device::protocol_adapters::AdapterBase {
public:
  struct Options {
    std::string host;
    std::uint16_t port = 502;
  };

  explicit ModbusTcpAdapter(Options opt, std::shared_ptr<iotgw::core::common::log::Logger> logger);

  std::string Name() const override;
  bool Start() override;
  void Stop() override;

private:
  Options opt_;
  std::shared_ptr<iotgw::core::common::log::Logger> logger_;
};

}  // namespace modbus
}  // namespace protocol_adapters
}  // namespace device
}  // namespace core
}  // namespace iotgw
