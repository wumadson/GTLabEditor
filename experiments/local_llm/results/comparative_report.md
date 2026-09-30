# GT Lab local-LLM lab — comparative report

No winner is declared. The maintainer must review the paired responses and the
rubric before any product decision.

## Environment

- Apple M1 Pro, 8 CPU cores, integrated 14-core GPU with Metal support
- 16 GB unified memory
- macOS 26.6.2, arm64
- Approximately 14 GiB free before the downloads
- llama.cpp 0.5.0-dev build 11265 (`94a0ae3e7`), isolated prebuilt arm64 runtime

## Candidates and performance

| Model | Quantization | File | Load | Mean first token | Mean generation | Peak server RSS |
|---|---:|---:|---:|---:|---:|---:|
| Phi-4-mini-instruct 3.8B | Q3_K_M | 2,117,532,992 B | 1.046 s warm-cache | 0.335 s | 32.97 tok/s | 4,743,020,544 B |
| Gemma 3 4B Instruct | Q4_K_M | 2,489,757,856 B | 3.106 s | 0.755 s | 34.92 tok/s | 5,467,897,856 B |

The earlier Phi smoke test measured an 8.111 s colder load, 0.713 s first token,
38.38 tok/s, and 3.78 GB peak RSS. Load numbers are therefore sensitive to OS
file cache and should not be treated as hardware guarantees.

Both full runs used temperature 0.2, a 4096-token context window, and up to 128
generated tokens. Mean grounded context was approximately 1,990 characters / 497
rough tokens. Each model produced 170 records: 85 grounded and 85 ungrounded.

## Grounding and hallucination observations

Automated checks flagged three Phi and four Gemma responses, all ungrounded.
They attempted generic Wi-Fi/Bluetooth/Cloud Reverb/Shimmer answers. The same
grounded cases produced zero automatic flags and correctly rejected unsupported
GT-10 features.

Grounding did not eliminate all factual errors. A targeted human pass found at
least three hard-fail candidates for Phi and seven for Gemma:

- Phi incorrectly attached the fixture's 9 s / 100 ms values to Ambience during
  the Ambience-versus-Modulate comparison.
- Phi follow-ups suggested adding an external/extra distortion pedal instead of
  reasoning cleanly about the fixture's existing, disabled Blues OD block.
- Gemma's artist-style answer invented an 800 Hz low-pass prescription despite
  the explicit no-cloning constraint.
- Gemma repeatedly treated PREAMP Gain 55 as “55 dB” and suggested invented
  “65 dB/70 dB” values.
- Gemma described the disabled Blues OD as if it were contributing to the patch.

These are minimum observed counts, not final scores; every response remains open
for human review.

## Language, tone reasoning, and follow-up

Phi was generally direct but showed Portuguese quality problems (“verificateis”,
“micrófono”, “basses”), occasional device/context drift, and awkward follow-up
phrasing. It often used the supplied facts but sometimes merged facts belonging
to different algorithms.

Gemma was usually more fluent and structured in pt-BR, but confidently added
unsupported specifics. Its follow-ups connected turns more naturally, yet the
invented units/ranges and failure to preserve the OD/DS enabled state are severe.

Both models explained likely cause/effect better than the deterministic baseline
on many tone questions. Neither is safe enough to become technical source of
truth. The deterministic current provider remains stronger for concise,
traceable anti-hallucination answers.

## Current Local comparison

`results/current-local/responses.jsonl` contains a representative sample backed
by current source/regression expectations. A full corpus export was intentionally
not added because that would require a production provider/build-target change.
The existing Local Knowledge regression remains 40 assertions / 0 failures.

## Runtime and licensing

llama.cpp is MIT-licensed and supports the controlled GGUF/Metal path used here,
with Windows x64 builds available for future evaluation.

Phi-4-mini-instruct is published by Microsoft under MIT. The selected GGUF is a
third-party conversion pinned in the manifest; redistribution would need to
preserve the MIT copyright/license notice and should verify conversion provenance.

Gemma 3 4B is governed by the Gemma Terms of Use, not MIT/Apache. Redistribution
requires the agreement/use-restriction notices, a copy of the agreement, and the
specified Notice text. This report is technical, not legal advice. No weights are
redistributed by this lab.

## Optional future model-download proposal (not implemented)

An eventual optional architecture could use a signed supported-model manifest
containing model id, publisher, license URL, file size, SHA-256, runtime minimum,
and download URL. The Editor would explicitly ask the user before download,
check free space, support resumable download, verify SHA-256 before activation,
store models under application data rather than the bundle, permit uninstall,
and keep offline use after verification. Updates would be opt-in and preserve the
previous verified model until the new one passes validation.

Important risks include several-GB downloads, disk pressure, unified-memory
contention, license acceptance/notice obligations, model supply-chain integrity,
latency variance, prompt-injection/context leakage, and factual hallucination.
None of this design is implemented in the product.
