import sys
import os
import time
import yaml


# Load the YAML content from benchmark.yaml
with open("benchmark.yaml", "r") as file:
    yaml_content = file.read()

config = yaml.safe_load(yaml_content)

# Initialize variables
producer_sleep = None
middle_sleep = []
consumer_sleep = None
particles_per_block = None
num_process_producers = None
num_of_middle = None
num_process_middle = []
num_process_consumers = None

# Parse the YAML content
for task in config["tasks"]:
    if task["func"] == "source":
        # producer_count = task.get("count")
        producer_sleep = task.get("sleep")
        particles_per_block = task.get("particle")
        num_process_producers = task.get("nprocs")
    elif task["func"] == "intermediate":
        num_of_middle = task.get("number of intermediate")
        num_process_middle = task.get("nprocs", [])
        middle_sleep = task.get("sleep", [])
        particles_per_block_middle = task.get("particle")
    elif task["func"] == "sink":
        # consumer_count = task.get("count")
        consumer_sleep = task.get("sleep")
        num_process_consumers = task.get("nprocs")

# Create the YAML content with updated arguments
middle_tasks = ""
for i in range(num_of_middle):
    middle_tasks += f'''
  - func: middle{i}
    nprocs: {num_process_middle[i]}
    args: ["1", "{middle_sleep[i]}", "{i}", "{i+1}", "{particles_per_block_middle[i]}"]
    inports:
      - filename: "outfile_{i}.h5"
        dsets:
          - name: /group1/particles
            passthru: 0
            metadata: 1
    outports:
      - filename: "outfile_{i+1}.h5"
        dsets:
          - name: /group1/particles
            passthru: 0
            metadata: 1
'''

yaml_content = f'''\
tasks:
  - func: prod-henson
    nprocs: {num_process_producers}
    args: ["1", "{producer_sleep}", "{particles_per_block}"]
    outports:
      - filename: "outfile_0.h5"
        dsets:
          - name: /group1/particles
            passthru: 0
            metadata: 1
{middle_tasks}
  - func: con-henson
    nprocs: {num_process_consumers}
    args: ["1", "{consumer_sleep}", "{num_of_middle}"]
    inports:
      - filename: "outfile_{num_of_middle}.h5"
        io_freq: 1
        dsets:
          - name: /group1/particles
            passthru: 1
            metadata: 0
'''


# # Create the YAML content with updated arguments
# middle_tasks = ""
# for i in range(num_of_middle):
#     middle_tasks += f'''
#   - func: middle{i}
#     nprocs: {num_process_middle[i]}
#     args: ["1", "{middle_sleep[i]}", "{i}", "{i+1}", "{particles_per_block_middle[i]}"]
#     inports:
#       - filename: "outfile_{i}.h5"
#         dsets:
#           - name: /group1/particles
#             passthru: 0
#             metadata: 1
#     outports:
#       - filename: "outfile_{i+1}.h5"
#         dsets:
#           - name: /group1/particles
#             passthru: 0
#             metadata: 1
# '''

# yaml_content = f'''\
# tasks:
#   - taskCount: {producer_count}
#     func: prod-henson
#     nprocs: {num_process_producers}
#     args: ["1", "{producer_sleep}", "{particles_per_block}"]
#     outports:
#       - filename: "*.h5"
#         dsets:
#           - name: /group1/particles
#             passthru: 0
#             metadata: 1
# {middle_tasks}
#   - taskCount: {consumer_count}
#     func: con-henson
#     nprocs: {num_process_consumers}
#     args: ["1", "{consumer_sleep}", "{num_of_middle}", "{producer_count}"]
#     inports:
#       - filename: "*.h5"
#         io_freq: 1
#         dsets:
#           - name: /group1/particles
#             passthru: 1
#             metadata: 0
# '''


# Write the content to a YAML file
filename = "wilkins_prod_con.yaml"
with open(filename, 'w') as f:
    f.write(yaml_content)

# Copy the middle.hx file for each middle task
for i in range(num_of_middle):
    os.system(f"cp middle.hx middle{i}.hx")

# Construct the command based on the loaded arguments
command = f"mpirun -n {num_process_producers + num_process_consumers + sum(num_process_middle)} -l python -u wilkins-master.py {filename} -s"

# Start timer
start_time = time.time()

# Execute the command using os.system
os.system(command)

# Calculate the execution time
execution_time = time.time() - start_time

# Print the execution time
print("End2end Execution Time:", execution_time, "seconds")

# Remove the middle.hx file for each middle task
for i in range(num_of_middle):
    os.system(f"rm middle{i}.hx")
