#! /bin/bash

rm run_ensemble.sh
echo "mpirun -l -n \c" >> run_ensemble.sh

cat wilkins_prod_con.yaml | grep func | awk -F: '{print substr($2,2)}' > func.txt
cat wilkins_prod_con.yaml | grep nprocs | awk -F: '{print $2}' > nprocs.txt
cat wilkins_prod_con.yaml | grep taskCount | awk -F: '{print $2}' > nodeCount.txt

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
echo "$np\c"  >> run_ensemble.sh
echo " python -u wilkins-master.py wilkins_prod_con.yaml -s " >> run_ensemble.sh

#cleanup
rm func.txt nprocs.txt nodeCount.txt
