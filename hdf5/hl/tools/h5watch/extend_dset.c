/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Board of Trustees of the University of Illinois.         *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the files COPYING and Copyright.html.  COPYING can be found at the root   *
 * of the source code distribution tree; Copyright.html can be found at the  *
 * root level of an installed copy of the electronic HDF5 document set and   *
 * is linked from the top-level documents page.  It can also be found at     *
 * http://hdfgroup.org/HDF5/doc/Copyright.html.  If you do not have          *
 * access to either file, you may request a copy from help@hdfgroup.org.     *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "h5hltest.h"

/*
 * Extending datasets in WATCH.h5 generated by h5watchgentest.c
 */
#define DSET_ONE "DSET_ONE"
#define DSET_TWO "DSET_TWO"
#define DSET_CMPD "DSET_CMPD"
#define DSET_CMPD_ESC "DSET_CMPD_ESC"
#define DSET_CMPD_TWO "DSET_CMPD_TWO"
#define DSET_ALLOC_LATE "DSET_ALLOC_LATE"
#define DSET_ALLOC_EARLY "DSET_ALLOC_EARLY"

/* The message sent by this process (extend_dset) to the test script to start "h5watch" */
#define WRITER_MESSAGE  "writer_message"
/* The message received from the test script to start extending dataset */
#define READER_MESSAGE  "reader_message"

/* Size of data buffer */
#define TEST_BUF_SIZE 100

/*
 * Test variations (incremental) for one-dimensional dataset:
 *     Varies from 10->13->12->12->1->3
 */
#define ONE_NTESTS    5
int one_tests[ONE_NTESTS] = {3, -1, 0, -11, 2};

/*
 * Test variations (incremental) for two-dimensional dataset:
 *    Varies from {4,10}->{6,12}->{8,1}->{10,1}->
 *                             {3,3}->{2,2}->{1,2}->
 *                    {1,4}->{1,3}->{1,3}
 */
#define TWO_NTESTS  9
int two_tests[TWO_NTESTS][2] = { {2, 2}, {2, -11}, {2, 0},
                                 {-7, 2}, {-1, -1}, {-1, 0},
                                 {0, 2}, {0, -1}, {0, 0}
                };

static herr_t extend_dset_two(const char *file, char *dname);
static herr_t extend_dset_one(const char *file, char *dname);


/* Data structures for datasets with compound data type */
typedef struct sub22_t {
    unsigned int a;
    unsigned int b;
    unsigned int c;
} sub22_t;

typedef struct sub2_t {
    unsigned int a;
    sub22_t b;
    unsigned int c;
} sub2_t;

typedef struct sub4_t {
    unsigned int a;
    unsigned int b;
} sub4_t;

typedef struct set_t {
    unsigned int field1;
    sub2_t field2;
    double field3;
    sub4_t field4;
} set_t;

/*
 ***********************************************************************
 *
 * Extending a two-dimensional dataset:
 *              dims[0]         dims[1]
 *                      -------         -------
 *      case #1:        increase        increase
 *      case #2:        increase        decrease
 *      case #3:        increase        same
 *      case #4:        decrease        increase
 *      case #5:        decrease        decrease        (no action)
 *      case #6:        decrease        same            (no action)
 *      case #7:        same            increase
 *      case #8:        same            decrease        (no action)
 *      case #9:        same            same            (no action)
 *
 *    two_tests[TWO_NTESTS][2] = { {2,2}, {2,-11}, {2,0},
 *                                   {-7,2}, {-1,-1}, {-1,0},
 *                                   {0,2}, {0,-1}, {0,0} }
 *    varies from {4,10}->{6,12}->{8,1}->{10,1}->
 *                             {3,3}->{2,2}->{1,2}->
 *                    {1,4}->{1,3}->{1,3}
 ***********************************************************************
 */
