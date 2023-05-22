if [[ $1 == '-single' ]]
then
    echo "Generating same file over the course of simulation execution"
    mpirun -n 2 -l python -u wilkins-master.py wilkins_prod_con_singleFile.yaml -s
else
    echo "Generating different files over the course of simulation execution"
    mpirun -n 2 -l python -u wilkins-master.py wilkins_prod_con.yaml -s
fi


