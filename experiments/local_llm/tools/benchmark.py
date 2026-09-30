#!/usr/bin/env python3
"""Run the isolated GT Lab corpus against a persistent local llama.cpp server."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import threading
import time
import urllib.error
import urllib.request
from pathlib import Path

from grounded_context_builder import GroundedLLMContextBuilder


LAB = Path(__file__).resolve().parents[1]
CORPUS = json.loads((LAB / "corpus/gt10_real_questions.json").read_text(encoding="utf-8"))


def post_stream(port: int, prompt: str, temperature: float, max_tokens: int) -> dict:
    body = json.dumps({
        "model": "local",
        "messages": [{"role": "user", "content": prompt}],
        "temperature": temperature,
        "max_tokens": max_tokens,
        "seed": 42,
        "stream": True,
        "stream_options": {"include_usage": True},
    }).encode()
    request = urllib.request.Request(
        f"http://127.0.0.1:{port}/v1/chat/completions", body,
        {"Content-Type": "application/json"})
    start = time.perf_counter()
    first = None
    chunks: list[str] = []
    usage = {}
    with urllib.request.urlopen(request, timeout=300) as response:
        for raw in response:
            line = raw.decode("utf-8", "replace").strip()
            if not line.startswith("data: ") or line == "data: [DONE]":
                continue
            event = json.loads(line[6:])
            usage = event.get("usage") or usage
            choices = event.get("choices") or []
            content = choices[0].get("delta", {}).get("content", "") if choices else ""
            if content:
                first = first or time.perf_counter()
                chunks.append(content)
    end = time.perf_counter()
    completion = int(usage.get("completion_tokens") or 0)
    generation_seconds = max(0.001, end - (first or start))
    return {
        "answer": "".join(chunks).strip(),
        "firstTokenSeconds": round((first or end) - start, 4),
        "totalSeconds": round(end - start, 4),
        "completionTokens": completion,
        "tokensPerSecond": round(completion / generation_seconds, 2) if completion else None,
    }


def rss_monitor(pid: int, stop: threading.Event, peak: list[int]) -> None:
    while not stop.wait(0.1):
        try:
            value = subprocess.check_output(
                ["ps", "-o", "rss=", "-p", str(pid)], text=True).strip()
            if value:
                peak[0] = max(peak[0], int(value))
        except (subprocess.SubprocessError, ValueError, OSError):
            pass


def wait_ready(port: int, process: subprocess.Popen, timeout: int = 180) -> float:
    start = time.perf_counter()
    while time.perf_counter() - start < timeout:
        if process.poll() is not None:
            raise RuntimeError("llama-server exited before becoming ready")
        try:
            with urllib.request.urlopen(f"http://127.0.0.1:{port}/health", timeout=1) as reply:
                if reply.status == 200:
                    return time.perf_counter() - start
        except (urllib.error.URLError, TimeoutError):
            time.sleep(0.2)
    raise TimeoutError("llama-server load timeout")


def flags(answer: str) -> list[str]:
    text = answer.casefold()
    result = []
    unsupported = ("neural capture", "wi-fi", "wifi", "bluetooth", "cloud reverb", "shimmer")
    if any(term in text for term in unsupported) and not any(
            denial in text for denial in ("não", "nao", "does not", "não possui", "não tem")):
        result.append("possible_invented_capability")
    if any(term in text for term in ("ouvi seu", "escutei", "i heard")):
        result.append("claims_audio_access")
    if any(term in text for term in ("clone fiel", "cópia exata", "exact clone")):
        result.append("artist_clone_claim")
    return result


def write_review(path: Path, model: str, records: list[dict]) -> None:
    hard = sum(bool(r["automatedFlags"]) for r in records)
    lines = [
        f"# Human review — {model}", "",
        f"Responses: {len(records)}; automatically flagged: {hard}.", "",
        "Use `scoring_rubric.md`; assign 0–5 manually. No automatic winner is declared.", "",
    ]
    for record in records:
        lines += [
            f"## {record['caseId']} — {record['mode']}", "",
            f"**Question:** {record['question']}", "",
            record["answer"], "",
            f"Flags: {', '.join(record['automatedFlags']) or 'none'}", "",
            "Scores: Intent __ / Factual __ / Context __ / Tone __ / Naturalness __ / Follow-up __ / Anti-hallucination __ / pt-BR __ / Helpfulness __ / Concision __", "",
        ]
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", required=True)
    parser.add_argument("--model-path", required=True, type=Path)
    parser.add_argument("--server", required=True, type=Path)
    parser.add_argument("--port", type=int, default=18081)
    parser.add_argument("--temperature", type=float, default=0.2)
    parser.add_argument("--max-tokens", type=int, default=128)
    parser.add_argument("--limit", type=int)
    args = parser.parse_args()

    output = LAB / "results" / args.model
    output.mkdir(parents=True, exist_ok=True)
    log = (output / "server.log").open("w", encoding="utf-8")
    process = subprocess.Popen([
        str(args.server), "-m", str(args.model_path), "--host", "127.0.0.1",
        "--port", str(args.port), "-ngl", "99", "-c", "4096",
    ], stdout=log, stderr=subprocess.STDOUT, text=True)
    stop = threading.Event()
    peak = [0]
    monitor = threading.Thread(target=rss_monitor, args=(process.pid, stop, peak), daemon=True)
    monitor.start()
    builder = GroundedLLMContextBuilder()
    records: list[dict] = []
    contexts: list[int] = []
    try:
        load_seconds = wait_ready(args.port, process)
        cases = CORPUS["questions"][:args.limit] if args.limit else CORPUS["questions"]
        for case in cases:
            for mode in ("ungrounded", "grounded"):
                if mode == "grounded":
                    context = builder.build(case["question"], uses_patch=case.get("usesPatch", False))
                    prompt = context.prompt
                    context_meta = {"characters": context.characters, "approximateTokens": context.approximate_tokens, "factIds": context.fact_ids}
                    contexts.append(context.characters)
                else:
                    prompt = case["question"]
                    context_meta = {"characters": len(prompt), "approximateTokens": max(1, round(len(prompt) / 4)), "factIds": []}
                result = post_stream(args.port, prompt, args.temperature, args.max_tokens)
                result.update({
                    "caseId": case["id"], "category": case["category"],
                    "question": case["question"], "mode": mode,
                    "context": context_meta,
                    "automatedFlags": flags(result["answer"]),
                })
                records.append(result)
                print(f"{args.model}: {case['id']} {mode} ({len(records)})", flush=True)

        for conversation in CORPUS["conversations"]:
            for mode in ("ungrounded", "grounded"):
                history: list[dict] = []
                for turn, question in enumerate(conversation["turns"], 1):
                    if mode == "grounded":
                        context = builder.build(question, uses_patch=True, history=history)
                        prompt = context.prompt
                        context_meta = {"characters": context.characters, "approximateTokens": context.approximate_tokens, "factIds": context.fact_ids}
                        contexts.append(context.characters)
                    else:
                        transcript = "\n".join(f"{m['role']}: {m['content']}" for m in history)
                        prompt = (transcript + "\nuser: " + question).strip()
                        context_meta = {"characters": len(prompt), "approximateTokens": max(1, round(len(prompt) / 4)), "factIds": []}
                    result = post_stream(args.port, prompt, args.temperature, args.max_tokens)
                    result.update({
                        "caseId": f"{conversation['id']}_turn_{turn}", "category": "FOLLOW_UP",
                        "question": question, "mode": mode, "context": context_meta,
                        "automatedFlags": flags(result["answer"]),
                    })
                    records.append(result)
                    print(f"{args.model}: {conversation['id']} turn {turn} {mode} ({len(records)})", flush=True)
                    history.extend([{"role": "user", "content": question}, {"role": "assistant", "content": result["answer"]}])
    finally:
        stop.set()
        process.terminate()
        try:
            process.wait(10)
        except subprocess.TimeoutExpired:
            process.kill()
        log.close()

    with (output / "responses.jsonl").open("w", encoding="utf-8") as handle:
        for record in records:
            handle.write(json.dumps(record, ensure_ascii=False) + "\n")
    latencies = [r["firstTokenSeconds"] for r in records]
    rates = [r["tokensPerSecond"] for r in records if r["tokensPerSecond"]]
    performance = {
        "model": args.model,
        "modelFileBytes": args.model_path.stat().st_size,
        "loadSeconds": round(load_seconds, 3),
        "meanFirstTokenSeconds": round(sum(latencies) / len(latencies), 3),
        "meanTokensPerSecond": round(sum(rates) / len(rates), 2) if rates else None,
        "peakServerRssBytes": peak[0] * 1024,
        "temperature": args.temperature,
        "contextWindow": 4096,
        "meanGroundedContextCharacters": round(sum(contexts) / len(contexts), 1),
        "meanGroundedContextApproximateTokens": round(sum(contexts) / len(contexts) / 4, 1),
    }
    (output / "performance.json").write_text(json.dumps(performance, indent=2) + "\n", encoding="utf-8")
    summary = {
        "model": args.model,
        "responseCount": len(records),
        "groundedCount": sum(r["mode"] == "grounded" for r in records),
        "ungroundedCount": sum(r["mode"] == "ungrounded" for r in records),
        "automaticallyFlagged": sum(bool(r["automatedFlags"]) for r in records),
        "hardFailCount": None,
        "humanReviewRequired": True,
        "winnerDeclared": False,
    }
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    write_review(output / "human_review.md", args.model, records)


if __name__ == "__main__":
    main()
