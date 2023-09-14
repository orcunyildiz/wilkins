#! /bin/bash

bin_dir=$1
differentFiles=$2
passthru=$3

if [ $differentFiles == 0 ]; then
    echo "mpirun -n 2 -l python -u wilkins-master.py wilkins_prod_stateless_test_singleFile.yaml"
    mpirun -n 2 -l python -u ../../examples/lowfive/wilkins-master.py ../../tests/wilkins_stateless_test_singleFile.yaml
else
    if [ $passthru == 0 ]; then
        echo "mpirun -n 2 -l python -u wilkins-master.py wilkins_stateless_test.yaml"
        mpirun -n 2 -l python -u ../../examples/lowfive/wilkins-master.py ../../tests/wilkins_stateless_test.yaml
    else
        echo "mpirun -n 7 -l python -u wilkins-master.py wilkins_stateless_test_passthru.yaml"
        mpirun -n 7 -l python -u ../../examples/lowfive/wilkins-master.py ../../tests/wilkins_stateless_test_passthru.yaml
    fi
fi

retval=$?

if [ -f "$bin_dir/outfile.h5" ]; then
    rm "$bin_dir/outfile.h5"
fi

if [ $retval == 0 ]; then
    exit 0
else
    exit 1
fi
