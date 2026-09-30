# Local LLM Prototype / Benchmark Lab

This directory is an offline experiment. It is not linked into the GT Lab
Editor, QRC, application bundle, provider, UI, or packaging.

The benchmark compares ungrounded and GT Lab-grounded prompts for Phi-4-mini
Instruct and Gemma 3 4B Instruct through a locally controlled `llama.cpp`
runtime. Model weights, runtime binaries, caches, and raw temporary output are
ignored and must never be committed.

## Reproducible workflow

1. Review `model_manifest.json` and the referenced licenses/model cards.
2. Place the `llama-cli` binary under `runtime/llama.cpp/`.
3. Place the exact GGUF files under `models/`.
4. Run `python3 tools/benchmark.py --model phi... --model-path ...`.
5. Review `results/<model>/human_review.md`; do not infer a winner from scores.

The builder sends only relevant facts, documentation summaries, tone guidance,
and the approved compact patch fixture. It never sends source paths, usernames,
Git data, full manuals, SysEx, or model paths.
