#!/usr/bin/env bash
set -euo pipefail

script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
if [ "$#" -ge 1 ] && [ "$1" = "convert" ]; then
    if [ "$#" -ne 3 ]; then
        echo "Usage: $0 convert <text .data/.graph> <binary .data/.graph>" >&2
        exit 64
    fi
    exec python3 "$script_dir/convert_to_binary.py" "$2" "$3"
fi

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 {mnist|fashion-mnist|sift1m|troll|com-orkut|paper datasets}" >&2
    exit 64
fi

dataset="$1"

case "$dataset" in
    mnist|fashion-mnist|sift1m|troll|com-orkut|dwt_72|lesmis|can_96|rajat11|jazz|visbrazil|grid17|mesh3e1|netscience|dwt_419|price_1000|dwt_1005|cage8|bcsstk09|block_2000|sierpinski3d|CA-GrQc|EVA|3elt|us_powergrid|G65|fe_4elt2|bcsstk31|venkat50|ship_003|web-NotreDame|Flan_1565|com-LiveJournal) ;;
    *)
        echo "Unknown dataset: $dataset" >&2
        exit 64
        ;;
esac

root_dir="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
data_dir="$root_dir/data"
raw_dir="$data_dir/raw/$dataset"
results_dir="$data_dir/results"
mkdir -p "$raw_dir" "$results_dir"

verify_sift1m_official() {
    archive="$1"
    output="$2"
    expected_md5="b23d1b3b2ee8469d819b61ca900ef0ed"
    if [ "$(md5sum "$archive" | awk '{print $1}')" != "$expected_md5" ]; then
        echo "SIFT1M official archive MD5 mismatch: $archive" >&2
        exit 65
    fi
    SIFT_ARCHIVE="$archive" SIFT_OUTPUT="$output" python3 - <<'PY'
import os
import struct
import tarfile

archive_path = os.environ["SIFT_ARCHIVE"]
output_path = os.environ["SIFT_OUTPUT"]
with tarfile.open(archive_path, "r:gz") as archive:
    source = archive.extractfile("sift/sift_base.fvecs")
    if source is None:
        raise ValueError("Official SIFT1M archive is missing sift_base.fvecs")
    with source, open(output_path, "rb") as output:
        header = output.read(32)
        if len(header) != 32:
            raise ValueError("SIFT1M binary output has an incomplete header")
        magic, version, kind, count, dimension = struct.unpack("<8sIIQQ", header)
        if (magic, version, kind, count, dimension) != (b"DRGBIN01", 1, 1, 1000000, 128):
            raise ValueError("SIFT1M binary output header does not match")
        for row in range(count):
            stored_dimension = source.read(4)
            values = source.read(dimension * 4)
            if len(stored_dimension) != 4 or len(values) != dimension * 4:
                raise ValueError(f"Official SIFT1M data is truncated at row {row}")
            if struct.unpack("<I", stored_dimension)[0] != dimension:
                raise ValueError(f"Official SIFT1M has an invalid dimension at row {row}")
            if output.read(dimension * 4) != values:
                raise ValueError(f"Official SIFT1M and output differ at row {row}")
        if source.read(1) or output.read(1):
            raise ValueError("SIFT1M data contains trailing content")
PY
}

download() {
    url="$1"
    destination="$2"
    if [ -f "$destination" ]; then
        return
    fi
    temporary="$destination.partial"
    if command -v curl >/dev/null 2>&1; then
        curl --fail --location --retry 3 --output "$temporary" "$url"
    elif command -v wget >/dev/null 2>&1; then
        wget --output-document="$temporary" "$url"
    else
        echo "curl or wget is required to download datasets" >&2
        exit 69
    fi
    mv "$temporary" "$destination"
}

