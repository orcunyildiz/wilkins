if [[ $1 == '-mpmd' ]]
then
    echo "Running under MPMD mode"
    mpirun -np 3 -l ./producer_1_mpmd wilkins_prod_2cons.yaml : -np 1  ./consumer_1_2_mpmd wilkins_prod_2cons.yaml : -np 1 ./consumer_2_2_mpmd wilkins_prod_2cons.yaml
else
    mpirun -np 5 -l ./wilkins_master wilkins_prod_2cons.yaml
fi
