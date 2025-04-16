import h5py
import numpy as np

def main(raw_args=None):
    print("Hello from the producer2")
    f = h5py.File('outfile2.h5', 'w')
    f.create_dataset("particles", data=np.full((4, 3, 2), 4, 'f'),maxshape=(None, 3, 2), chunks=True) 
    f.close()

if __name__ == "__main__":
    main()
