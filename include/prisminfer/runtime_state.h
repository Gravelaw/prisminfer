#pragma once

namespace prisminfer::event {

inline constexpr const char* RunStart = "run_start";
inline constexpr const char* ConfigValidated = "config_validated";
inline constexpr const char* FailedClosed = "failed_closed";
inline constexpr const char* TelemetryProbe = "telemetry_probe";
inline constexpr const char* ModelSidecarValidated = "model_sidecar_validated";
inline constexpr const char* BackendSelected = "backend_selected";
inline constexpr const char* DependencyPinsResolved = "dependency_pins_resolved";
inline constexpr const char* CudaContextProbe = "cuda_context_probe";
inline constexpr const char* CapSemanticsResolved = "cap_semantics_resolved";
inline constexpr const char* HostPrepare = "host_prepare";
inline constexpr const char* ModelLoadPlan = "model_load_plan";
inline constexpr const char* BackendInit = "backend_init";
inline constexpr const char* BackendWarmup = "backend_warmup";
inline constexpr const char* KvCacheProfile = "kv_cache_profile";
inline constexpr const char* PrefillStart = "prefill_start";
inline constexpr const char* PrefillEnd = "prefill_end";
inline constexpr const char* KvCacheSample = "kv_cache_sample";
inline constexpr const char* DecodeStart = "decode_start";
inline constexpr const char* DecodeToken = "decode_token";
inline constexpr const char* DecodeEnd = "decode_end";
inline constexpr const char* QualityGateResult = "quality_gate_result";
inline constexpr const char* CapCertificationResult = "cap_certification_result";
inline constexpr const char* Completed = "completed";
inline constexpr const char* RunEnd = "run_end";

}  // namespace prisminfer::event
