#!/bin/bash

if [ $# -lt 1 ]; then
    echo "Usage: $0 <yaml_config_file>"
    exit 1
fi

yaml_file="$1"

# Check if the yaml file exists
if [ ! -f "$yaml_file" ]; then
    echo "Error: File '$yaml_file' not found."
    exit 1
fi

rm -f run_ensemble.sh

cat "$yaml_file" | grep func | awk -F: '{print substr($2,2)}' > func.txt
cat "$yaml_file" | grep nprocs | awk -F: '{print $2}' > nprocs.txt
cat "$yaml_file" | grep taskCount | awk -F: '{print $2}' > nodeCount.txt

count=$(cat nprocs.txt | wc -l)
np=0
for ((i=1;i<=$count;i++))
do
    nodeCount=$(awk 'NR=='$i'' nodeCount.txt)
    for ((j=0;j<$nodeCount;j++))
    do
        nprocs=$(awk 'NR=='$i'' nprocs.txt)
        ((np+=nprocs))
        func=$(awk 'NR=='$i'' func.txt)
        cp "$func".hx "$func"_"$j".hx
    done
done

echo "mpirun -l -n $np python -u wilkins-master.py $yaml_file" > run_ensemble.sh

chmod +x run_ensemble.sh

# Cleanup
rm func.txt nprocs.txt nodeCount.txt

echo "Created run_ensemble.sh with $np processes for configuration in $yaml_file"
