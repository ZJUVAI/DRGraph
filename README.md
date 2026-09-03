# DRGraph

DRGraph is a CPU program for two related tasks. The input extension selects the task:

| Input | Task | First stage |
| --- | --- | --- |
| `.data` | Dimensionality reduction for high-dimensional vectors | Exact kNN for small inputs or EFANNA for larger inputs |
| `.graph` | Layout of an existing weighted graph | Read the graph directly |

Both tasks use the same probability CSR representation after input loading. They then share hierarchy construction, sampling, optimization, output, and evaluation code.

The current implementation is a clean, unified CPU pipeline inspired by the DimensionReduction and DRGraph repositories. It should not be described as a bit-for-bit reproduction of either paper. In particular, the current `.data` path uses the project probability transform, does not include the original LargeVis kNN backend, and does not reproduce every original parameter and random-number stream. See [Research compatibility](#research-compatibility) before comparing published numbers.

## Build

The build requires a C++11 compiler, CMake 3.18 or newer, and standard threading support.

```sh
scripts/install.sh --test
```

This installs the executable at `install/bin/drgraph` by default. Use `--prefix`, `--build-dir`, and `--jobs` to change the install prefix, build directory, and parallelism.

For a manual build:

```sh
cmake -S . -B build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 8
ctest --test-dir build --output-on-failure
```

OpenMP and AVX enable the in-tree EFANNA kNN implementation when available. Inputs with fewer than 4096 vectors use exact kNN. The DRGraph paper settings are 400 samples per vertex, five negative samples, and gamma 0.01 on the coarsest level followed by 0.1 on finer levels. Uniform graph weights use row sampling without an edge-sized alias table, while weighted graphs retain exact per-row alias sampling.

## Binary input

The core executable reads `DRGBIN01` little-endian binary files. Text files are accepted only by the preparation and conversion scripts.

The fixed 32-byte header is:

```text
8 bytes   magic: DRGBIN01
uint32    version: 1
uint32    kind: 1 for .data, 2 for .graph
uint64    first count: N
uint64    second count: D for .data, M for .graph
```

`.data` stores `N * D` contiguous `float32` vector values. `.graph` stores `M` records, each containing `uint32 source`, `uint32 target`, and positive `float32 weight`. The reader checks the extension, header, exact file length, finite values, and vertex ranges. Graph self-loops are rejected and duplicate undirected edges are merged while building CSR.

Convert custom text with matching extensions:

```sh
scripts/prepare.sh convert vectors.data data/results/custom.data
scripts/prepare.sh convert network.graph data/results/custom.graph
```

The text `.data` format starts with `N D` followed by one vector per line. The text `.graph` format starts with `N M` followed by `source target [weight]` records. Built-in preparation targets are `mnist`, `fashion-mnist`, `sift1m`, `troll`, and `com-orkut`:

```sh
scripts/prepare.sh mnist
scripts/prepare.sh troll
```

Preparation downloads data into `data/raw/` and writes binary inputs into `data/results/`. Network access and the dataset-specific external tools described by the script may be required.

The paper table has one node-count difference from the current SuiteSparse source. `bcsstk31` currently has 35,588 rows and 572,914 off-diagonal undirected edges; the paper lists 32,715 nodes. Preparation records the paper count in the manifest and uses the authoritative source row count for binary validation. Matrix Market diagonal and explicit zero entries are dropped, general matrices keep their upper-triangle support, and matrix coefficients are treated as unweighted graph support. SNAP node IDs are compacted when the source declares a sparse ID space.

## Run

The output is a text embedding. Its first line is `N output_dim`, followed by one coordinate row per vertex.

Dimensionality reduction:

```sh
install/bin/drgraph \
  --input data/results/mnist.data \
  --output data/results/mnist.embedding \
  --knn-k 15 --threads 32 --seed 42 \
  --stats-json data/results/mnist.json
```

Graph layout:

```sh
install/bin/drgraph \
  --input data/results/troll.graph \
  --output data/results/troll.embedding \
  --threads 32 --seed 42 \
  --stats-json data/results/troll.json
```

Common options cover input, output, threads, seed, output dimension, kNN degree, determinism, and evaluation. Fast optimization is the default. Add `--deterministic` for reproducible sequential optimization, or use `--deterministic 0` explicitly. For the graph datasets from the DRGraph paper, use `--epochs 1 --samples 400 --negative 5 --threads 32 --output-dim 2`. For high-dimensional `.data` experiments based on the hierarchical visualization paper, use a 100-NN graph and 400 samples per vertex when the required input and labels are available. Run `drgraph --help` for the common interface and `drgraph --help-all` for optimization and evaluation controls.

## Evaluation

Evaluation requires a stats path:

```sh
install/bin/drgraph \
  --input data/results/mnist.data \
  --output data/results/mnist.embedding \
  --evaluate \
  --stats-json data/results/mnist-evaluation.json
```

The optional label file contains exactly one non-negative integer per `.data` vertex and must be supplied by the user. Add `--labels path/to/labels.txt` when it is available. Labels are valid only for `.data` input. Graph-layout evaluation writes edge distance/stress metrics, neighborhood preservation, global stress, neighbor stress, and KL divergence. A `.data` run can additionally write sampled trustworthiness and 1/5/10/20/30/40/50-NN classifier accuracy when labels are supplied. Evaluation sampling and neighborhood hop limits are available through `--help-all`.

## Visualization

`visualize.py` reads the same binary input used by `drgraph` and the text embedding produced by it. It supports both vector and graph inputs. Install Python 3 with NumPy and Matplotlib, then run:

For high-dimensional vectors, points are colored by optional labels:

```sh
python3 scripts/visualize.py \
  --input data/results/mnist.data \
  --embedding data/results/mnist.embedding \
  --labels data/labels/mnist.labels \
  --output data/results/mnist.png
```

For graphs, edges are drawn from the binary CSR source. The drawing ratio is 100% below 600,000 edges, 10% from 600,000 through 2,999,999 edges, and 0.1% from 3,000,000 edges onward. `--max-edges` remains an additional upper bound. Sampling uses a reproducible reservoir sampler:

```sh
python3 scripts/visualize.py \
  --input data/results/troll.graph \
  --embedding data/results/troll.embedding \
  --output data/results/troll.png \
  --max-edges 300000 --seed 42
```

The script uses the first two embedding dimensions. A one-dimensional embedding uses zero for its second plotting coordinate. It validates the embedding vertex count and can validate an optional label file for either visualization.

## Benchmarks and repository data

Run the benchmark helper with a binary input:

```sh
scripts/benchmark.sh install/bin/drgraph \
  data/results/mnist.data data/results/mnist-benchmark
```

The helper records one warm-up and five measured CPU runs, command lines, input paths, stage statistics, and timing information. Keep generated results under `data/results/`.

`data/raw/` and `data/results/` are ignored by Git. They can contain multi-gigabyte downloads, converted binaries, embeddings, plots, and benchmark output without being uploaded to GitHub. The dataset manifest is kept at `data/datasets.yml`; test fixtures are kept under `tests/fixtures`. Before pushing, verify with `git ls-files data` and `git status --short`; use Git LFS or external dataset hosting only when a project requires versioned large artifacts.

## Research compatibility

The repository now presents one runnable CPU pipeline for the two input types. This provides a reproducible engineering baseline with binary validation, shared CSR processing, deterministic controls, evaluation JSON, and visualization.

Published-result reproduction still requires a separate compatibility effort. Known differences include the data probability construction, missing original LargeVis kNN behavior, current EFANNA/exact backend choices, changed memory and sampling implementation, and differences in evaluation sampling and random streams. Claims about paper-level reproduction should include the exact input files, parameters, compiler, thread count, seed, and generated stats JSON.
