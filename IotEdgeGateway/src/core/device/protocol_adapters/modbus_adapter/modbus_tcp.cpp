#include "core/device/protocol_adapters/modbus_adapter/modbus_adapter.hpp"

#include <utility>

namespace iotgw {
namespace core {
namespace device {
namespace protocol_adapters {
namespace modbus {

ModbusTcpAdapter::ModbusTcpAdapter(Options opt,
                                   std::shared_ptr<iotgw::core::common::log::Logger> logger)
    : opt_(std::move(opt)), logger_(std::move(logger)) {}

std::string ModbusTcpAdapter::Name() const { return "modbus_tcp"; }

bool ModbusTcpAdapter::Start() { return false; }

void ModbusTcpAdapter::Stop() {}

}  // namespace modbus
}  // namespace protocol_adapters
}  // namespace device
}  // namespace core
}  // namespace iotgw