paper_graph_source=""
paper_graph_format=""
paper_graph_vertices=0
paper_graph_edges=0
case "$dataset" in
    dwt_72) paper_graph_source="https://raw.githubusercontent.com/ZJUVAI/DRGraph/master/data/dwt_72.txt"; paper_graph_format=text; paper_graph_vertices=72; paper_graph_edges=75 ;;
    lesmis) paper_graph_source="https://raw.githubusercontent.com/ZJUVAI/DRGraph/master/data/lesmis.txt"; paper_graph_format=text; paper_graph_vertices=77; paper_graph_edges=254 ;;
    can_96) paper_graph_source="https://raw.githubusercontent.com/ZJUVAI/DRGraph/master/data/can_96.txt"; paper_graph_format=text; paper_graph_vertices=96; paper_graph_edges=336 ;;
    rajat11) paper_graph_source="https://raw.githubusercontent.com/ZJUVAI/DRGraph/master/data/rajat11.txt"; paper_graph_format=text; paper_graph_vertices=135; paper_graph_edges=377 ;;
    jazz) paper_graph_source="https://raw.githubusercontent.com/ZJUVAI/DRGraph/master/data/jazz.txt"; paper_graph_format=text; paper_graph_vertices=198; paper_graph_edges=2742 ;;
    visbrazil) paper_graph_source="https://raw.githubusercontent.com/ZJUVAI/DRGraph/master/data/visbrazil.txt"; paper_graph_format=text; paper_graph_vertices=222; paper_graph_edges=336 ;;
    grid17) paper_graph_source="https://raw.githubusercontent.com/ZJUVAI/DRGraph/master/data/grid17.txt"; paper_graph_format=text; paper_graph_vertices=289; paper_graph_edges=544 ;;
    mesh3e1) paper_graph_source="https://raw.githubusercontent.com/ZJUVAI/DRGraph/master/data/mesh3e1.txt"; paper_graph_format=text; paper_graph_vertices=289; paper_graph_edges=800 ;;
    netscience) paper_graph_source="https://raw.githubusercontent.com/ZJUVAI/DRGraph/master/data/netscience.txt"; paper_graph_format=text; paper_graph_vertices=379; paper_graph_edges=914 ;;
    dwt_419) paper_graph_source="https://raw.githubusercontent.com/ZJUVAI/DRGraph/master/data/dwt_419.txt"; paper_graph_format=text; paper_graph_vertices=419; paper_graph_edges=1572 ;;
    price_1000) paper_graph_source="https://raw.githubusercontent.com/ZJUVAI/DRGraph/master/data/price_1000.txt"; paper_graph_format=text; paper_graph_vertices=1000; paper_graph_edges=999 ;;
    dwt_1005) paper_graph_source="https://raw.githubusercontent.com/ZJUVAI/DRGraph/master/data/dwt_1005.txt"; paper_graph_format=text; paper_graph_vertices=1005; paper_graph_edges=3808 ;;
    cage8) paper_graph_source="https://raw.githubusercontent.com/ZJUVAI/DRGraph/master/data/cage8.txt"; paper_graph_format=text; paper_graph_vertices=1015; paper_graph_edges=4994 ;;
    block_2000) paper_graph_source="https://raw.githubusercontent.com/ZJUVAI/DRGraph/master/data/block_2000.txt"; paper_graph_format=text; paper_graph_vertices=2000; paper_graph_edges=9912 ;;
    sierpinski3d) paper_graph_source="https://raw.githubusercontent.com/ZJUVAI/DRGraph/master/data/sierpinski3d.txt"; paper_graph_format=text; paper_graph_vertices=2050; paper_graph_edges=6144 ;;
    CA-GrQc) paper_graph_source="https://raw.githubusercontent.com/ZJUVAI/DRGraph/master/data/CA-GrQc.txt"; paper_graph_format=text; paper_graph_vertices=4158; paper_graph_edges=13422 ;;
    EVA) paper_graph_source="https://raw.githubusercontent.com/ZJUVAI/DRGraph/master/data/EVA.txt"; paper_graph_format=text; paper_graph_vertices=4475; paper_graph_edges=4652 ;;
    3elt) paper_graph_source="https://raw.githubusercontent.com/ZJUVAI/DRGraph/master/data/3elt.txt"; paper_graph_format=text; paper_graph_vertices=4720; paper_graph_edges=13722 ;;
    us_powergrid) paper_graph_source="https://raw.githubusercontent.com/ZJUVAI/DRGraph/master/data/us_powergrid.txt"; paper_graph_format=text; paper_graph_vertices=4941; paper_graph_edges=6594 ;;
    G65) paper_graph_source="https://raw.githubusercontent.com/ZJUVAI/DRGraph/master/data/G65.txt"; paper_graph_format=text; paper_graph_vertices=8000; paper_graph_edges=16000 ;;
    bcsstk09) paper_graph_source="https://github.com/HanKruiger/tsNET/raw/refs/heads/master/graphs/bcsstk09.vna"; paper_graph_format=vna; paper_graph_vertices=1083; paper_graph_edges=8677 ;;
    fe_4elt2) paper_graph_source="https://sparse.tamu.edu/MM/DIMACS10/fe_4elt2.tar.gz"; paper_graph_format=matrix-market; paper_graph_vertices=11143; paper_graph_edges=32818 ;;
    bcsstk31) paper_graph_source="https://sparse.tamu.edu/MM/HB/bcsstk31.tar.gz"; paper_graph_format=matrix-market; paper_graph_vertices=35588; paper_graph_edges=572914 ;;
    venkat50) paper_graph_source="https://sparse.tamu.edu/MM/Simon/venkat50.tar.gz"; paper_graph_format=matrix-market; paper_graph_vertices=62424; paper_graph_edges=827671 ;;
    ship_003) paper_graph_source="https://sparse.tamu.edu/MM/DNVS/ship_003.tar.gz"; paper_graph_format=matrix-market; paper_graph_vertices=121728; paper_graph_edges=1827654 ;;
    web-NotreDame) paper_graph_source="https://snap.stanford.edu/data/web-NotreDame.txt.gz"; paper_graph_format=snap; paper_graph_vertices=325729; paper_graph_edges=1469679 ;;
    Flan_1565) paper_graph_source="https://sparse.tamu.edu/MM/Janna/Flan_1565.tar.gz"; paper_graph_format=matrix-market; paper_graph_vertices=1564794; paper_graph_edges=56300289 ;;
    com-LiveJournal) paper_graph_source="https://snap.stanford.edu/data/bigdata/communities/com-lj.ungraph.txt.gz"; paper_graph_format=snap; paper_graph_vertices=3997962; paper_graph_edges=34681189 ;;