static herr_t
extend_dset_two(const char *file, char *dname)
{
    hid_t fid = -1;         /* file id                                          */
    hid_t fapl = -1;        /* file access property list id                     */
    hid_t did = -1;         /* dataset id                                       */
    hid_t sid = -1;         /* dataspace id                                     */
    hid_t dtid = -1;        /* dataset's datatype id                            */
    int ndims;              /* # of dimension sizes                             */
    unsigned i, j;          /* local index variable                             */
    hsize_t ext_dims[2];    /* new dimension sizes after extension              */
    hsize_t cur_dims[2];    /* current dimension sizes                          */
    size_t dtype_size;      /* size of the dataset's datatype                   */
    unsigned num_elmts;     /* number of elements in the dataset                */
    int *ibuf = NULL;       /* buffer for storing retrieved elements (integer)  */
    set_t *cbuf = NULL;     /* buffer for storing retrieved elemnets (compound) */

    /* Allocate memory */
    if(NULL == (ibuf = (int *)HDcalloc(TEST_BUF_SIZE, sizeof(int))))
        goto error;
    if(NULL == (cbuf = (set_t *)HDcalloc(TEST_BUF_SIZE, sizeof(set_t))))
        goto error;

    /* Create a copy of file access property list */
    if((fapl = H5Pcreate(H5P_FILE_ACCESS)) < 0)
        goto error;

    /* Set to use the latest library format */
    if(H5Pset_libver_bounds(fapl, H5F_LIBVER_LATEST, H5F_LIBVER_LATEST) < 0)
        goto error;

    /* Open the file and dataset with SWMR write */
    if((fid = H5Fopen(file, H5F_ACC_RDWR|H5F_ACC_SWMR_WRITE, fapl)) < 0)
        goto error;

    if((did = H5Dopen2(fid, dname, H5P_DEFAULT)) < 0)
        goto error;

    /* Send message to the test script to start "h5watch" */
    h5_send_message(WRITER_MESSAGE, NULL, NULL);

    if((sid = H5Dget_space(did)) < 0)
        goto error;

    if((ndims = H5Sget_simple_extent_ndims(sid)) < 0)
        goto error;

    /* Get the size of the dataset's datatype */
    if((dtype_size = H5LDget_dset_type_size(did, NULL)) == 0)
        goto error;

    /* Get the dataset's data type */
    if((dtid = H5Tget_native_type(H5Dget_type(did), H5T_DIR_DEFAULT)) < 0)
        goto error;

    /* Wait for message from the test script to start extending dataset */
    if(h5_wait_message(READER_MESSAGE) < 0)
        goto error;

    /* Loop through different variations of extending the dataset */
    for(i = 0; i < TWO_NTESTS; i++) {

        /* sleep to emulate about 2 seconds of application operation */
        HDsleep(2);

        /* Get current dimension sizes */
        if(H5LDget_dset_dims(did, cur_dims) < 0)
            goto error;

        /* Set up the new extended dimension sizes  */
        ext_dims[0] = cur_dims[0] + (hsize_t)two_tests[i][0];
        ext_dims[1] = cur_dims[1] + (hsize_t)two_tests[i][1];

        /* Extend the dataset */
        if(H5Dset_extent(did, ext_dims) < 0)
            goto error;

        num_elmts = 1;
        for(j = 0; j < (unsigned)ndims; j++)
            num_elmts *= (unsigned)ext_dims[j];

        /* Compound type */
        if(!HDstrcmp(dname, DSET_CMPD_TWO)) {

            HDmemset(cbuf, 0, TEST_BUF_SIZE * sizeof(set_t));
            for(j = 0; j < num_elmts; j++) {
                cbuf[j].field1 = i + 1;
                cbuf[j].field2.a = i + 1;
                cbuf[j].field2.c = i + 1;
                cbuf[j].field2.b.a = i + 1;
                cbuf[j].field2.b.b = i + 1;
                cbuf[j].field2.b.c = i + 1;
                cbuf[j].field3 = i + 1;
                cbuf[j].field4.a = i + 1;
                cbuf[j].field4.b = i + 1;
            } /* end for */

            /* Write to the dataset */
            if(H5Dwrite(did, dtid, H5S_ALL, H5S_ALL, H5P_DEFAULT, cbuf) < 0)
                goto error;
        } else { /* Integer type */
            HDmemset(ibuf, 0, TEST_BUF_SIZE * sizeof(int));
            for(j = 0; j < num_elmts; j++)
                ibuf[j] = (int)(i + 1);

            /* Write to the dataset */
            if(H5Dwrite(did, dtid, H5S_ALL, H5S_ALL, H5P_DEFAULT, ibuf) < 0)
                goto error;
        } /* end if-else */

        if(H5Dflush(did) < 0)
            goto error;

    } /* end for TWO_NTESTS */

    /* Closing */
    if(H5Tclose(dtid) < 0) goto error;
    if(H5Dclose(did) < 0) goto error;
    if(H5Pclose(fapl) < 0) goto error;
    if(H5Fclose(fid) < 0) goto error;

    HDfree(ibuf);
    HDfree(cbuf);

    return SUCCEED;

error:
    H5E_BEGIN_TRY
        H5Tclose(dtid);
        H5Dclose(did);
        H5Pclose(fapl);
        H5Fclose(fid);
    H5E_END_TRY

    if(ibuf)
        HDfree(ibuf);
    if(cbuf)
        HDfree(cbuf);

    return FAIL;

} /* end extend_dset_two() */

