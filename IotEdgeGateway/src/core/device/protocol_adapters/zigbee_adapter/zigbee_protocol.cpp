#include "core/device/protocol_adapters/zigbee_adapter/zigbee_adapter.hpp"

#include <utility>

namespace iotgw {
namespace core {
namespace device {
namespace protocol_adapters {
namespace zigbee {

ZigbeeAdapter::ZigbeeAdapter(Options opt, std::shared_ptr<iotgw::core::common::log::Logger> logger)
    : opt_(std::move(opt)), logger_(std::move(logger)) {}

std::string ZigbeeAdapter::Name() const { return "zigbee"; }

bool ZigbeeAdapter::Start() { return false; }

void ZigbeeAdapter::Stop() {}

}  // namespace zigbee
}  // namespace protocol_adapters
}  // namespace device
}  // namespace core
}  // namespace iotgw

