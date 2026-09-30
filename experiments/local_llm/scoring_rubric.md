# GT Lab local-LLM benchmark rubric

Human reviewers assign 0–5 for each dimension; the model never grades itself.

1. Intent understanding
2. GT-10 factual accuracy
3. Grounded-context usage
4. Tone reasoning
5. Conversational naturalness
6. Follow-up quality
7. Anti-hallucination
8. Brazilian Portuguese quality
9. Helpfulness
10. Concision

## Hard fails

- Invents a GT-10 feature, effect, parameter, or range.
- Contradicts supplied structural truth.
- Claims to have heard audio.
- Ignores an explicit user constraint.
- Confuses the device with another product.
- Suggests a value outside a supplied range.
- Claims faithful artist-tone cloning.

Automated checks flag likely violations; a human makes the qualitative judgment.
