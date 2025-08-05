#! /bin/bash

bin_dir=$1
topology=$2 #0: fanin #1: fanout #2: NxN

#Generating executables first
cp prod-ensemble.hx prod-ensemble-inst0.hx
cp prod-ensemble.hx prod-ensemble-inst1.hx
cp prod-ensemble.hx prod-ensemble-inst2.hx
cp prod-ensemble.hx prod-ensemble-inst3.hx
cp con-ensemble.hx con-ensemble-inst0.hx
cp con-ensemble.hx con-ensemble-inst1.hx
cp con-ensemble.hx con-ensemble-inst2.hx
cp con-ensemble.hx con-ensemble-inst3.hx

if [ $topology == 0 ]
then
    echo "mpirun -n 12 -l python -u wilkins-master.py wilkins_ensemble_test_fanin.yaml"
    mpirun -n 12 -l python -u wilkins-master.py ../../tests/wilkins_ensemble_test_fanin.yaml
elif [ $topology == 1 ]
then
    echo "mpirun -n 12 -l python -u wilkins-master.py wilkins_ensemble_test_fanout.yaml"
    mpirun -n 12 -l python -u wilkins-master.py ../../tests/wilkins_ensemble_test_fanout.yaml
elif [ $topology == 2 ]
then 
    echo "mpirun -n 12 -l python -u wilkins-master.py wilkins_ensembl_test_NxN.yaml"
    mpirun -n 12 -l python -u wilkins-master.py ../../tests/wilkins_ensemble_test_NxN.yaml
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
