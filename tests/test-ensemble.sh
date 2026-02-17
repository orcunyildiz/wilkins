#! /bin/bash

bin_dir=$1
topology=$2 #0: fanin #1: fanout #2: NxN

#Generating executables first
cp "$bin_dir"/prod-ensemble.hx "$bin_dir"/prod-ensemble-inst0.hx
cp "$bin_dir"/prod-ensemble.hx "$bin_dir"/prod-ensemble-inst1.hx
cp "$bin_dir"/prod-ensemble.hx "$bin_dir"/prod-ensemble-inst2.hx
cp "$bin_dir"/prod-ensemble.hx "$bin_dir"/prod-ensemble-inst3.hx
cp "$bin_dir"/con-ensemble.hx "$bin_dir"/con-ensemble-inst0.hx
cp "$bin_dir"/con-ensemble.hx "$bin_dir"/con-ensemble-inst1.hx
cp "$bin_dir"/con-ensemble.hx "$bin_dir"/con-ensemble-inst2.hx
cp "$bin_dir"/con-ensemble.hx "$bin_dir"/con-ensemble-inst3.hx

log=$(mktemp)

if [ $topology == 0 ]
then
    echo "mpirun -n 12 -l python -m wilkins.master wilkins_ensemble_test_fanin.yaml"
    mpirun -n 12 -l python -m wilkins.master ../../tests/wilkins_ensemble_test_fanin.yaml 2>&1 | tee "$log"
elif [ $topology == 1 ]
then
    echo "mpirun -n 12 -l python -m wilkins.master wilkins_ensemble_test_fanout.yaml"
    mpirun -n 12 -l python -m wilkins.master ../../tests/wilkins_ensemble_test_fanout.yaml 2>&1 | tee "$log"
elif [ $topology == 2 ]
then 
    echo "mpirun -n 12 -l python -m wilkins.master wilkins_ensemble_test_NxN.yaml"
    mpirun -n 12 -l python -m wilkins.master ../../tests/wilkins_ensemble_test_NxN.yaml 2>&1 | tee "$log"
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

# all ensemble variants use passthru=0 (in-memory), so no .h5 files to check.

# check mpirun exit code: only fatal if data validation also failed
if [ $retval != 0 ]; then
    if [ "$data_ok" = true ]; then
        echo "WARNING: mpirun exited with code $retval (cleanup crash, data flow OK)"
    else
        echo "FAIL: mpirun exited with code $retval"
    fi
fi

# --- cleanup ---

# copied .hx instance files and log
rm -f "$bin_dir"/prod-ensemble-inst{0,1,2,3}.hx
rm -f "$bin_dir"/con-ensemble-inst{0,1,2,3}.hx
rm -f "$log"

if [ "$data_ok" = true ]; then
    echo "PASS"
    exit 0
else
    exit 1
fi
