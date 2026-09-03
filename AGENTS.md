# DRGraph Collaboration Rules

- Develop on the `codex` branch. Preserve existing user changes and keep the CPU program independently runnable.
- Multiple agents may edit the repository. Inspect overlapping changes and integrate them incrementally. Do not revert unrelated work.
- `DimensionReduction/` and `system/` were removed. Do not restore them or add build or runtime dependencies on them.
- The input extension selects the task: `.data` runs dimensionality reduction and `.graph` runs graph layout. The core executable reads only `DRGBIN01` binary input. Text belongs in preparation and conversion scripts. Reject unknown extensions and legacy binary graph formats.
- Both input kinds must converge at probability CSR and share hierarchy, sampling, optimization, output, and evaluation. New task branches must reuse the shared pipeline.
- Keep graphs and kNN data in contiguous sparse structures. Do not allocate per edge, store adjacency ownership in nodes, retain duplicate long-lived graphs, or use fixed sampling tables unrelated to data size.
- Use explicit RAII and stage ownership. Release high-dimensional vectors, kNN indexes and workspaces, temporary COO data, temporary coarse graphs, stage samplers, and host/device conversion buffers as soon as downstream stages no longer need them.
- Keep the dataset manifest and source data descriptions under `data/`; keep small test fixtures under `tests/fixtures`. Keep preparation, conversion, benchmark, and visualization scripts under `scripts/`. Do not commit large downloads, generated embeddings, plots, raw benchmark output, or temporary files.
- Keep durable implementation facts in code and README sections when they are needed by users or maintainers. Do not add separate planning or results tracking files.
- `visualize.py` must support both `.data` and `.graph` `DRGBIN01` inputs and the text embedding emitted by `drgraph`. It must validate headers, payload lengths, embedding shape, finite values, and graph endpoints. Large graph rendering must use bounded, seedable edge sampling.
- Keep deterministic and fast modes race-free. Deterministic mode must fix random streams, partitions, reductions, and output order. Fast mode must pass race checks and document quality variation when measured.
- The project is CPU-only. Public and core headers remain free of accelerator dependencies. CPU-only configure, build, and tests must work on systems without the CUDA Toolkit.
- Common CLI help shows input, output, threads, seed, output dimension, kNN degree, determinism, and evaluation options. Advanced algorithm controls belong in `--help-all`.
- User-visible text, comments, and project documentation use English. Use short, clear sentences and avoid unnecessary jargon.
- Changes require tests proportional to risk. Cover binary format checks, CSR invariants, memory lifetime, both pipeline inputs, evaluation, deterministic output, CPU-only builds, and visualization for both input kinds.
- Before handoff, run `git diff --check`, a CPU build, CTest, and focused script checks. Preserve the current branch and do not create a commit unless the user asks for one.
