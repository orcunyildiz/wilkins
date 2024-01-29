dw_counter = 0

def callback(vol, rank):
    def adw_cb():
        global dw_counter
        import sys
        dw_counter = dw_counter + 1
        if dw_counter % 4 == 0:
            print("calling adw callback")
            vol.serve_all(True, True)
    vol.set_after_dataset_write(adw_cb)
    vol.serve_on_close = False
