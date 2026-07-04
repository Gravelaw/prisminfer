#pragma once

namespace prisminfer::event {

inline constexpr const char* RunStart = "run_start";
inline constexpr const char* ConfigValidated = "config_validated";
inline constexpr const char* FailedClosed = "failed_closed";
inline constexpr const char* TelemetryProbe = "telemetry_probe";
inline constexpr const char* CudaContextProbe = "cuda_context_probe";
inline constexpr const char* CapSemanticsResolved = "cap_semantics_resolved";
inline constexpr const char* HostPrepare = "host_prepare";
inline constexpr const char* CapCertificationResult = "cap_certification_result";
inline constexpr const char* Completed = "completed";
inline constexpr const char* RunEnd = "run_end";

}  // namespace prisminfer::event

