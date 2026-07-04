# Phase 2 Quality Gates

Phase 2 rejects KV compression claims unless the exact validation cell has a
quality gate result. `perplexity` or score-only evidence is not enough for
retrieval or long-context claims.

Minimum deterministic gates:

| Gate | Required Evidence |
|---|---|
| `smoke` | Deterministic prompt output remains within the declared delta. |
| `retrieval` | Smoke gate plus retrieval pass/fail result. |
| `long-context` | Retrieval gate plus long-context pass/fail result. |

The `prism-quality` tool evaluates deterministic fixtures:

```powershell
.\build\Debug\prism-quality.exe `
  --quality-gate retrieval `
  --baseline-score 1.0 `
  --observed-score 1.01 `
  --max-delta 0.02 `
  --retrieval-passed true `
  --deterministic-match true
```

Accepted compression evidence must retain:

- matching validation-cell metadata,
- baseline and observed score,
- threshold,
- retrieval or long-context result when applicable,
- memory savings after metadata overhead,
- cap result.
