#!/usr/bin/env python
import sys
import yaml
import os
import argparse

def generate_run_scripts(config_file, generate_all=False, scheduler=None, walltime="01:00:00", cores_per_node=32):
    # Get absolute path of config file
    config_file_abs_path = os.path.abspath(config_file)
    config_dir = os.path.dirname(config_file_abs_path)
    config_file_name = os.path.basename(config_file_abs_path)
    
    # Read the YAML configuration
    with open(config_file_abs_path, 'r') as f:
        config = yaml.safe_load(f)
    
    tasks = config['tasks']
    total_procs = sum(task.get('nprocs', 1) for task in tasks)
    
    # Base command to run the wilkins-master
    base_cmd = f"python -u wilkins-master.py {config_file_name}"
    
    if generate_all or scheduler == "mpi":
        generate_mpi_script(config_file_name, config_dir, base_cmd, total_procs)
    
    if generate_all or scheduler == "slurm":
        generate_slurm_script(config_file_name, config_dir, base_cmd, total_procs, walltime)
        
    if generate_all or scheduler == "pbs":
        generate_pbs_script(config_file_name, config_dir, base_cmd, total_procs, walltime, cores_per_node)

def generate_mpi_script(config_file, config_dir, base_cmd, total_procs):
    script_content = f"""#!/bin/bash

# Auto-generated MPI run script for {config_file}
cd {config_dir}
mpirun -n {total_procs} -l {base_cmd}
"""
    
    script_path = os.path.join(config_dir, "run_wilkins_mpi.sh")
    with open(script_path, "w") as script_file:
        script_file.write(script_content)
    
    os.chmod(script_path, 0o755)
    
    print(f"MPI run script generated as '{script_path}'")
    print(f"Command: mpirun -n {total_procs} -l {base_cmd}")

def generate_slurm_script(config_file, config_dir, base_cmd, total_procs, walltime):
    script_content = f"""#!/bin/bash
#SBATCH --account={{user_account}}
#SBATCH --job-name=wilkins_job
#SBATCH --output=wilkins_%j.out
#SBATCH --error=wilkins_%j.err
#SBATCH --ntasks={total_procs}
#SBATCH --time={walltime}

# Change to the directory containing the configuration file
cd {config_dir}

# Load any required modules
# module load mpich

# Run the application
srun -n {total_procs} {base_cmd}
"""
    script_path = os.path.join(config_dir, "run_wilkins_slurm.sh")
    with open(script_path, "w") as script_file:
        script_file.write(script_content)
    
    os.chmod(script_path, 0o755)
    
    print(f"SLURM run script generated as '{script_path}'")
    print(f"Submit with: sbatch {script_path}")

def generate_pbs_script(config_file, config_dir, base_cmd, total_procs, walltime, cores_per_node):
    nodes = (total_procs + cores_per_node - 1) // cores_per_node
    ppn = min(total_procs, cores_per_node)
    
    script_content = f"""#!/bin/bash
#PBS -A {{user_account}}
#PBS -N wilkins_job
#PBS -o wilkins.out
#PBS -e wilkins.err
#PBS -l nodes={nodes}:ppn={ppn}
#PBS -l walltime={walltime}

# Change to the directory containing the configuration file
cd {config_dir}

# Load any required modules
# module load mpich

# Run the application
mpirun -n {total_procs} {base_cmd}
"""
    script_path = os.path.join(config_dir, "run_wilkins_pbs.sh")
    with open(script_path, "w") as script_file:
        script_file.write(script_content)
    
    os.chmod(script_path, 0o755)
    
    print(f"PBS run script generated as '{script_path}'")
    print(f"Submit with: qsub {script_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Generate run scripts for MPI, SLURM, or PBS')
    parser.add_argument('config_file', help='YAML configuration file')
    parser.add_argument('--all', action='store_true', help='Generate scripts for all schedulers')
    parser.add_argument('--scheduler', choices=['mpi', 'slurm', 'pbs'], 
                        help='Generate script for specific scheduler')
    parser.add_argument('--walltime', default="01:00:00", 
                        help='Wall clock time limit (format: HH:MM:SS, default: 01:00:00)')
    parser.add_argument('--cores-per-node', type=int, default=32, 
                        help='Number of cores per node (default: 32)')
    
    args = parser.parse_args()
    
    if not args.all and args.scheduler is None:
        # Default to MPI if no scheduler specified
        args.scheduler = 'mpi'
    
    generate_run_scripts(
        args.config_file, 
        args.all, 
        args.scheduler, 
        args.walltime, 
        args.cores_per_node
    )
