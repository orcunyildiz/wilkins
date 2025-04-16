import h5py

def main(raw_args=None):
    print("Hello from the consumer1")
    f = h5py.File("outfile1.h5", "r")
    data = f["grid"][:]
    print(data)
    f.close()

    import time
    #emulating some computation
    time.sleep(2)
    print("Hello from the consumer1 again")
    f = h5py.File("outfile2.h5", "r")
    data = f["particles"][:]
    print(data)
    f.close()

if __name__ == "__main__":
    main()
