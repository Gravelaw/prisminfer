# llama.cpp Static CPU Executable Receipt

This receipt records the executable approved by
`llama.cpp-executable-approval.json`. It is an identity and build-provenance
record, not authorization for a model-backed or CUDA run.

## Source and build

- Source commit: `ef2d770117db45b05aa7ecd1b0acca36370c5470`
- Generator: Visual Studio 18 2026, x64
- Configuration: Release
- `BUILD_SHARED_LIBS=OFF`
- `GGML_BACKEND_DL=OFF`
- `GGML_CUDA=OFF`
- `LLAMA_OPENSSL=OFF`
- `LLAMA_BUILD_SERVER=ON`
- `LLAMA_BUILD_TESTS=OFF`
- `LLAMA_BUILD_EXAMPLES=ON`

The source worktree was clean at the recorded commit. The resulting executable
is workspace-relative at
`tools/llama.cpp/ef2d770117db45b05aa7ecd1b0acca36370c5470/build-prisminfer-static-vs2026/bin/Release/llama.exe`.

## Identity

- `llama.exe --version`: `b9871-ef2d77011`
- File size: `17618944` bytes
- SHA-256: `f31fd598048c5773c304ef6d8b3225569091399de17cb9f34d0ab385519e0be8`

## PE dependency inventory

Visual Studio 18 Build Tools `dumpbin /dependents` reported only these Windows
and Microsoft runtime imports:

```text
KERNEL32.dll
SHELL32.dll
ADVAPI32.dll
MSVCP140.dll
WS2_32.dll
VCOMP140.DLL
VCRUNTIME140.dll
VCRUNTIME140_1.dll
api-ms-win-crt-stdio-l1-1-0.dll
api-ms-win-crt-environment-l1-1-0.dll
api-ms-win-crt-string-l1-1-0.dll
api-ms-win-crt-heap-l1-1-0.dll
api-ms-win-crt-runtime-l1-1-0.dll
api-ms-win-crt-convert-l1-1-0.dll
api-ms-win-crt-locale-l1-1-0.dll
api-ms-win-crt-math-l1-1-0.dll
api-ms-win-crt-time-l1-1-0.dll
api-ms-win-crt-filesystem-l1-1-0.dll
api-ms-win-crt-utility-l1-1-0.dll
```

No DLL files are present beside the approved executable. The worker additionally
applies the child-bound System32-preferred image-load mitigation.
