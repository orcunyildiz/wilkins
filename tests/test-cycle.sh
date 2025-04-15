#! /bin/bash

bin_dir=$1
passthru=$2

cp ../../examples/lowfive/actions/passthru-actions.py .
cp ../../examples/lowfive/wilkins-master.py .

if [ $passthru == 0 ]; then
    echo "mpirun -n 3 -l python -u wilkins-master.py wilkins_cycle_test_memory.yaml"
    mpirun -n 3 -l python -u wilkins-master.py ../../tests/wilkins_cycle_test_memory.yaml
else
    echo "mpirun -n 3 -l python -u wilkins-master.py wilkins_cycle_test_passthru.yaml"
    mpirun -n 3 -l python -u wilkins-master.py ../../tests/wilkins_cycle_test_passthru.yaml
fi

retval=$?

if [ -f "$bin_dir/outfile0.h5" ]; then
    rm "$bin_dir/outfile0.h5"
fi

if [ -f "$bin_dir/outfile1.h5" ]; then
    rm "$bin_dir/outfile1.h5"
fi

if [ -f "$bin_dir/outfile2.h5" ]; then
    rm "$bin_dir/outfile2.h5"
fi

if [ $retval == 0 ]; then
    exit 0
else
    exit 1
fi