esac

if [ -n "$paper_graph_source" ]; then
    output_path="$results_dir/$dataset.graph"
    case "$paper_graph_format" in
        text)
            source_path="$raw_dir/$dataset.txt"
            download "$paper_graph_source" "$source_path"
            python3 "$script_dir/convert_graph_source.py" text "$source_path" "$output_path" \
                --vertices "$paper_graph_vertices" --edges "$paper_graph_edges"
            ;;
        vna)
            source_path="$raw_dir/$dataset.vna"
            download "$paper_graph_source" "$source_path"
            python3 "$script_dir/convert_graph_source.py" vna "$source_path" "$output_path" \
                --vertices "$paper_graph_vertices" --edges "$paper_graph_edges"
            ;;
        snap)
            source_path="$raw_dir/$dataset.txt.gz"
            download "$paper_graph_source" "$source_path"
            python3 "$script_dir/convert_graph_source.py" snap "$source_path" "$output_path" \
                --vertices "$paper_graph_vertices" --edges "$paper_graph_edges"
            ;;
        matrix-market)
            source_path="$raw_dir/$dataset.tar.gz"
            download "$paper_graph_source" "$source_path"
            python3 "$script_dir/convert_graph_source.py" matrix-market "$source_path" "$output_path" \
                --vertices "$paper_graph_vertices" --edges "$paper_graph_edges"
            ;;
    esac
    echo "Generated DRGBIN01 input: $output_path"
    exit 0
