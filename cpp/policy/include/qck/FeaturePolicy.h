#pragma once
#include <array>
#include <optional>
#include <string_view>

namespace qck {
enum class Feature { Ocr, Translation, ClipboardHistory, AdminStartup, Updates, Count };
inline constexpr auto FeatureCount = static_cast<std::size_t>(Feature::Count);
using FeatureValues = std::array<bool, FeatureCount>;

struct FeatureDescriptor {
    Feature Id;
    std::string_view Key;
    bool DefaultEnabled;
};

// This policy owns permission to initialize a service, never its runtime readiness.
const std::array<FeatureDescriptor, FeatureCount>& GetFeatures();
std::optional<Feature> GetFeature(std::string_view Key);
std::optional<bool> ParseSwitch(std::string_view Value);
enum class ConfigurationResult { Applied, Unknown, Invalid };
ConfigurationResult ApplySwitch(FeatureValues& Configuration, std::string_view Key, std::string_view Value);
FeatureValues GetCompiledAvailability();
FeatureValues GetDefaultConfiguration();

class FeaturePolicy {
public:
    FeaturePolicy(FeatureValues Availability = GetCompiledAvailability(),
                  FeatureValues Configuration = GetDefaultConfiguration());
    bool IsAvailable(Feature Id) const;
    bool IsConfigured(Feature Id) const;
    bool IsEnabled(Feature Id) const;
    bool CanTranslateImage() const;
private:
    FeatureValues Availability;
    FeatureValues Configuration;
};
}
