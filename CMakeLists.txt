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
  src/benchmark/atomic_file.cpp
  src/benchmark/benchmark_comparator.cpp
  src/benchmark/kernel_evidence.cpp
  src/benchmark/flat_json.cpp
  src/benchmark/kernel_benchmark_manifest_schema.cpp
  src/benchmark/kernel_benchmark_manifest.cpp
  src/benchmark/sha256.cpp
  src/benchmark/transfer_metrics.cpp
  src/kernels/ggml_q4_k_reference.cpp
  src/kernels/q4_decode_gemv.cpp
  src/planner/hybrid_plan.cpp
  src/kv/kv_cache_ledger.cpp
  src/kv/kv_compression_policy.cpp
  src/quality/quality_gate.cpp
  src/governor/allocator_tracker.cpp
  src/governor/admission_token.cpp
  src/governor/backend_memory_observer.cpp
  src/governor/checked_arithmetic.cpp
  src/governor/exclusive_gpu_lease.cpp
  src/governor/gpu_admission_session.cpp
  src/governor/gpu_cap_policy.cpp
  src/governor/host_admission.cpp
  src/governor/host_memory_tracker.cpp
  src/governor/memory_ledger.cpp
  src/governor/memory_cap.cpp
  src/governor/claim_taxonomy.cpp
  src/governor/profitability_policy.cpp
  src/governor/supervisor_admission.cpp
  src/governor/supervisor_watchdog.cpp
  src/governor/usability_policy.cpp
  src/offload/offload_planner.cpp
  src/governor/cuda_context_probe.cpp
  src/validation/evidence_bundle.cpp
  src/validation/model_sidecar.cpp
  src/telemetry/telemetry_jsonl.cpp
  src/telemetry/telemetry_clock.cpp
  src/telemetry/windows_evidence.cpp
  src/runtime/native_worker.cpp)

