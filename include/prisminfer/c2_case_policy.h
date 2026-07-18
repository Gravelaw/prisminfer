#pragma once

#include <string>

#include "prisminfer/gpu_admission_session.h"
#include "prisminfer/native_worker.h"

namespace prisminfer {

// Pure hosted-testable classification for the four attended hardware cases.
// It never launches a worker or grants promotion/C2 credit.
[[nodiscard]] bool c2_case_result_matches_contract(
    const std::string& case_name, const NativeWorkerResult& result,
    GpuAdmissionSessionState cleanup, bool output_removed);

// C2 requires the native Windows WDDM process counter. NVML remains a valid
// general-runtime fallback but cannot satisfy this clearance evidence class.
[[nodiscard]] ProcessDeviceMemorySample require_c2_wddm_process_sample(
    ProcessDeviceMemorySample sample);

}  // namespace prisminfer
