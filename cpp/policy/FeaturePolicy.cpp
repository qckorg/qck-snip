#include <qck/FeaturePolicy.h>
#include <qck/FeatureConfig.h>

namespace qck {
namespace {
constexpr std::array<FeatureDescriptor, FeatureCount> Features{{
    {Feature::Ocr, "ocr.enabled", true},
    {Feature::Translation, "translation.enabled", true},
    {Feature::ClipboardHistory, "clipboard.history", true},
    {Feature::AdminStartup, "admin.startup", true},
    {Feature::Updates, "updates.enabled", true},
}};
}

const std::array<FeatureDescriptor, FeatureCount>& GetFeatures() { return Features; }

std::optional<Feature> GetFeature(std::string_view Key) {
    for (const auto& Descriptor : Features)
        if (Descriptor.Key == Key) return Descriptor.Id;
    return std::nullopt;
}

std::optional<bool> ParseSwitch(std::string_view Value) {
    if (Value == "true" || Value == "1") return true;
    if (Value == "false" || Value == "0") return false;
    return std::nullopt;
}

FeatureValues GetCompiledAvailability() {
    return {QCK_ENABLE_OCR != 0, QCK_ENABLE_TRANSLATION != 0,
            QCK_ENABLE_CLIPBOARD_HISTORY != 0, QCK_ENABLE_ADMIN_STARTUP != 0,
            QCK_ENABLE_UPDATES != 0};
}

ConfigurationResult ApplySwitch(FeatureValues& Configuration, std::string_view Key, std::string_view Value) {
    const auto Id = GetFeature(Key);
    if (!Id) return ConfigurationResult::Unknown;
    const auto Enabled = ParseSwitch(Value);
    Configuration[static_cast<std::size_t>(*Id)] = Enabled.value_or(false);
    return Enabled ? ConfigurationResult::Applied : ConfigurationResult::Invalid;
}

FeatureValues GetDefaultConfiguration() {
    FeatureValues Values{};
    for (const auto& Descriptor : Features)
        Values[static_cast<std::size_t>(Descriptor.Id)] = Descriptor.DefaultEnabled;
    return Values;
}

FeaturePolicy::FeaturePolicy(FeatureValues Availability, FeatureValues Configuration)
    : Availability(Availability), Configuration(Configuration) {}

bool FeaturePolicy::IsAvailable(Feature Id) const {
    const auto Index = static_cast<std::size_t>(Id);
    return Index < FeatureCount && Availability[Index];
}

bool FeaturePolicy::IsConfigured(Feature Id) const {
    const auto Index = static_cast<std::size_t>(Id);
    return Index < FeatureCount && Configuration[Index];
}

bool FeaturePolicy::IsEnabled(Feature Id) const { return IsAvailable(Id) && IsConfigured(Id); }
bool FeaturePolicy::CanTranslateImage() const {
    return IsEnabled(Feature::Translation) && IsEnabled(Feature::Ocr);
}
}
