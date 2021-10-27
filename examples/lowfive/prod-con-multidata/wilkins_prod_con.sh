if [[ $1 == '-mpmd' ]]
then
    echo "Running under MPMD mode"
    mpirun -np 3 -l ./producer_mpmd wilkins_prod_con.yaml : -np 1  ./consumer_mpmd wilkins_prod_con.yaml
else
    mpirun -np 4 -l ./wilkins_master wilkins_prod_con.yaml
fi
