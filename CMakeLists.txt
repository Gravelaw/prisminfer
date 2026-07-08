cmake_minimum_required(VERSION 3.24)

project(PrismInfer
  VERSION 0.1.0
  DESCRIPTION "Low-VRAM inference governor and telemetry harness"
  LANGUAGES CXX)

option(PRISMINFER_ENABLE_CUDA_PROBE "Enable CUDA-specific probing code" OFF)
option(PRISMINFER_ENABLE_LLAMA_BACKEND "Enable the llama.cpp backend adapter surface" OFF)
option(PRISMINFER_ENABLE_CUDA_KERNELS "Enable guarded CUDA kernel prototypes" OFF)
option(PRISMINFER_ENABLE_CUBLASLT_BASELINE "Enable cuBLASLt baseline discovery for kernel work" ON)
option(PRISMINFER_ENABLE_CUTLASS_BASELINE "Enable CUTLASS baseline integration for kernel work" OFF)
set(PRISMINFER_CUDA_KERNEL_ARCHS "" CACHE STRING "Explicit CMAKE_CUDA_ARCHITECTURES list for CUDA kernel prototypes")

if(PRISMINFER_ENABLE_CUDA_PROBE)
  include(CheckLanguage)
  check_language(CUDA)
  if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    set(PRISMINFER_CUDA_LANGUAGE_ENABLED ON)
  else()
    set(PRISMINFER_CUDA_LANGUAGE_ENABLED OFF)
    message(WARNING
      "CUDA language toolset was not available for this generator; "
      "falling back to CUDAToolkit runtime linking for probe support.")
  endif()
  find_package(CUDAToolkit REQUIRED)
  find_library(PRISMINFER_NVML_LIBRARY
    NAMES nvml nvidia-ml
    PATHS
      "${CUDAToolkit_LIBRARY_DIR}"
      "$ENV{CUDA_PATH}/lib/x64"
    REQUIRED)
endif()

if(PRISMINFER_ENABLE_CUDA_KERNELS)
  if(PRISMINFER_CUDA_KERNEL_ARCHS STREQUAL "")
    message(FATAL_ERROR
      "PRISMINFER_ENABLE_CUDA_KERNELS requires PRISMINFER_CUDA_KERNEL_ARCHS")
  endif()
  enable_language(CUDA)
  set(CMAKE_CUDA_ARCHITECTURES "${PRISMINFER_CUDA_KERNEL_ARCHS}")
  find_package(CUDAToolkit REQUIRED)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(prisminfer_core
  src/config.cpp
  src/errors.cpp
  src/backend/fake_backend.cpp
  src/backend/llama_backend.cpp
  src/backend/null_backend.cpp
  src/benchmark/manifest_writer.cpp
  src/benchmark/benchmark_comparator.cpp
  src/benchmark/kernel_evidence.cpp
  src/benchmark/flat_json.cpp
  src/benchmark/kernel_benchmark_manifest_schema.cpp
  src/benchmark/kernel_benchmark_manifest.cpp
  src/benchmark/transfer_metrics.cpp
  src/kernels/q4_decode_gemv.cpp
  src/planner/hybrid_plan.cpp
  src/kv/kv_cache_ledger.cpp
  src/kv/kv_compression_policy.cpp
  src/quality/quality_gate.cpp
  src/governor/allocator_tracker.cpp
  src/governor/backend_memory_observer.cpp
  src/governor/gpu_cap_policy.cpp
  src/governor/host_memory_tracker.cpp
  src/governor/memory_ledger.cpp
  src/governor/memory_cap.cpp
  src/governor/claim_taxonomy.cpp
  src/governor/profitability_policy.cpp
  src/governor/usability_policy.cpp
  src/offload/offload_planner.cpp
  src/governor/cuda_context_probe.cpp
  src/validation/evidence_bundle.cpp
  src/validation/model_sidecar.cpp
  src/telemetry/telemetry_jsonl.cpp
  src/telemetry/telemetry_clock.cpp)

