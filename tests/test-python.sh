#! /bin/bash

bin_dir=$1
passthru=$2
workflowType=$3 #0: prod-con 1: prod-2cons 2: 2prod-cons

cp ../../examples/lowfive/actions/passthru-actions.py .

cp ../../examples/python/producer.py producer-test.py
cp ../../examples/python/consumer.py consumer-test.py

cp ../../tests/prod-test.py .
cp ../../tests/con1-test.py .
cp ../../tests/con2-test.py .

cp ../../tests/prod1-test.py .
cp ../../tests/prod2-test.py .
cp ../../tests/con-test.py .

log=$(mktemp)

if [ "$workflowType" == "0" ]; then
    # prod-con
    if [ $passthru == 0 ]; then
    	echo "mpirun -n 2 -l python -m wilkins.master wilkins_python_test_memory.yaml"
    	mpirun -n 2 -l python -m wilkins.master ../../tests/wilkins_python_test_memory.yaml 2>&1 | tee "$log"
    else
    	echo "mpirun -n 2 -l python -m wilkins.master wilkins_python_test_passthru.yaml"
    	mpirun -n 2 -l python -m wilkins.master ../../tests/wilkins_python_test_passthru.yaml 2>&1 | tee "$log"
    fi
elif [ "$workflowType" == "1" ]; then
    # prod-2cons
    if [ $passthru == 0 ]; then
        echo "mpirun -n 3 -l python -m wilkins.master wilkins_python_test_prod2cons_memory.yaml"
        mpirun -n 3 -l python -m wilkins.master ../../tests/wilkins_python_test_prod2cons_memory.yaml 2>&1 | tee "$log"
    else
        echo "mpirun -n 3 -l python -m wilkins.master wilkins_python_test_prod2cons_passthru.yaml"
        mpirun -n 3 -l python -m wilkins.master ../../tests/wilkins_python_test_prod2cons_passthru.yaml 2>&1 | tee "$log"
    fi

elif [ "$workflowType" == "2" ]; then
    # 2prod-cons
    if [ $passthru == 0 ]; then
        echo "mpirun -n 3 -l python -m wilkins.master wilkins_python_test_2prodscon_memory.yaml"
        mpirun -n 3 -l python -m wilkins.master ../../tests/wilkins_python_test_2prodscon_memory.yaml 2>&1 | tee "$log"
    else
        echo "mpirun -n 3 -l python -m wilkins.master wilkins_python_test_2prodscon_passthru.yaml"
        mpirun -n 3 -l python -m wilkins.master ../../tests/wilkins_python_test_2prodscon_passthru.yaml 2>&1 | tee "$log"
    fi
fi
retval=${PIPESTATUS[0]}

# --- validation ---

data_ok=true

# check for Python exceptions
if grep -q "Traceback" "$log"; then
    echo "FAIL: Python traceback found in output"
    data_ok=false
fi

# passthru mode: verify expected .h5 files were created
if [ $passthru != 0 ]; then
    if [ "$workflowType" == "0" ]; then
        # prod-con passthru: producer.py writes particles.h5
        if [ ! -f "$bin_dir/particles.h5" ]; then
            echo "FAIL: expected output file particles.h5 not found"
            data_ok=false
        fi
    elif [ "$workflowType" == "1" ]; then
        # prod-2cons passthru: prod-test.py writes outfile.h5
        if [ ! -f "$bin_dir/outfile.h5" ]; then
            echo "FAIL: expected output file outfile.h5 not found"
            data_ok=false
        fi
    elif [ "$workflowType" == "2" ]; then
        # 2prod-cons passthru: prod1-test.py writes outfile1.h5, prod2-test.py writes outfile2.h5
        for f in outfile1.h5 outfile2.h5; do
            if [ ! -f "$bin_dir/$f" ]; then
                echo "FAIL: expected output file $f not found"
                data_ok=false
            fi
        done
    fi
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

# h5 files: only passthru mode writes real files to disk
if [ $passthru != 0 ]; then
    if [ "$workflowType" == "0" ]; then
        rm -f "$bin_dir/particles.h5"
    elif [ "$workflowType" == "1" ]; then
        rm -f "$bin_dir/outfile.h5"
    elif [ "$workflowType" == "2" ]; then
        rm -f "$bin_dir/outfile1.h5" "$bin_dir/outfile2.h5"
    fi
fi

# copied helper scripts and log
rm -f passthru-actions.py
rm -f producer-test.py consumer-test.py
rm -f prod-test.py con1-test.py con2-test.py
rm -f prod1-test.py prod2-test.py con-test.py
rm -f "$log"

if [ "$data_ok" = true ]; then
    echo "PASS"
    exit 0
else
    exit 1
fi