/*
 ***********************************************************************
 *
 * Extending a one-dimensional dataset
 *    Test cases:
 *        #1: increase
 *        #2: decrease
 *        #3: same
 *        #4: decrease
 *        #5: increase
 *
 *     one_tests[ONE_NTESTS] = {3, -1, 0, -11, 2}
 *     varies from 10->13->12->12->1->3
 *
 ***********************************************************************
 */
static herr_t
extend_dset_one(const char *file, char *dname)
{
    hid_t fid = -1;         /* file id                                          */
    hid_t fapl = -1;        /* file access property list id                     */
    hid_t did = -1;         /* dataset id                                       */
    hid_t dtid = -1;        /* dataset's datatype id                            */
    hid_t sid = -1;         /* dataspace id                                     */
    hid_t mid = -1;         /* memory space id                                  */
    unsigned i, j;          /* local index variable                             */
    hsize_t cur_dims[1];    /* current dimension sizes                          */
    hsize_t ext_dims[1];    /* new dimension sizes after extension              */
    hsize_t offset[1];      /* starting offsets of appended data                */
    hsize_t count[1];       /* dimension sizes of appended data                 */
    size_t dtype_size;      /* size of the dataset's datatype                   */
    int *ibuf = NULL;       /* buffer for storing retrieved elements (integer)  */
    set_t *cbuf = NULL;     /* buffer for storing retrieved elemnets (compound) */

    /* Allocate memory */
    if(NULL == (ibuf = (int *)HDcalloc(TEST_BUF_SIZE, sizeof(int))))
        goto error;
    if(NULL == (cbuf = (set_t *)HDcalloc(TEST_BUF_SIZE, sizeof(set_t))))
        goto error;

    /* Create a copy of file access property list */
    if((fapl = H5Pcreate(H5P_FILE_ACCESS)) < 0)
        goto error;

    /* Set to use the latest library format */
    if(H5Pset_libver_bounds(fapl, H5F_LIBVER_LATEST, H5F_LIBVER_LATEST) < 0)
        goto error;

    /* Open the file and dataset with SWMR write */
    if((fid = H5Fopen(file, H5F_ACC_RDWR|H5F_ACC_SWMR_WRITE, fapl)) < 0)
        goto error;

    /* Send message to the test script to start "h5watch" */
    h5_send_message(WRITER_MESSAGE, NULL, NULL);

    if((did = H5Dopen2(fid, dname, H5P_DEFAULT)) < 0)
        goto error;

    /* Get size of the dataset's datatype */
    if((dtype_size = H5LDget_dset_type_size(did, NULL)) == 0)
        goto error;

    /* Get dataset's datatype */
    if((dtid = H5Tget_native_type(H5Dget_type(did), H5T_DIR_DEFAULT)) < 0)
        goto error;

    /* Wait for message from the test script to start extending dataset */
    if(h5_wait_message(READER_MESSAGE) < 0)
        goto error;

    /* Loop through different variations of extending the dataset */
    for(i = 0; i < ONE_NTESTS; i++) {

        /* sleep to emulate about 2 seconds of application operation */
        HDsleep(2);

        /* Get current dimension sizes */
        if(H5LDget_dset_dims(did, cur_dims) < 0)
            goto error;

        /* Set up the new extended dimension sizes  */
        ext_dims[0] = cur_dims[0] + (hsize_t)one_tests[i];

        /* Extend the dataset */
        if(H5Dset_extent(did, ext_dims) < 0)
            goto error;

        /* Write to the new appended region of the dataset */
        if(one_tests[i] > 0) {

            /* Select the extended region */
            offset[0] = cur_dims[0];
            count[0] = (hsize_t)one_tests[i];
            if((sid = H5Dget_space(did)) < 0)
                goto error;
            if(H5Sselect_hyperslab(sid, H5S_SELECT_SET, offset, NULL, count, NULL) < 0)
                goto error;

            /* Set up memory space and get dataset's datatype */
            if((mid = H5Screate_simple(1, count, NULL)) < 0)
                goto error;

            /* Initialize data for the extended region of the dataset */
            /* Compound type */
            if(!HDstrcmp(dname, DSET_CMPD) || !HDstrcmp(dname, DSET_CMPD_ESC)) {

                HDmemset(cbuf, 0, TEST_BUF_SIZE * sizeof(set_t));
                for(j = 0; j < (unsigned)one_tests[i]; j++) {
                    cbuf[j].field1 = j + 1;
                    cbuf[j].field2.a = j + 2;
                    cbuf[j].field2.b.a = j + 2;
                    cbuf[j].field2.b.b = j + 2;
                    cbuf[j].field2.b.c = j + 2;
                    cbuf[j].field2.c = j + 2;

                    cbuf[j].field3 = j + 3;

                    cbuf[j].field4.a = j + 4;
                    cbuf[j].field4.b = j + 4;
                } /* end for */

                /* Write to the extended region of the dataset */
                if(H5Dwrite(did, dtid, mid, sid, H5P_DEFAULT, cbuf) < 0)
                    goto error;
            } else { /* Integer type */

                HDmemset(ibuf, 0, TEST_BUF_SIZE * sizeof(int));
                for(j = 0; j < (unsigned)one_tests[i]; j++)
                    ibuf[j] = (int)j;

                /* Write to the extended region of the dataset */
                if(H5Dwrite(did, dtid, mid, sid, H5P_DEFAULT, ibuf) < 0)
                    goto error;
            } /* end if-else */

            /* Closing */
            if(H5Sclose(sid) < 0) goto error;
            if(H5Sclose(mid) < 0) goto error;
        } /* end if */

        if(H5Dflush(did) < 0)
            goto error;

    } /* end for ONE_NTESTS */

    /* Closing */
    if(H5Tclose(dtid) < 0) goto error;
    if(H5Dclose(did) < 0) goto error;
    if(H5Pclose(fapl) < 0) goto error;
    if(H5Fclose(fid) < 0) goto error;

    HDfree(ibuf);
    HDfree(cbuf);

    return SUCCEED;

error:
    H5E_BEGIN_TRY
        H5Sclose(sid);
        H5Sclose(mid);
        H5Tclose(dtid);
        H5Dclose(did);
        H5Pclose(fapl);
        H5Fclose(fid);
    H5E_END_TRY

    if(ibuf)
        HDfree(ibuf);
    if(cbuf)
        HDfree(cbuf);

    return FAIL;
} /* end extend_dset_one() */

