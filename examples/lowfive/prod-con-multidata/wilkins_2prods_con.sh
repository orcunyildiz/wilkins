if [[ $1 == '-mpmd' ]]
then
    echo "Running under MPMD mode"
    mpirun -np 3 -l ./producer_1_2_mpmd wilkins_2prods_con.yaml : -np 1  ./producer_2_2_mpmd wilkins_2prods_con.yaml : -np 1 ./consumer_1_mpmd wilkins_2prods_con.yaml
else
    mpirun -np 5 -l ./wilkins_master wilkins_2prods_con.yaml
fi
