#pragma once

#include <set>
#include <string>
#include <vector>

#include "prisminfer/kernel_benchmark_manifest.h"

namespace prisminfer {

const std::set<std::string>& kernel_manifest_allowed_keys();
const std::vector<std::string>& kernel_manifest_required_keys();
bool kernel_manifest_identity_constraints_ok(
    const KernelBenchmarkManifest& manifest);
bool kernel_manifest_optional_constraints_ok(
    const KernelBenchmarkManifest& manifest);

}  // namespace prisminfer