/* Usage: extend_dset xx.h5 dname */
int
main(int argc, const char *argv[])
{
    char *dname = NULL;
    char *fname = NULL;

    if(argc != 3) {
        HDfprintf(stderr, "Should have file name and dataset name to be extended...\n");
        goto error;
    } /* end if */

    /* Get the dataset name to be extended */
    fname = HDstrdup(argv[1]);
    dname = HDstrdup(argv[2]);

    if(!HDstrcmp(dname, DSET_CMPD) || !HDstrcmp(dname, DSET_CMPD_ESC)) {
        if(extend_dset_one(fname, dname) < 0)
            goto error;
    } else if(!HDstrcmp(dname, DSET_ONE) || !HDstrcmp(dname, DSET_ALLOC_LATE) || !HDstrcmp(dname, DSET_ALLOC_EARLY)) {
        if(extend_dset_one(fname, dname) < 0)
            goto error;
    } else if(!HDstrcmp(dname, DSET_TWO) || !HDstrcmp(dname, DSET_CMPD_TWO)) {
        if(extend_dset_two(fname, dname) < 0)
            goto error;
    } else {
        HDfprintf(stdout, "Dataset cannot be extended...\n");
        goto error;
    } /* end if-else */

    HDexit(EXIT_SUCCESS);

error:
    if(dname)
        HDfree(dname);
    if(fname)
        HDfree(fname);
    HDexit(EXIT_FAILURE);
} /* end main() */

