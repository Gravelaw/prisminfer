# Third-Party Notices and Licensing Boundaries

This file records the licensing boundaries for PrismInfer's current source
tree. It is not a release software bill of materials. Every source or binary
distribution must re-check its actual dependency graph and bundled artifacts.

## First-party PrismInfer material

PrismInfer's first-party source code, documentation, scripts, tests, schemas,
and configuration are licensed under the Apache License, Version 2.0, as
described by [`LICENSE`](LICENSE) and [`NOTICE`](NOTICE).

That license does not relicense third-party software, system libraries,
drivers, toolchains, model artifacts, datasets, or services.

## llama.cpp, GGML, and GGUF support

The repository records an external llama.cpp source baseline in
[`third_party/llama.cpp-pin.json`](third_party/llama.cpp-pin.json). No
llama.cpp source is currently vendored by that pin.

- Project: [ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp)
- Pinned baseline: `ef2d770117db45b05aa7ecd1b0acca36370c5470`
- Upstream license at that baseline: [MIT License](https://github.com/ggml-org/llama.cpp/blob/ef2d770117db45b05aa7ecd1b0acca36370c5470/LICENSE)

GGML and GGUF support referenced by PrismInfer comes from that external
runtime baseline. If llama.cpp/GGML source or binaries are vendored or
distributed later, the applicable upstream copyright and license notices must
be included in the distribution.

## Optional platform and toolchain dependencies

PrismInfer can be configured against user-installed NVIDIA CUDA Runtime and
NVML components. Those components, drivers, headers, libraries, and tools are
provided under NVIDIA's applicable terms, including the
[NVIDIA Software Development Kits EULA](https://docs.nvidia.com/cuda/eula/index.html),
and are not relicensed under Apache-2.0. The current repository does not
redistribute NVIDIA binaries. Any future binary package must review the exact
redistributable components and required notices before release.

Windows SDK headers and system libraries used by Windows builds are external
platform components and are not redistributed by this source tree.

The GitHub Actions referenced by repository workflow files are development and
CI dependencies fetched by GitHub. They remain under the licenses and service
terms published by their respective owners and are not part of the PrismInfer
source distribution.

## Model and data artifacts

This repository does not include model weights, tokenizers, production GGUF
artifacts, calibration corpora, or evaluation datasets. Model families named
in research documents are candidate test cells, not bundled dependencies and
not Apache-2.0-licensed by PrismInfer.

Before acquiring, converting, calibrating, serving, or redistributing any
model or dataset, record and comply with the exact artifact's license,
acceptable-use terms, attribution requirements, access restrictions, and
redistribution conditions. Converting a model to GGUF or producing a derived
quantization does not replace the source artifact's terms.

## Distribution rule

Before adding a vendored dependency, generated artifact, model, dataset, or
binary bundle:

1. record its immutable source and version;
2. identify its governing license and redistribution obligations;
3. retain every required copyright, license, and attribution notice;
4. update this file and the release software bill of materials; and
5. block distribution when provenance or licensing is unknown.

An external component may be technically optional and still impose license
obligations when it is copied into a source or binary distribution.
