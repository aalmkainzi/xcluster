/*
XList stands for "Exploding Array",
when deleting, it splits the the hole into 2 arrays,
in order to maintain pointer/iterator stability of other elements.



when inserting, it looks into stack of non_full_buckets, if empty, alloc new bucket.

when deleting, split into 2 arrays, left one is full from the start, right one will keep the old cap + left_arr_size

but how will iterators be stable? and what about it_next()?



*/

#include <stddef.h>
#include <stdint.h>

typedef struct XListBucket
{
    struct XListBucket *next;
    struct XListBucket *prev;
    
    XARR_T *elms;
    size_t cap;
    size_t len;
} XListBucket;

typedef struct XList
{
    XListBucket **not_full_buckets;
    size_t nfb_size;
    size_t nfb_cap;
    
    XListBucket *buckets;
} XList;

typedef struct XListIterator
{
    XARR_T *ptr;
    size_t id; // something to do with index+bucket?
    // maybe we can store a second buffer at the bucket?
    // or the one-past-last elm stores some data about owning bucket? (might be the way)
    // issue is, without a ptr to the bucket, we dont know where the end is (maybe sentinel value?)
    
    // new idea:
    void *allocation; //key1
    size_t idx_within_allocation; //key2
    // these two fields will be used as key into some map
    
    // or maybe use sentinel value to mark end of array (-1 for ints for example)
    // the address of the sentinel value will be used as a key in some map to get the owning bucket. (THIS MIGHT BE THE WAY!)
    
    // new idea: the main list structure holds array of all buckets, just check if ptr >= begin and < end of which bucket
} XListIterator;