target_include_directories(prisminfer_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_definitions(prisminfer_core
  PUBLIC
    PRISMINFER_CMAKE_VERSION="${CMAKE_VERSION}"
    PRISMINFER_VERSION="${PROJECT_VERSION}"
    PRISMINFER_BUILD_CONFIG="$<CONFIG>"
    PRISMINFER_CUDA_KERNEL_ARCHS="${PRISMINFER_CUDA_KERNEL_ARCHS}"
    $<$<BOOL:${PRISMINFER_ENABLE_CUBLASLT_BASELINE}>:PRISMINFER_ENABLE_CUBLASLT_BASELINE=1>
    $<$<BOOL:${PRISMINFER_ENABLE_CUTLASS_BASELINE}>:PRISMINFER_ENABLE_CUTLASS_BASELINE=1>
    $<$<BOOL:${PRISMINFER_ENABLE_CUDA_PROBE}>:PRISMINFER_ENABLE_CUDA_PROBE=1>
    $<$<BOOL:${PRISMINFER_ENABLE_LLAMA_BACKEND}>:PRISMINFER_ENABLE_LLAMA_BACKEND=1>
    $<$<BOOL:${PRISMINFER_ENABLE_CUDA_KERNELS}>:PRISMINFER_ENABLE_CUDA_KERNELS=1>)

if(PRISMINFER_ENABLE_CUDA_PROBE)
  target_link_libraries(prisminfer_core PUBLIC CUDA::cudart ${PRISMINFER_NVML_LIBRARY})
endif()

if(PRISMINFER_ENABLE_CUDA_KERNELS)
  add_library(prisminfer_cuda_kernels STATIC
    src/kernels/cuda/q4_decode_gemv.cu)
  target_include_directories(prisminfer_cuda_kernels
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(prisminfer_cuda_kernels PUBLIC CUDA::cudart)
endif()

if(WIN32)
  target_link_libraries(prisminfer_core PUBLIC psapi)
endif()

if(MSVC)
  target_compile_options(prisminfer_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(prisminfer_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(prism-probe tools/prism-probe/main.cpp)
target_link_libraries(prism-probe PRIVATE prisminfer_core)

add_executable(prism-validate-lifecycle tools/prism-validate-lifecycle/main.cpp)
target_link_libraries(prism-validate-lifecycle PRIVATE prisminfer_core)

add_executable(prism-quality tools/prism-quality/main.cpp)
target_link_libraries(prism-quality PRIVATE prisminfer_core)

add_executable(prism-plan-90b tools/prism-plan-90b/main.cpp)
target_link_libraries(prism-plan-90b PRIVATE prisminfer_core)

add_executable(prism-validate-claim tools/prism-validate-claim/main.cpp)
target_link_libraries(prism-validate-claim PRIVATE prisminfer_core)

add_executable(prism-compare-benchmark tools/prism-compare-benchmark/main.cpp)
target_link_libraries(prism-compare-benchmark PRIVATE prisminfer_core)

enable_testing()

add_executable(test_config tests/unit/test_config.cpp)
target_link_libraries(test_config PRIVATE prisminfer_core)
add_test(NAME test_config COMMAND test_config)

add_executable(test_cap_semantics tests/unit/test_cap_semantics.cpp)
target_link_libraries(test_cap_semantics PRIVATE prisminfer_core)
add_test(NAME test_cap_semantics COMMAND test_cap_semantics)

add_executable(test_allocator_tracker tests/unit/test_allocator_tracker.cpp)
target_link_libraries(test_allocator_tracker PRIVATE prisminfer_core)
add_test(NAME test_allocator_tracker COMMAND test_allocator_tracker)

add_executable(test_telemetry_jsonl tests/unit/test_telemetry_jsonl.cpp)
target_link_libraries(test_telemetry_jsonl PRIVATE prisminfer_core)
add_test(NAME test_telemetry_jsonl COMMAND test_telemetry_jsonl)

add_executable(test_manifest_writer tests/unit/test_manifest_writer.cpp)
target_link_libraries(test_manifest_writer PRIVATE prisminfer_core)
add_test(NAME test_manifest_writer COMMAND test_manifest_writer)

add_executable(test_lifecycle_validator tests/unit/test_lifecycle_validator.cpp)
target_link_libraries(test_lifecycle_validator PRIVATE prisminfer_core)
add_test(NAME test_lifecycle_validator COMMAND test_lifecycle_validator)

add_executable(test_model_sidecar tests/unit/test_model_sidecar.cpp)
target_link_libraries(test_model_sidecar PRIVATE prisminfer_core)
add_test(NAME test_model_sidecar COMMAND test_model_sidecar)

add_executable(test_backend_adapter tests/unit/test_backend_adapter.cpp)
target_link_libraries(test_backend_adapter PRIVATE prisminfer_core)
add_test(NAME test_backend_adapter COMMAND test_backend_adapter)

add_executable(test_host_memory_tracker tests/unit/test_host_memory_tracker.cpp)
target_link_libraries(test_host_memory_tracker PRIVATE prisminfer_core)
add_test(NAME test_host_memory_tracker COMMAND test_host_memory_tracker)

add_executable(test_kv_cache_ledger tests/unit/test_kv_cache_ledger.cpp)
target_link_libraries(test_kv_cache_ledger PRIVATE prisminfer_core)
add_test(NAME test_kv_cache_ledger COMMAND test_kv_cache_ledger)

add_executable(test_quality_gate tests/unit/test_quality_gate.cpp)
target_link_libraries(test_quality_gate PRIVATE prisminfer_core)
add_test(NAME test_quality_gate COMMAND test_quality_gate)

add_executable(test_transfer_metrics tests/unit/test_transfer_metrics.cpp)
target_link_libraries(test_transfer_metrics PRIVATE prisminfer_core)
add_test(NAME test_transfer_metrics COMMAND test_transfer_metrics)

add_executable(test_profitability_policy tests/unit/test_profitability_policy.cpp)
target_link_libraries(test_profitability_policy PRIVATE prisminfer_core)
add_test(NAME test_profitability_policy COMMAND test_profitability_policy)

add_executable(test_offload_planner tests/unit/test_offload_planner.cpp)
target_link_libraries(test_offload_planner PRIVATE prisminfer_core)
add_test(NAME test_offload_planner COMMAND test_offload_planner)

add_executable(test_claim_taxonomy tests/unit/test_claim_taxonomy.cpp)
target_link_libraries(test_claim_taxonomy PRIVATE prisminfer_core)
add_test(NAME test_claim_taxonomy COMMAND test_claim_taxonomy)

add_executable(test_hybrid_plan tests/unit/test_hybrid_plan.cpp)
target_link_libraries(test_hybrid_plan PRIVATE prisminfer_core)
add_test(NAME test_hybrid_plan COMMAND test_hybrid_plan)

add_executable(test_usability_policy tests/unit/test_usability_policy.cpp)
target_link_libraries(test_usability_policy PRIVATE prisminfer_core)
add_test(NAME test_usability_policy COMMAND test_usability_policy)

add_executable(test_evidence_bundle tests/unit/test_evidence_bundle.cpp)
target_link_libraries(test_evidence_bundle PRIVATE prisminfer_core)
add_test(NAME test_evidence_bundle COMMAND test_evidence_bundle)

add_executable(test_benchmark_comparator tests/unit/test_benchmark_comparator.cpp)
target_link_libraries(test_benchmark_comparator PRIVATE prisminfer_core)
add_test(NAME test_benchmark_comparator COMMAND test_benchmark_comparator)

add_executable(test_kernel_evidence tests/unit/test_kernel_evidence.cpp)
target_link_libraries(test_kernel_evidence PRIVATE prisminfer_core)
add_test(NAME test_kernel_evidence COMMAND test_kernel_evidence)

add_executable(test_kernel_benchmark_manifest tests/unit/test_kernel_benchmark_manifest.cpp)
target_link_libraries(test_kernel_benchmark_manifest PRIVATE prisminfer_core)
add_test(NAME test_kernel_benchmark_manifest COMMAND test_kernel_benchmark_manifest)

add_executable(test_q4_decode_gemv tests/unit/test_q4_decode_gemv.cpp)
target_link_libraries(test_q4_decode_gemv PRIVATE prisminfer_core)
add_test(NAME test_q4_decode_gemv COMMAND test_q4_decode_gemv)

add_test(NAME prism_compare_benchmark_manifest_same
  COMMAND prism-compare-benchmark
    --baseline-manifest ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests/baseline-valid.json
    --candidate-manifest ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests/candidate-variant-valid.json)
add_test(NAME prism_compare_benchmark_manifest_mismatch
  COMMAND prism-compare-benchmark
    --baseline-manifest ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests/baseline-valid.json
    --candidate-manifest ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests/candidate-model-mismatch.json)
set_tests_properties(prism_compare_benchmark_manifest_mismatch PROPERTIES WILL_FAIL TRUE)
add_test(NAME prism_compare_benchmark_manifest_unknown_field
  COMMAND prism-compare-benchmark
    --baseline-manifest ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests/baseline-valid.json
    --candidate-manifest ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests/candidate-unknown-field.json)
set_tests_properties(prism_compare_benchmark_manifest_unknown_field PROPERTIES WILL_FAIL TRUE)
