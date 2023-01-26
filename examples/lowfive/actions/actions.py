dw_counter = 0

def callback(vol):
    def adw_cb():
        global dw_counter
        import sys
        dw_counter = dw_counter + 1
        if dw_counter % 4 == 0:
            print("calling adw callback")
            sys.stdout.flush()
            vol.serve_all(False)
    vol.set_after_dataset_write(adw_cb)
