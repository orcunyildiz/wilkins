rm runWilkins.sh
echo -n "mpirun -l " >> runWilkins.sh

cat wilkins_prod_con.yaml | grep cmdline | awk -F: '{print $2}' > cmdline.txt
cat wilkins_prod_con.yaml | grep nprocs | awk -F: '{print $2}' > nprocs.txt
cat wilkins_prod_con.yaml | grep nodeCount | awk -F: '{print $2}' > nodeCount.txt

count=$(cat nprocs.txt | wc -l)
for ((i=1;i<=$count;i++))
do
    nodeCount=$(awk 'NR=='$i'' nodeCount.txt)
    for ((j=0;j<$nodeCount;j++))
    do
        echo -n "-np " >> runWilkins.sh
        nprocs=$(awk 'NR=='$i'' nprocs.txt)
        echo -n $nprocs  >> runWilkins.sh
      	echo -n " " >> runWilkins.sh
        cmdline=$(awk 'NR=='$i'' cmdline.txt)
        echo -n $cmdline >> runWilkins.sh
        echo -n " " >> runWilkins.sh
        echo -n $j >> runWilkins.sh
        if [ $i -lt $count ] || [ $(( $j + 1)) -lt $nodeCount ]
        then
            echo -n " : " >> runWilkins.sh
        fi
    done
done

rm cmdline.txt nprocs.txt nodeCount.txt

