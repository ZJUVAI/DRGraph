#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <drgraph> <input .data/.graph> <output-dir> [--timeout <seconds>] [drgraph options...]" >&2
}

if [ "$#" -lt 3 ]; then
    usage
    exit 64
fi

program="$1"
input="$2"
output_dir="$3"
shift 3
timeout_seconds="${DRGRAPH_BENCHMARK_TIMEOUT_SECONDS:-3600}"
extra_args=()
while [ "$#" -gt 0 ]; do
    case "$1" in
        --timeout)
            if [ "$#" -lt 2 ]; then
                usage
                exit 64
            fi
            timeout_seconds="$2"
            shift 2
            ;;
        --timeout=*)
            timeout_seconds="${1#--timeout=}"
            shift
            ;;
        *)
            extra_args+=("$1")
            shift
            ;;
    esac
done

case "$timeout_seconds" in ''|*[!0-9]*) echo "Error: timeout must be a positive integer" >&2; exit 64 ;; esac
if [ "$timeout_seconds" -eq 0 ]; then
    echo "Error: timeout must be a positive integer" >&2
    exit 64
fi
if [ ! -x "$program" ]; then
    echo "Error: drgraph is not executable: $program" >&2
    exit 66
fi
if [ "$(basename -- "$program")" != "drgraph" ]; then
    echo "Error: benchmark program must be named drgraph: $program" >&2
    exit 64
fi
if [ ! -f "$input" ]; then
    echo "Error: input does not exist or is not a regular file: $input" >&2
    exit 66
fi
case "$input" in
    *.data|*.graph) ;;
    *) echo "Error: input extension must be .data or .graph: $input" >&2; exit 64 ;;
esac
if ! command -v timeout >/dev/null 2>&1; then
    echo "Error: timeout is required to limit benchmark runtime" >&2
    exit 69
fi

argument_index=0
while [ "$argument_index" -lt "${#extra_args[@]}" ]; do
    argument="${extra_args[$argument_index]}"
    case "$argument" in
        --input|--input=*|-input|-input=*|--output|--output=*|-output|-output=*|--stats-json|--stats-json=*)
            echo "Error: extra options cannot override input, output, or stats paths" >&2
            exit 64
            ;;
        --knn-backend|--knn-backend=*|--optimizer-backend|--optimizer-backend=*|--gpu-*|--ivf-*|--pq-*|--hnsw-*|--faiss-*)
            echo "Error: unsupported benchmark option: $argument" >&2
            exit 64
            ;;
    esac
    argument_index=$((argument_index + 1))
done

if [ -e "$output_dir" ]; then
    if [ ! -d "$output_dir" ]; then
        echo "Error: output path is not a directory: $output_dir" >&2
        exit 73
    fi
    if find "$output_dir" -mindepth 1 -maxdepth 1 -print -quit | grep -q .; then
        echo "Error: output directory must be empty: $output_dir" >&2
        exit 73
    fi
elif ! mkdir -p -- "$output_dir"; then
    echo "Error: cannot create output directory: $output_dir" >&2
    exit 73
fi

time_program=""
for candidate in "$(command -v gtime 2>/dev/null || true)" /usr/bin/time; do
    if [ -n "$candidate" ] && [ -x "$candidate" ] && "$candidate" --version 2>&1 | grep -q 'GNU time'; then
        time_program="$candidate"
        break
    fi
done

printf '%q ' "$program" --input "$input" --output '<run-dir>/embedding.txt' --stats-json '<run-dir>/stats.json' "${extra_args[@]}" >"$output_dir/command.txt"
printf '\n' >>"$output_dir/command.txt"
printf 'input=%s\n' "$input" >"$output_dir/input.txt"
printf 'timeout_seconds=%s\n' "$timeout_seconds" >>"$output_dir/input.txt"

for run in 0 1 2 3 4 5; do
    run_dir="$output_dir/run-$run"
    mkdir -p -- "$run_dir"
    output="$run_dir/embedding.txt"
    stats_json="$run_dir/stats.json"
    metrics="$run_dir/time.txt"
    stdout="$run_dir/stdout.txt"
    stderr="$run_dir/stderr.txt"
    command=("$program" --input "$input" --output "$output" --stats-json "$stats_json" "${extra_args[@]}")
    printf '%q ' "${command[@]}" >"$run_dir/command.txt"
    printf '\n' >>"$run_dir/command.txt"
    if [ -n "$time_program" ]; then
        "$time_program" -v -o "$metrics" timeout --foreground --kill-after=30s "${timeout_seconds}s" "${command[@]}" >"$stdout" 2>"$stderr" &
    else
        (
            TIMEFORMAT='wall_seconds=%3R user_seconds=%3U system_seconds=%3S'
            { time timeout --foreground --kill-after=30s "${timeout_seconds}s" "${command[@]}" >"$stdout" 2>"$stderr"; } 2>"$metrics"
        ) &
    fi
    child_pid=$!
    if wait "$child_pid"; then
        run_exit=0
    else
        run_exit=$?
    fi
    if [ -z "$time_program" ]; then
        printf 'peak_rss_kib=unavailable\n' >>"$metrics"
    fi
    if [ "$run_exit" -eq 124 ] || [ "$run_exit" -eq 137 ]; then
        echo "Error: run $run timed out after ${timeout_seconds} seconds" >&2
        exit 124
    fi
    if [ "$run_exit" -ne 0 ]; then
        echo "Error: run $run failed with exit code $run_exit" >&2
        exit "$run_exit"
    fi
    if [ ! -s "$stats_json" ]; then
        echo "Error: run $run did not write stats: $stats_json" >&2
        exit 74
    fi
    if ! grep -q '"configuration"' "$stats_json"; then
        echo "Error: run $run stats lack configuration: $stats_json" >&2
        exit 74
    fi
    if [ "$input" != "${input%.data}" ]; then
        if ! grep -q '"resolved_knn"' "$stats_json" || ! grep -q '"knn_stages"' "$stats_json"; then
            echo "Error: run $run stats lack kNN records: $stats_json" >&2
            exit 74
        fi
    fi
    cp "$stats_json" "$run_dir/profile.json"
done

echo "Completed one warm-up and five CPU measurements. Each run waits for completion, failure, or timeout."
