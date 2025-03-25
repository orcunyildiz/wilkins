import h5py

def main(task_args=None): #optional task_args
    """
    :param task_args: command-line args parser
    """

    f = h5py.File("particles.h5", "r")
    data = f["data"][:]
    print(data)
    f.close()

if __name__ == "__main__":
    main()