fi

case "$dataset" in
    mnist|fashion-mnist)
        output_path="$results_dir/$dataset.data"
        if [ "$dataset" = "mnist" ]; then
            base_url="https://ossci-datasets.s3.amazonaws.com/mnist"
        else
            base_url="http://fashion-mnist.s3-website.eu-central-1.amazonaws.com"
        fi
        for name in train-images-idx3-ubyte.gz t10k-images-idx3-ubyte.gz; do
            download "$base_url/$name" "$raw_dir/$name"
        done
        RAW_DIR="$raw_dir" OUTPUT="$output_path" python3 - <<'PY'
import gzip
import os
import struct

paths = [os.path.join(os.environ["RAW_DIR"], name)
         for name in ("train-images-idx3-ubyte.gz", "t10k-images-idx3-ubyte.gz")]
headers = []
for path in paths:
    with gzip.open(path, "rb") as stream:
        magic, count, rows, cols = struct.unpack(">IIII", stream.read(16))
        if magic != 2051:
            raise ValueError(f"Invalid IDX image file: {path}")
        headers.append((count, rows * cols))
count = sum(item[0] for item in headers)
dimension = headers[0][1]
if any(item[1] != dimension for item in headers):
    raise ValueError("IDX dimensions do not match")
output_path = os.environ["OUTPUT"]
temporary_path = output_path + ".partial"
with open(temporary_path, "wb") as output:
    output.write(struct.pack("<8sIIQQ", b"DRGBIN01", 1, 1, count, dimension))
    for path, (part_count, _) in zip(paths, headers):
        with gzip.open(path, "rb") as stream:
            stream.read(16)
            for _ in range(part_count):
                row = stream.read(dimension)
                if len(row) != dimension:
                    raise ValueError(f"Invalid IDX file length: {path}")
                output.write(struct.pack("<" + "f" * dimension, *row))
os.replace(temporary_path, output_path)
PY
        ;;
    sift1m)
        output_path="$results_dir/sift1m.data"
        archive="$raw_dir/sift-128-euclidean.hdf5"
        download "https://ann-benchmarks.com/sift-128-euclidean.hdf5" "$archive"
        if ! command -v h5cc >/dev/null 2>&1; then
            echo "SIFT1M conversion requires the system HDF5 h5cc compiler" >&2
            exit 69
        fi
        converter="$raw_dir/convert_sift1m_hdf5"
        if ! h5cc -std=c11 -O3 "$script_dir/convert_sift1m_hdf5.c" -o "$converter"; then
            rm -f "$converter"
            exit 69
        fi
        if ! "$converter" "$archive" "$output_path"; then
            rm -f "$converter"
            exit 65
        fi
        rm -f "$converter"
        official_archive="$raw_dir/sift.tar.gz"
        if [ -f "$official_archive" ]; then
            verify_sift1m_official "$official_archive" "$output_path"
        fi
        ;;
    com-orkut)
        output_path="$results_dir/com-orkut.graph"
        archive="$raw_dir/com-orkut.ungraph.txt.gz"
        download "https://snap.stanford.edu/data/bigdata/communities/com-orkut.ungraph.txt.gz" "$archive"
        python3 "$script_dir/convert_graph_source.py" snap "$archive" "$output_path" \
            --vertices 3072441 --edges 117185083
        ;;
    troll)
        output_path="$results_dir/troll.graph"
        archive="$raw_dir/troll.tar.gz"
        download "https://sparse.tamu.edu/MM/DNVS/troll.tar.gz" "$archive"
        python3 "$script_dir/convert_graph_source.py" matrix-market "$archive" "$output_path" \
            --vertices 213453 --edges 5885829
        ;;
esac

echo "Generated DRGBIN01 input: $output_path"
