#!/usr/bin/env bash
# Run the evaluation suite. Uses uv when available, system python3 otherwise
# (e.g. inside the Docker dev container, where tqdm/opencv are preinstalled).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

if command -v uv >/dev/null 2>&1; then
    exec uv run --group eval --project "$ROOT" python "$ROOT/eval/run_eval.py" "$@"
fi

exec python3 "$ROOT/eval/run_eval.py" "$@"
