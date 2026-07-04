cmake_minimum_required(VERSION 3.24)

project(PrismInfer
  VERSION 0.1.0
  DESCRIPTION "Low-VRAM inference governor and telemetry harness"
  LANGUAGES CXX)

option(PRISMINFER_ENABLE_CUDA_PROBE "Enable CUDA-specific probing code" OFF)
option(PRISMINFER_ENABLE_LLAMA_BACKEND "Enable the llama.cpp backend adapter surface" OFF)

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
  src/kv/kv_cache_ledger.cpp
  src/kv/kv_compression_policy.cpp
  src/quality/quality_gate.cpp
  src/governor/allocator_tracker.cpp
  src/governor/backend_memory_observer.cpp
  src/governor/gpu_cap_policy.cpp
  src/governor/host_memory_tracker.cpp
  src/governor/memory_ledger.cpp
  src/governor/memory_cap.cpp
  src/governor/cuda_context_probe.cpp
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
    $<$<BOOL:${PRISMINFER_ENABLE_CUDA_PROBE}>:PRISMINFER_ENABLE_CUDA_PROBE=1>
    $<$<BOOL:${PRISMINFER_ENABLE_LLAMA_BACKEND}>:PRISMINFER_ENABLE_LLAMA_BACKEND=1>)

if(PRISMINFER_ENABLE_CUDA_PROBE)
  target_link_libraries(prisminfer_core PUBLIC CUDA::cudart ${PRISMINFER_NVML_LIBRARY})
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
