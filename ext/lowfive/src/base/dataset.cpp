#include <lowfive/vol-base.hpp>

/*-------------------------------------------------------------------------
 * Function:    dataset_create
 *
 * Purpose:     Creates a dataset in a container
 *
 * Return:      Success:    Pointer to a dataset object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void*
LowFive::VOLBase::
_dataset_create(void *obj, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t lcpl_id, hid_t type_id, hid_t space_id,
    hid_t dcpl_id, hid_t dapl_id, hid_t dxpl_id, void **req)
{
    pass_through_t *dset;
    pass_through_t *o = (pass_through_t *)obj;
    void *under;

#ifdef LOWFIVE_ENABLE_PASSTHRU_LOGGING
    printf("------- PASS THROUGH VOL DATASET Create\n");
#endif

    under = o->vol->dataset_create(o->under_object, loc_params, name, lcpl_id, type_id, space_id, dcpl_id, dapl_id, dxpl_id, req);

    if(under) {
        dset = o->create(under);

        /* Check for async request */
        if(req && *req)
            *req = o->create(*req);
    } /* end if */
    else
        dset = NULL;

    return (void *)dset;
} /* end dataset_create() */

void*
LowFive::VOLBase::
dataset_create(void *obj, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t lcpl_id, hid_t type_id, hid_t space_id,
    hid_t dcpl_id, hid_t dapl_id, hid_t dxpl_id, void **req)
{
    return H5VLdataset_create(obj, loc_params, info.under_vol_id, name, lcpl_id, type_id, space_id, dcpl_id,  dapl_id, dxpl_id, req);
}

/*-------------------------------------------------------------------------
 * Function:    dataset_open
 *
 * Purpose:     Opens a dataset in a container
 *
 * Return:      Success:    Pointer to a dataset object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void*
LowFive::VOLBase::
_dataset_open(void *obj, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t dapl_id, hid_t dxpl_id, void **req)
{
    pass_through_t *dset;
    pass_through_t *o = (pass_through_t *)obj;
    void *under;

#ifdef ENABLE_PASSTHRU_LOGGING
    printf("------- PASS THROUGH VOL DATASET Open\n");
#endif

    under = o->vol->dataset_open(o->under_object, loc_params, name, dapl_id, dxpl_id, req);

    if(under) {
        dset = o->create(under);

        /* Check for async request */
        if(req && *req)
            *req = o->create(*req);
    } /* end if */
    else
        dset = NULL;

    return (void *)dset;
} /* end dataset_open() */

void*
LowFive::VOLBase::
dataset_open(void *obj, const H5VL_loc_params_t *loc_params,
    const char *name, hid_t dapl_id, hid_t dxpl_id, void **req)
{
    return H5VLdataset_open(obj, loc_params, info.under_vol_id, name, dapl_id, dxpl_id, req);
}

/*-------------------------------------------------------------------------
 * Function:    dataset_read
 *
 * Purpose:     Reads data elements from a dataset into a buffer.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
herr_t
LowFive::VOLBase::
_dataset_read(void *dset, hid_t mem_type_id, hid_t mem_space_id, hid_t file_space_id, hid_t plist_id, void *buf, void **req)
{
    pass_through_t *o = (pass_through_t *)dset;
    herr_t ret_value;

#ifdef LOWFIVE_ENABLE_PASSTHRU_LOGGING 
    printf("------- PASS THROUGH VOL DATASET Read\n");
#endif

    ret_value = o->vol->dataset_read(o->under_object, mem_type_id, mem_space_id, file_space_id, plist_id, buf, req);

    /* Check for async request */
    if(req && *req)
        *req = o->create(*req);

    return ret_value;
} /* end dataset_read() */

herr_t
LowFive::VOLBase::
dataset_read(void *dset, hid_t mem_type_id, hid_t mem_space_id, hid_t file_space_id, hid_t plist_id, void *buf, void **req)
{
    return H5VLdataset_read(dset, info.under_vol_id, mem_type_id, mem_space_id, file_space_id, plist_id, buf, req);
}

/*-------------------------------------------------------------------------
 * Function:    dataset_write
 *
 * Purpose:     Writes data elements from a buffer into a dataset.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
herr_t
LowFive::VOLBase::
_dataset_write(void *dset, hid_t mem_type_id, hid_t mem_space_id, hid_t file_space_id, hid_t plist_id, const void *buf, void **req)
{
    pass_through_t *o = (pass_through_t *)dset;
    herr_t ret_value;

#ifdef LOWFIVE_ENABLE_PASSTHRU_LOGGING 
    printf("------- PASS THROUGH VOL DATASET Write\n");
#endif

    ret_value = o->vol->dataset_write(o->under_object, mem_type_id, mem_space_id, file_space_id, plist_id, buf, req);

    /* Check for async request */
    if(req && *req)
        *req = o->create(*req);

    return ret_value;
} /* end dataset_write() */

herr_t
LowFive::VOLBase::
dataset_write(void *dset, hid_t mem_type_id, hid_t mem_space_id, hid_t file_space_id, hid_t plist_id, const void *buf, void **req)
{
    return H5VLdataset_write(dset, info.under_vol_id, mem_type_id, mem_space_id, file_space_id, plist_id, buf, req);
}

/*-------------------------------------------------------------------------
 * Function:    dataset_get
 *
 * Purpose:     Gets information about a dataset
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
herr_t
LowFive::VOLBase::
_dataset_get(void *dset, H5VL_dataset_get_t get_type, hid_t dxpl_id, void **req, va_list arguments)
{
    pass_through_t *o = (pass_through_t *)dset;
    herr_t ret_value;

#ifdef LOWFIVE_ENABLE_PASSTHRU_LOGGING
    printf("------- PASS THROUGH VOL DATASET Get\n");
#endif

    ret_value = o->vol->dataset_get(o->under_object, get_type, dxpl_id, req, arguments);

    /* Check for async request */
    if(req && *req)
        *req = o->create(*req);

    return ret_value;
} /* end pass_through_dataset_get() */

herr_t
LowFive::VOLBase::
dataset_get(void *dset, H5VL_dataset_get_t get_type, hid_t dxpl_id, void **req, va_list arguments)
{
    return H5VLdataset_get(dset, info.under_vol_id, get_type, dxpl_id, req, arguments);
}

/*-------------------------------------------------------------------------
 * Function:    dataset_close
 *
 * Purpose:     Closes a dataset.
 *
 * Return:      Success:    0
 *              Failure:    -1, dataset not closed.
 *
 *-------------------------------------------------------------------------
 */
herr_t
LowFive::VOLBase::
_dataset_close(void *dset, hid_t dxpl_id, void **req)
{
    pass_through_t *o = (pass_through_t *)dset;
    herr_t ret_value;

#ifdef LOWFIVE_ENABLE_PASSTHRU_LOGGING 
    printf("------- PASS THROUGH VOL DATASET Close\n");
#endif

    ret_value = o->vol->dataset_close(o->under_object, dxpl_id, req);

    /* Check for async request */
    if(req && *req)
        *req = o->create(*req);

    /* Release our wrapper, if underlying dataset was closed */
    if(ret_value >= 0)
        pass_through_t::destroy(o);

    return ret_value;
} /* end dataset_close() */

herr_t
LowFive::VOLBase::
dataset_close(void *dset, hid_t dxpl_id, void **req)
{
    return H5VLdataset_close(dset, info.under_vol_id, dxpl_id, req);
}