#!/usr/bin/env python3
"""Minimal deterministic context selection for the isolated LLM lab."""

from __future__ import annotations

import json
import re
import unicodedata
from dataclasses import dataclass
from pathlib import Path


LAB = Path(__file__).resolve().parents[1]


def normalized(text: str) -> str:
    text = unicodedata.normalize("NFKD", text.casefold())
    return " ".join("".join(c for c in text if not unicodedata.combining(c)).split())


def rough_tokens(text: str) -> int:
    return max(1, round(len(text) / 4))


@dataclass(frozen=True)
class GroundedContext:
    prompt: str
    characters: int
    approximate_tokens: int
    fact_ids: tuple[str, ...]


class GroundedLLMContextBuilder:
    """Selects only keyword-relevant facts plus optional compact patch context."""

    def __init__(self) -> None:
        self.system_prompt = (LAB / "system_prompt.txt").read_text(encoding="utf-8").strip()
        self.facts = json.loads((LAB / "knowledge/grounding_facts.json").read_text(encoding="utf-8"))
        self.patch = json.loads(
            (LAB.parents[1] / "tests/fixtures/ai_context_wc_all_in_one.json").read_text(encoding="utf-8")
        )

    def select(self, question: str, limit: int = 5) -> list[dict]:
        query = normalized(question)
        scored: list[tuple[int, int, dict]] = []
        for order, fact in enumerate(self.facts):
            score = sum(1 for keyword in fact["keywords"] if normalized(keyword) in query)
            if score:
                scored.append((-score, order, fact))
        if not scored:
            authority = next(f for f in self.facts if f["id"] == "authority")
            return [authority]
        return [item[2] for item in sorted(scored)[:limit]]

    def build(self, question: str, *, uses_patch: bool, history: list[dict] | None = None) -> GroundedContext:
        selected = self.select(question)
        facts = "\n".join(f"- [{f['kind']}] {f['text']}" for f in selected)
        parts = [
            self.system_prompt,
            "\nVERIFIED GT LAB CONTEXT:\n" + facts,
        ]
        if uses_patch:
            compact = json.dumps(self.patch, ensure_ascii=False, separators=(",", ":"))
            parts.append("\nAPPROVED COMPACT PATCH CONTEXT:\n" + compact)
        if history:
            safe_history = history[-6:]
            parts.append("\nCONVERSATION SO FAR:\n" + "\n".join(
                f"{m['role'].upper()}: {m['content']}" for m in safe_history
            ))
        parts.append("\nUSER QUESTION:\n" + question)
        parts.append("\nAnswer in Brazilian Portuguese.")
        prompt = "\n".join(parts)
        forbidden = ("/Users/", "username", ".git", "model_path", "F0 41")
        if any(value in prompt for value in forbidden):
            raise ValueError("context contains forbidden internal data")
        return GroundedContext(prompt, len(prompt), rough_tokens(prompt), tuple(f["id"] for f in selected))


if __name__ == "__main__":
    builder = GroundedLLMContextBuilder()
    sample = builder.build("Esse Pre Delay de 100 ms faz sentido?", uses_patch=True)
    print(json.dumps(sample.__dict__, ensure_ascii=False, indent=2))
