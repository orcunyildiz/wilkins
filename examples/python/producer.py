import h5py
import numpy as np

def main():

    f = h5py.File('particles.h5', 'w')
    f.create_dataset("data", data=np.ones((4, 3, 2), 'f'))
    f.close()

if __name__ == "__main__":
    main()
