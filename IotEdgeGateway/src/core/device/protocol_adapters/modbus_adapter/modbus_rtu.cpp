#include "core/device/protocol_adapters/modbus_adapter/modbus_adapter.hpp"

#include <utility>

namespace iotgw {
namespace core {
namespace device {
namespace protocol_adapters {
namespace modbus {

ModbusRtuAdapter::ModbusRtuAdapter(Options opt, std::shared_ptr<iotgw::core::common::log::Logger> logger)
    : opt_(std::move(opt)), logger_(std::move(logger)) {}

std::string ModbusRtuAdapter::Name() const { return "modbus_rtu"; }

bool ModbusRtuAdapter::Start() { return false; }

void ModbusRtuAdapter::Stop() {}

}  // namespace modbus
}  // namespace protocol_adapters
}  // namespace device
}  // namespace core
}  // namespace iotgw