target_include_directories(prisminfer_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_definitions(prisminfer_core
  PUBLIC
    PRISMINFER_CMAKE_VERSION="${CMAKE_VERSION}"
    PRISMINFER_VERSION="${PROJECT_VERSION}"
    PRISMINFER_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
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

  add_executable(test_q4_decode_gemv_cuda
    tests/cuda/test_q4_decode_gemv_cuda.cu)
  target_link_libraries(test_q4_decode_gemv_cuda
    PRIVATE prisminfer_core prisminfer_cuda_kernels CUDA::cudart)
  add_test(NAME test_q4_decode_gemv_cuda COMMAND test_q4_decode_gemv_cuda)
  set_tests_properties(test_q4_decode_gemv_cuda PROPERTIES
    LABELS cuda_kernel
    RUN_SERIAL TRUE
    TIMEOUT 60)
endif()

if(WIN32)
  target_link_libraries(prisminfer_core PUBLIC psapi bcrypt dxgi pdh)
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

add_executable(prism-emit-benchmark tools/prism-emit-benchmark/main.cpp)
target_link_libraries(prism-emit-benchmark PRIVATE prisminfer_core)

enable_testing()

find_program(PRISMINFER_PYTHON_EXECUTABLE NAMES python3 python)
if(PRISMINFER_PYTHON_EXECUTABLE)
  add_test(
    NAME test_evidence_runner
    COMMAND ${PRISMINFER_PYTHON_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/test_evidence_runner.py)
  add_test(
    NAME test_inventory_gguf
    COMMAND ${PRISMINFER_PYTHON_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/test_inventory_gguf.py)
  add_test(
    NAME test_admit_model_artifact
    COMMAND ${PRISMINFER_PYTHON_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/test_admit_model_artifact.py)
endif()

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests/emitter-valid.json
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests/candidate-aborted.json
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests/failure-record-aborted.jsonl)
file(SHA256
     ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests/emitter-valid.json
     PRISMINFER_EMITTER_FIXTURE_SHA256)
file(SHA256
     ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests/candidate-aborted.json
     PRISMINFER_ABORTED_MANIFEST_SHA256)
file(SHA256
     ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests/failure-record-aborted.jsonl
     PRISMINFER_ABORTED_RECORD_SHA256)

add_test(
    NAME prism_emit_benchmark_binds_raw_evidence
    COMMAND prism-emit-benchmark
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests/emitter-valid.json
            $<TARGET_FILE_DIR:prism-emit-benchmark>/emitter-valid-${PRISMINFER_EMITTER_FIXTURE_SHA256}.json
            --output-root
            $<TARGET_FILE_DIR:prism-emit-benchmark>
            --evidence-root
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests
            --raw-trials
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests/raw-trials.jsonl)
add_test(
    NAME prism_emit_benchmark_rejects_raw_evidence_mismatch
    COMMAND prism-emit-benchmark
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests/emitter-valid.json
            $<TARGET_FILE_DIR:prism-emit-benchmark>/emitter-mismatch-${PRISMINFER_EMITTER_FIXTURE_SHA256}.json
            --output-root
            $<TARGET_FILE_DIR:prism-emit-benchmark>
            --evidence-root
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests
            --raw-trials
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests/raw-trials-mismatch.jsonl)
set_tests_properties(prism_emit_benchmark_rejects_raw_evidence_mismatch
                     PROPERTIES WILL_FAIL TRUE)
add_test(
    NAME prism_emit_benchmark_retains_aborted_failure
    COMMAND prism-emit-benchmark
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests/candidate-aborted.json
            $<TARGET_FILE_DIR:prism-emit-benchmark>/emitter-aborted-${PRISMINFER_ABORTED_MANIFEST_SHA256}-${PRISMINFER_ABORTED_RECORD_SHA256}.json
            --output-root
            $<TARGET_FILE_DIR:prism-emit-benchmark>
            --evidence-root
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests
            --failure-record
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests/failure-record-aborted.jsonl)
set_tests_properties(prism_emit_benchmark_retains_aborted_failure
                     PROPERTIES WILL_FAIL TRUE)
add_test(
    NAME prism_emit_benchmark_aborted_outputs_published
    COMMAND ${CMAKE_COMMAND} -E cat
            $<TARGET_FILE_DIR:prism-emit-benchmark>/emitter-aborted-${PRISMINFER_ABORTED_MANIFEST_SHA256}-${PRISMINFER_ABORTED_RECORD_SHA256}.json
            $<TARGET_FILE_DIR:prism-emit-benchmark>/emitter-aborted-${PRISMINFER_ABORTED_MANIFEST_SHA256}-${PRISMINFER_ABORTED_RECORD_SHA256}.json.sha256)
set_tests_properties(prism_emit_benchmark_aborted_outputs_published
                     PROPERTIES DEPENDS prism_emit_benchmark_retains_aborted_failure)

add_executable(test_config tests/unit/test_config.cpp)
target_link_libraries(test_config PRIVATE prisminfer_core)
add_test(NAME test_config COMMAND test_config)

add_executable(test_cap_semantics tests/unit/test_cap_semantics.cpp)
target_link_libraries(test_cap_semantics PRIVATE prisminfer_core)
add_test(NAME test_cap_semantics COMMAND test_cap_semantics)

add_executable(test_checked_arithmetic tests/unit/test_checked_arithmetic.cpp)
target_link_libraries(test_checked_arithmetic PRIVATE prisminfer_core)
add_test(NAME test_checked_arithmetic COMMAND test_checked_arithmetic)

add_executable(test_admission_token tests/unit/test_admission_token.cpp)
target_link_libraries(test_admission_token PRIVATE prisminfer_core)
add_test(NAME test_admission_token COMMAND test_admission_token)

add_executable(test_exclusive_gpu_lease tests/unit/test_exclusive_gpu_lease.cpp)
target_link_libraries(test_exclusive_gpu_lease PRIVATE prisminfer_core)
add_test(NAME test_exclusive_gpu_lease COMMAND test_exclusive_gpu_lease)

add_executable(test_gpu_admission_session
  tests/unit/test_gpu_admission_session.cpp)
target_link_libraries(test_gpu_admission_session PRIVATE prisminfer_core)
add_test(NAME test_gpu_admission_session COMMAND test_gpu_admission_session)

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

add_executable(test_host_admission tests/unit/test_host_admission.cpp)
target_link_libraries(test_host_admission PRIVATE prisminfer_core)
add_test(NAME test_host_admission COMMAND test_host_admission)

add_executable(test_supervisor_admission tests/unit/test_supervisor_admission.cpp)
target_link_libraries(test_supervisor_admission PRIVATE prisminfer_core)
add_test(NAME test_supervisor_admission COMMAND test_supervisor_admission)

add_executable(test_windows_evidence tests/unit/test_windows_evidence.cpp)
target_link_libraries(test_windows_evidence PRIVATE prisminfer_core)
add_test(NAME test_windows_evidence COMMAND test_windows_evidence)

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

add_executable(test_sha256_trusted_file tests/unit/test_sha256_trusted_file.cpp)
target_link_libraries(test_sha256_trusted_file PRIVATE prisminfer_core)
add_test(NAME test_sha256_trusted_file COMMAND test_sha256_trusted_file)

add_executable(test_q4_decode_gemv tests/unit/test_q4_decode_gemv.cpp)
target_link_libraries(test_q4_decode_gemv PRIVATE prisminfer_core)
add_test(NAME test_q4_decode_gemv COMMAND test_q4_decode_gemv)

add_executable(test_ggml_q4_k_reference tests/unit/test_ggml_q4_k_reference.cpp)
target_link_libraries(test_ggml_q4_k_reference PRIVATE prisminfer_core)
add_test(NAME test_ggml_q4_k_reference COMMAND test_ggml_q4_k_reference)

add_executable(test_real_gguf_slices tests/unit/test_real_gguf_slices.cpp)
target_link_libraries(test_real_gguf_slices PRIVATE prisminfer_core)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/test_real_gguf_slices.cpp" PRISMINFER_SLICE_TEST_SHA)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/include/prisminfer/kernels/ggml_q4_k_reference.h" PRISMINFER_SLICE_HEADER_SHA)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/src/kernels/ggml_q4_k_reference.cpp" PRISMINFER_SLICE_IMPL_SHA)
string(SHA256 PRISMINFER_REAL_SLICE_DECODER_ID
  "${PRISMINFER_SLICE_TEST_SHA}:${PRISMINFER_SLICE_HEADER_SHA}:${PRISMINFER_SLICE_IMPL_SHA}")
target_compile_definitions(test_real_gguf_slices PRIVATE
  PRISMINFER_REAL_SLICE_DECODER_ID="${PRISMINFER_REAL_SLICE_DECODER_ID}")

add_executable(test_native_worker tests/unit/test_native_worker.cpp)
target_link_libraries(test_native_worker PRIVATE prisminfer_core)
if(WIN32)
  target_link_libraries(test_native_worker PRIVATE shell32)
endif()
add_test(NAME test_native_worker COMMAND test_native_worker)

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
add_test(NAME prism_compare_benchmark_manifest_validation_cell_mismatch
  COMMAND prism-compare-benchmark
    --baseline-manifest ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests/baseline-valid.json
    --candidate-manifest ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests/candidate-validation-cell-mismatch.json)
set_tests_properties(prism_compare_benchmark_manifest_validation_cell_mismatch PROPERTIES WILL_FAIL TRUE)
add_test(NAME prism_compare_benchmark_manifest_aborted_rejected
  COMMAND prism-compare-benchmark
    --baseline-manifest ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests/baseline-valid.json
    --candidate-manifest ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/kernel-manifests/candidate-aborted.json)
set_tests_properties(prism_compare_benchmark_manifest_aborted_rejected PROPERTIES WILL_FAIL TRUE)
