#! /bin/bash

bin_dir=$1
passthru=$2
workflowType=$3 #0: prod-con 1: prod-2cons 2: 2prod-cons

cp ../../examples/lowfive/actions/passthru-actions.py .
cp ../../examples/lowfive/wilkins-master.py .

cp ../../examples/python/producer.py producer-test.py
cp ../../examples/python/consumer.py consumer-test.py

cp ../../tests/prod-test.py .
cp ../../tests/con1-test.py .
cp ../../tests/con2-test.py .

cp ../../tests/prod1-test.py .
cp ../../tests/prod2-test.py .
cp ../../tests/con-test.py .
if [ "$workflowType" == "0" ]; then
    # prod-con
    if [ $passthru == 0 ]; then
    	echo "mpirun -n 2 -l python -u wilkins-master.py wilkins_python_test_memory.yaml"
    	mpirun -n 2 -l python -u wilkins-master.py ../../tests/wilkins_python_test_memory.yaml
    else
    	echo "mpirun -n 2 -l python -u wilkins-master.py wilkins_python_test_passthru.yaml"
    	mpirun -n 2 -l python -u wilkins-master.py ../../tests/wilkins_python_test_passthru.yaml
    fi
elif [ "$workflowType" == "1" ]; then
    # prod-2cons
    if [ $passthru == 0 ]; then
        echo "mpirun -n 3 -l python -u wilkins-master.py wilkins_python_test_prod2cons_memory.yaml"
        mpirun -n 3 -l python -u wilkins-master.py ../../tests/wilkins_python_test_prod2cons_memory.yaml
    else
        echo "mpirun -n 3 -l python -u wilkins-master.py wilkins_python_test_prod2cons_passthru.yaml"
        mpirun -n 3 -l python -u wilkins-master.py ../../tests/wilkins_python_test_prod2cons_passthru.yaml
    fi

elif [ "$workflowType" == "2" ]; then
    # 2prod-cons
    if [ $passthru == 0 ]; then
        echo "mpirun -n 3 -l python -u wilkins-master.py wilkins_python_test_2prodcons_memory.yaml"
        mpirun -n 3 -l python -u wilkins-master.py ../../tests/wilkins_python_test_2prodscon_memory.yaml
    else
        echo "mpirun -n 3 -l python -u wilkins-master.py wilkins_python_test_2prodcons_passthru.yaml"
        mpirun -n 3 -l python -u wilkins-master.py ../../tests/wilkins_python_test_2prodscon_passthru.yaml
    fi
fi
retval=$?

if [ -f "$bin_dir/particles.h5" ]; then
    rm "$bin_dir/particles.h5"
fi

if [ -f "$bin_dir/outfile.h5" ]; then
    rm "$bin_dir/outfile.h5"
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
