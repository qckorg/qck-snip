#include "core/feature_settings.h"
#include "core/data_location.h"
#include "core/logger.h"
#include <QSettings>

namespace qck::FeatureSettings {
namespace {
struct Snapshot {
    FeaturePolicy Policy;
    int UnknownCount = 0;
    int InvalidCount = 0;
};

Snapshot ReadPolicy() {
    Snapshot Result;
    auto Configuration = GetDefaultConfiguration();
    QSettings Settings(jietuba::data_location().settings_file(), QSettings::IniFormat);
    Settings.beginGroup("features");
    for (const auto& Key : Settings.allKeys()) {
        const auto Bytes = Key.toUtf8();
        const auto Text = Settings.value(Key).toString().trimmed().toLower().toUtf8();
        const auto Applied = ApplySwitch(Configuration,
            std::string_view(Bytes.constData(), static_cast<std::size_t>(Bytes.size())),
            std::string_view(Text.constData(), static_cast<std::size_t>(Text.size())));
        if (Applied == ConfigurationResult::Unknown) ++Result.UnknownCount;
        if (Applied == ConfigurationResult::Invalid) ++Result.InvalidCount;
    }
    if (Settings.status() != QSettings::NoError) {
        Configuration.fill(false);
        ++Result.InvalidCount;
    }
    Result.Policy = FeaturePolicy(GetCompiledAvailability(), Configuration);
    return Result;
}

const Snapshot& GetSnapshot() {
    static const Snapshot Result = ReadPolicy();
    return Result;
}
}

const FeaturePolicy& GetPolicy() {
    return GetSnapshot().Policy;
}

bool IsEnabled(Feature Id) { return GetPolicy().IsEnabled(Id); }

void ReportDiagnostics() {
    // Emit only after logger setup; never expose arbitrary keys or values.
    const auto& Result = GetSnapshot();
    if (Result.UnknownCount)
        jietuba::log_warning(jietuba::LogMsg("[System:Features] Unknown feature keys ignored"), "Features");
    if (Result.InvalidCount)
        jietuba::log_warning(jietuba::LogMsg("[System:Features] Invalid feature values disabled their capabilities"), "Features");
}
}
