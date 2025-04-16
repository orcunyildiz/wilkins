import h5py
import numpy as np

def main(raw_args=None):
    print("Hello from the producer1")
    f = h5py.File('outfile1.h5', 'w')
    f.create_dataset("grid", data=np.ones((4, 3, 2), 'f'),maxshape=(None, 3, 2), chunks=True)
    f.close()

if __name__ == "__main__":
    main()
