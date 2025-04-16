import h5py
import numpy as np

def main(raw_args=None):
    f = h5py.File('outfile.h5', 'w')
    f.create_dataset("particles", data=np.ones((4, 3, 2), 'f'))

    f.create_dataset("grid", data=np.full((4, 3, 2), 4, 'f'))
    f.close()

if __name__ == "__main__":
    main()
