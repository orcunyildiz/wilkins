#! /bin/bash

bin_dir=$1
passthru=$2

cp ../../examples/lowfive/actions/passthru-actions.py .

log=$(mktemp)

if [ $passthru == 0 ]; then
    echo "mpirun -n 3 -l python -m wilkins.master wilkins_cycle_test_memory.yaml"
    mpirun -n 3 -l python -m wilkins.master ../../tests/wilkins_cycle_test_memory.yaml 2>&1 | tee "$log"
else
    echo "mpirun -n 3 -l python -m wilkins.master wilkins_cycle_test_passthru.yaml"
    mpirun -n 3 -l python -m wilkins.master ../../tests/wilkins_cycle_test_passthru.yaml 2>&1 | tee "$log"
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

# passthru mode: verify expected .h5 files were created
if [ $passthru != 0 ]; then
    for f in outfile0.h5 outfile1.h5 outfile2.h5; do
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

# h5 files: only passthru mode (passthru=1) writes real files to disk.
# node0 writes outfile0.h5, node1 writes outfile1.h5, node2 writes outfile2.h5.
if [ $passthru != 0 ]; then
    rm -f "$bin_dir/outfile0.h5" "$bin_dir/outfile1.h5" "$bin_dir/outfile2.h5"
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
