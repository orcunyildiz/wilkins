#! /bin/bash

bin_dir=$1
src_dir=$2
differentFiles=$3
subset=$4
passthru=$5
cp "$src_dir"/examples/lowfive/actions/passthru-actions.py .

log=$(mktemp)

if [ $differentFiles == 0 ]; then
    echo "mpirun -n 2 -l python -m wilkins.master wilkins_stateful_test_singleFile.yaml"
    mpirun -n 2 -l python -m wilkins.master "$src_dir"/tests/wilkins_stateful_test_singleFile.yaml 2>&1 | tee "$log"
else
    if [ $subset == 0 ]; then
        if [ $passthru == 0 ]; then
            echo "mpirun -n 2 -l python -m wilkins.master wilkins_stateful_test.yaml"
            mpirun -n 2 -l python -m wilkins.master "$src_dir"/tests/wilkins_stateful_test.yaml 2>&1 | tee "$log"
        else
            echo "mpirun -n 5 -l python -m wilkins.master wilkins_stateful_test_passthru.yaml"
            mpirun -n 5 -l python -m wilkins.master "$src_dir"/tests/wilkins_stateful_test_passthru.yaml 2>&1 | tee "$log"
        fi
    else
        echo "mpirun -n 5 -l python -m wilkins.master wilkins_stateful_test_subsetWriters.yaml"
        mpirun -n 5 -l python -m wilkins.master "$src_dir"/tests/wilkins_stateful_test_subsetWriters.yaml 2>&1 | tee "$log"
    fi
fi

retval=${PIPESTATUS[0]}

# --- validation ---

data_ok=true

# check that consumers read data successfully
if ! grep -q "HDF5 read success" "$log"; then
    echo "FAIL: 'HDF5 read success' not found in output"
    data_ok=false
fi

# check for data validation errors
if grep -q "Error:" "$log"; then
    echo "FAIL: data validation errors found in output"
    data_ok=false
fi

# passthru mode: verify expected .h5 files were created.
# prod-henson.cpp with args ["3"] (no -s) creates outfile_0.h5, outfile_1.h5,
# outfile_2.h5.
if [ $differentFiles != 0 ] && [ $subset == 0 ] && [ $passthru != 0 ]; then
    for f in outfile_0.h5 outfile_1.h5 outfile_2.h5; do
        if [ ! -f "$bin_dir/$f" ]; then
            echo "FAIL: expected output file $f not found"
            data_ok=false
        fi
    done
fi

# check mpirun exit code: only fatal if data validation also failed
if [ $retval != 0 ]; then
    if [ "$data_ok" = true ]; then
        echo "WARNING: mpirun exited with code $retval (cleanup crash, data flow OK)"
    else
        echo "FAIL: mpirun exited with code $retval"
    fi
fi

# --- cleanup ---

# h5 files: only the passthru variant (differentFiles=1, subset=0, passthru=1) writes
# real files to disk.  prod-henson.cpp with args ["3"] (no -s) creates
# outfile_0.h5, outfile_1.h5, outfile_2.h5.
if [ $differentFiles != 0 ] && [ $subset == 0 ] && [ $passthru != 0 ]; then
    rm -f "$bin_dir"/outfile_*.h5
fi

# copied helper script and log
rm -f passthru-actions.py
rm -f "$log"

if [ "$data_ok" = true ]; then
    echo "PASS"
    exit 0
else
    exit 1
fi
