import h5py

def main(raw_args=None):

    f = h5py.File("outfile.h5", "r")
    data = f["particles"][:]
    print(data)
    f.close()

    import time
    #emulating some computation
    time.sleep(2)

if __name__ == "__main__":
    main()
