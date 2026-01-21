#pragma once

#include <algorithm>
#include <cctype>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace iotgw {
namespace core {
namespace control {
namespace rule_engine {

struct Condition {
  std::string sensor_id;
  std::string op;
  double value = 0.0;
};

struct Action {
  	std::string type;
  	std::string actuator_id;
  	std::string value;
  	std::string level;
  	std::string message;
};

struct Rule {
  std::string id;
  std::string category;
  bool enabled = true;
  Condition when;
  std::vector<Action> then;
};

class RuleEngine {
public:
  void Clear() { rules_.clear(); }
  const std::vector<Rule>& Rules() const { return rules_; }

  bool SetEnabled(const std::string& rule_id, bool enabled) {
    for (auto& r : rules_) {
      if (r.id == rule_id) {
        r.enabled = enabled;
        return true;
      }
    }
    return false;
  }

  bool HasRule(const std::string& rule_id) const {
    return std::any_of(rules_.begin(), rules_.end(), [&](const Rule& r) { return r.id == rule_id; });
  }

  void AddRules(std::vector<Rule> rules) {
    for (auto& r : rules) rules_.push_back(std::move(r));
  }

  void OnSensorValue(const std::string& sensor_id, double value,
                     const std::function<void(const Rule& rule, const Action& action)>& exec) {
    for (const auto& r : rules_) {
      if (!r.enabled) continue;
      if (r.when.sensor_id != sensor_id) continue;
      if (!Eval(r.when, value)) continue;
      for (const auto& a : r.then) exec(r, a);
    }
  }

private:
  static bool Eval(const Condition& c, double v);

private:
  std::vector<Rule> rules_;
};

inline bool RuleEngine::Eval(const Condition& c, double v) {
  std::string op;
  op.reserve(c.op.size());
  for (char ch : c.op) op.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  if (op == ">") return v > c.value;
  if (op == ">=") return v >= c.value;
  if (op == "<") return v < c.value;
  if (op == "<=") return v <= c.value;
  if (op == "==" || op == "=") return v == c.value;
  if (op == "!=") return v != c.value;
  return false;
}

}  // namespace rule_engine
}  // namespace control
}  // namespace core
}  // namespace iotgw

