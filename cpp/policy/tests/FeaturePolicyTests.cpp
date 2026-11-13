#include <qck/FeaturePolicy.h>
#include <qck/Product.h>
#include <iostream>
#include <set>

namespace {
int Checks = 0;
int Failures = 0;
void Check(bool Condition, const char* Message) {
    ++Checks;
    if (!Condition) {
        ++Failures;
        std::cerr << "[Test:FeaturePolicy] " << Message << '\n';
    }
}
}

int main() {
    using namespace qck;
    FeatureValues All{};
    All.fill(true);
    FeatureValues None{};
    const FeaturePolicy Default;
    const auto Compiled = GetCompiledAvailability();
    std::set<std::string_view> Keys;
    for (const auto& Descriptor : GetFeatures()) {
        Check(Keys.insert(Descriptor.Key).second, "Feature identifiers must be unique");
        Check(GetFeature(Descriptor.Key) == Descriptor.Id, "Feature identifier must round trip");
        Check(Default.IsConfigured(Descriptor.Id), "Existing feature configuration defaults on");
        Check(Default.IsEnabled(Descriptor.Id) == Compiled[static_cast<std::size_t>(Descriptor.Id)],
              "Default enablement must honor this build's availability");
        Check(!FeaturePolicy(None, All).IsEnabled(Descriptor.Id), "Config cannot reenable an unavailable capability");
        Check(!FeaturePolicy(All, None).IsEnabled(Descriptor.Id), "Config must disable an available capability");
    }
    Check(!GetFeature("ocr.enable"), "Unknown identifier must not select a capability");
    Check(!GetFeature(""), "Empty identifier must not select a capability");
    Check(ParseSwitch("true") == true, "true must parse");
    Check(ParseSwitch("false") == false, "false must parse");
    Check(ParseSwitch("1") == true, "1 must parse");
    Check(ParseSwitch("0") == false, "0 must parse");
    for (const auto Value : {"", "yes", "false trailing", "2", "null"})
        Check(!ParseSwitch(Value), "Malformed switch must be rejected");
    Check(!Default.IsEnabled(Feature::Count), "Sentinel must fail closed");
    Check(!Default.IsEnabled(static_cast<Feature>(-1)), "Invalid enum must fail closed");
    for (const bool Ocr : {false, true}) {
        for (const bool Translation : {false, true}) {
            auto Configuration = All;
            Configuration[static_cast<std::size_t>(Feature::Ocr)] = Ocr;
            Configuration[static_cast<std::size_t>(Feature::Translation)] = Translation;
            Check(FeaturePolicy(All, Configuration).CanTranslateImage() == (Ocr && Translation),
                  "Image translation requires both capabilities");
        }
    }
    Check(std::string_view(Product::Name) == "qck-snip", "Fork product identity must be isolated");
    auto Configuration = All;
    Check(ApplySwitch(Configuration, "ocr.enabled", "false") == ConfigurationResult::Applied, "Valid config must apply");
    Check(!FeaturePolicy(All, Configuration).IsEnabled(Feature::Ocr), "A configured OCR disable must reach effective policy");
    Check(FeaturePolicy(All, Configuration).IsEnabled(Feature::Translation), "Disabling OCR must preserve text translation");
    Check(!FeaturePolicy(All, Configuration).CanTranslateImage(), "Configured OCR disable must stop image translation");
    const auto Before = Configuration;
    Check(ApplySwitch(Configuration, "unknown.enabled", "true") == ConfigurationResult::Unknown, "Unknown config must be diagnosed");
    Check(Configuration == Before, "Unknown config must not mutate other capabilities");
    Check(ApplySwitch(Configuration, "translation.enabled", "yes") == ConfigurationResult::Invalid, "Invalid config must be diagnosed");
    Check(!FeaturePolicy(All, Configuration).IsEnabled(Feature::Translation), "Invalid config must fail closed");
    Check(FeaturePolicy(All, Configuration).IsEnabled(Feature::ClipboardHistory), "Invalid config must not disable unrelated history");
    ApplySwitch(Configuration, "ocr.enabled", "true");
    Check(!FeaturePolicy(None, Configuration).IsEnabled(Feature::Ocr), "Parsed config cannot override an unavailable build");
    std::cout << "[Test:FeaturePolicy] " << Checks << " checks, " << Failures << " failures\n";
    return Failures == 0 ? 0 : 1;
}
