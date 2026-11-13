#pragma once
#include <qck/FeaturePolicy.h>

namespace qck::FeatureSettings {
// First resolved on the bootstrap thread; immutable until the next application start.
const FeaturePolicy& GetPolicy();
bool IsEnabled(Feature Id);
void ReportDiagnostics();
}
