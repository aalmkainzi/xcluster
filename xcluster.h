/*
XCLUSTER_NAME stands for "Exploding Array",
when deleting, it splits the the hole into 2 arrays,
in order to maintain pointer/iterator stability of other elements.

when inserting, it looks into stack of non_full_buckets, if empty, alloc new bucket.

when deleting, split into 2 arrays, left one is full from the start, right one will keep the old cap + left_arr_size

but how will iterators be stable? and what about it_next()?

*/

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#if !defined(XCLUSTER_T) || !defined(XCLUSTER_NAME) || !defined(XCLUSTER_MAKE_SENTINEL) || !defined(XCLUSTER_IS_SENTINEL) || !defined(XCLUSTER_SENTINEL_GET_PTR) || !defined(XCLUSTER_SENTINEL_SET_PTR)
    #error "Must define XCLUSTER_T, XCLUSTER_NAME, XCLUSTER_MAKE_SENTINEL, XCLUSTER_IS_SENTINEL, XCLUSTER_SENTINEL_GET_PTR, and XCLUSTER_SENTINEL_SET_PTR"
#endif

#define XCLUSTER_CAT_(a, b) a##b
#define XCLUSTER_CAT(a, b)  XCLUSTER_CAT_(a,b)

#define xcluster_bucket_t   XCLUSTER_CAT(XCLUSTER_NAME, _bucket_t)

#define xcluster_init       XCLUSTER_CAT(XCLUSTER_NAME, _init)
#define xcluster_put_ptr    XCLUSTER_CAT(XCLUSTER_NAME, _put_ptr)
#define xcluster_put        XCLUSTER_CAT(XCLUSTER_NAME, _put)
#define xcluster_del        XCLUSTER_CAT(XCLUSTER_NAME, _del)
#define xcluster_deinit     XCLUSTER_CAT(XCLUSTER_NAME, _deinit)
#define xcluster_begin      XCLUSTER_CAT(XCLUSTER_NAME, _begin)
#define xcluster_end        XCLUSTER_CAT(XCLUSTER_NAME, _end)
#define xcluster_next       XCLUSTER_CAT(XCLUSTER_NAME, _next)

typedef struct XCLUSTER_NAME
{
    struct
    {
        struct xcluster_bucket_t **array;
        size_t count;
        size_t cap;
    } not_full_buckets;
    
    struct
    {
        struct xcluster_bucket_t **array;
        size_t cap;
        size_t count;
    } buckets_with_prev_bridges; // only store prevs, that way we don't get the same hole repeated as a next bridge and a prev bridge
    
    struct
    {
        void **array;
        size_t count;
        size_t cap;
    } allocations;
    
    struct xcluster_bucket_t *tail;
    struct xcluster_bucket_t *head;
    struct xcluster_bucket_t *end_sentinel;
    size_t bucket_count;
    
    size_t prev_cap;
    size_t count;
} XCLUSTER_NAME;

void xcluster_init(XCLUSTER_NAME *cls);
XCLUSTER_T *xcluster_put_ptr(XCLUSTER_NAME *cls, const XCLUSTER_T *new_elm);
XCLUSTER_T *xcluster_put(XCLUSTER_NAME *cls, XCLUSTER_T elm);
XCLUSTER_T *xcluster_del(XCLUSTER_NAME *cls, XCLUSTER_T *elm);
void xcluster_deinit(XCLUSTER_NAME *cls);
XCLUSTER_T *xcluster_begin(XCLUSTER_NAME *cls);
XCLUSTER_T *xcluster_end(XCLUSTER_NAME *cls);
XCLUSTER_T *xcluster_next(XCLUSTER_T *it);

#ifdef XCLUSTER_IMPL

typedef struct xcluster_bucket_t
{
    struct xcluster_bucket_t *bridge_prev;
    struct xcluster_bucket_t *bridge_next;
    
    struct xcluster_bucket_t *next;
    struct xcluster_bucket_t *prev;
    
    size_t not_full_index;
    
    XCLUSTER_T *elms;
    size_t count;
    size_t cap;
} xcluster_bucket_t;

// TODO const iterator. it will be invalidated by inserts/deletes
// but is faster to iterate, since we wont check for sentinel every iteration,
// instead we know each node's count

// TODO maybe some kind of SOA thing
// where each node stores array for EACH type
// actually, the user type doesn't *need* a pointer field
// if the pointer field macros are not defined,
// we can just store a secondary buffer for pointers only
// it wont slow down iteration speed
// because we'll only use that buffer when we reach a node's end

#ifdef XCLUSTER_DEBUG
    #define xcluster_validate XCLUSTER_CAT(XCLUSTER_NAME, _validate)
    bool xcluster_validate(XCLUSTER_NAME *cls);
#endif

#define xcluster_erase_not_full_bucket XCLUSTER_CAT(XCLUSTER_NAME, _erase_not_full_bucket)
#define xcluster_push_not_full_bucket  XCLUSTER_CAT(XCLUSTER_NAME, _push_not_full_bucket)
#define xcluster_assign_sentinel       XCLUSTER_CAT(XCLUSTER_NAME, _assign_sentinel)
#define xcluster_unlink_bucket         XCLUSTER_CAT(XCLUSTER_NAME, _unlink_bucket)
#define xcluster_steal_node_links      XCLUSTER_CAT(XCLUSTER_NAME, _steal_node_links)
#define xcluster_alloc_node            XCLUSTER_CAT(XCLUSTER_NAME, _alloc_node)

#if defined(__cplusplus) && defined(_MSC_VER)
    #define XCLUSTER_TYPEOF decltype
#else
    #define XCLUSTER_TYPEOF __typeof__
#endif

#define XCLUSTER_MAYBE_GROW(s, ...)                                     \
do                                                                      \
{                                                                       \
    const size_t _n = (__VA_ARGS__ +0) ? (__VA_ARGS__ +0) : 1;          \
    if(((s).count + _n - 1) >= (s).cap)                                 \
    {                                                                   \
        (s).cap = ((s).cap + (_n - 1)) * 2;                             \
        (s).array = (XCLUSTER_TYPEOF((s).array))                        \
                    realloc((s).array, (s).cap * sizeof(*((s).array))); \
    }                                                                   \
} while(0)

#define XCLUSTER_POP(s) \
((s).array[--(s).count])

#define XCLUSTER_PUSH(s, x) \
((s).array[(s).count++] = (x))

void xcluster_assign_sentinel(XCLUSTER_T *elm, xcluster_bucket_t *bucket)
{
    XCLUSTER_MAKE_SENTINEL((elm));
    XCLUSTER_SENTINEL_SET_PTR((elm), (bucket));
}

void xcluster_init(XCLUSTER_NAME *cls)
{
    memset(cls, 0, sizeof(XCLUSTER_NAME));
    
    
    cls->end_sentinel = (xcluster_bucket_t*) malloc(sizeof(xcluster_bucket_t));
    memset(cls->end_sentinel, 0, sizeof(xcluster_bucket_t));
    
    cls->head = cls->end_sentinel;
    cls->tail = cls->end_sentinel;
    
    cls->not_full_buckets.cap = 16;
    cls->not_full_buckets.array = (xcluster_bucket_t**) malloc(sizeof(xcluster_bucket_t*) * cls->not_full_buckets.cap);
    
    cls->allocations.cap = 16;
    cls->allocations.array = (void**) malloc(sizeof(void*) * cls->allocations.cap);
    
    cls->buckets_with_prev_bridges.cap = 16;
    cls->buckets_with_prev_bridges.array = (xcluster_bucket_t**) malloc(sizeof(xcluster_bucket_t*) * cls->buckets_with_prev_bridges.cap);
    
    cls->prev_cap = 2048;
}

void xcluster_erase_not_full_bucket(XCLUSTER_NAME *cls, xcluster_bucket_t *b)
{
    assert(b->not_full_index != (size_t)-1);
    
    size_t index = b->not_full_index;
    size_t last = cls->not_full_buckets.count - 1;
    
    cls->not_full_buckets.array[index] = cls->not_full_buckets.array[last];
    cls->not_full_buckets.array[index]->not_full_index = index;
    cls->not_full_buckets.count -= 1;
    b->not_full_index = (size_t)-1;
}

void xcluster_push_not_full_bucket(XCLUSTER_NAME *cls, xcluster_bucket_t *b)
{
    assert(b->not_full_index == (size_t)-1);
    
    XCLUSTER_MAYBE_GROW(cls->not_full_buckets);
    XCLUSTER_PUSH(cls->not_full_buckets, b);
    
    b->not_full_index = cls->not_full_buckets.count - 1;
}

void xcluster_unlink_bucket(XCLUSTER_NAME *cls, xcluster_bucket_t *b)
{
    if(b == cls->head)
    {
        cls->head = cls->head->next;
        cls->head->prev = NULL;
        if(b == cls->tail)
        {
            cls->tail = cls->end_sentinel;
        }
    }
    else if(b == cls->tail)
    {
        cls->tail = cls->tail->prev;
        cls->tail->next = cls->end_sentinel;
        cls->end_sentinel->prev = cls->tail;
    }
    else
    {
        b->prev->next = b->next;
        b->next->prev = b->prev;
    }
    
    b->next = b->prev = NULL;
}

void xcluster_steal_node_links(XCLUSTER_NAME *cls, xcluster_bucket_t *new_node, xcluster_bucket_t *old_node)
{
    new_node->next = old_node->next;
    new_node->prev = old_node->prev;
    
    new_node->next->prev = new_node;
    
    if(new_node->prev != NULL)
    {
        new_node->prev->next = new_node;
    }
    
    if(old_node == cls->head)
    {
        cls->head = new_node;
    }
    if(old_node == cls->tail)
    {
        cls->tail = new_node;
    }
    
    old_node->next = old_node->prev = NULL;
}

void xcluster_link_node(XCLUSTER_NAME *cls, xcluster_bucket_t *bucket)
{
    if(cls->tail == cls->end_sentinel) // empty
    {
        cls->head = cls->tail = bucket;
        bucket->next = cls->end_sentinel;
        cls->end_sentinel->prev = bucket;
    }
    else
    {
        cls->tail->next = bucket;
        bucket->prev = cls->tail;
        cls->tail = bucket;
        bucket->next = cls->end_sentinel;
        cls->end_sentinel->prev = bucket;
    }
}

XCLUSTER_T *xcluster_put_ptr(XCLUSTER_NAME *cls, const XCLUSTER_T *new_elm)
{
    if(cls->not_full_buckets.count != 0)
    {
        cls->count++;
        
        xcluster_bucket_t *bucket = cls->not_full_buckets.array[cls->not_full_buckets.count - 1];
        
        if(bucket->count == 0)
        {
            assert(bucket->next == NULL);
            assert(bucket->prev == NULL);
            xcluster_link_node(cls, bucket);
        }
        
        // assert(bucket->next != NULL);
        // assert(bucket == cls->head || bucket->prev != NULL);
        
        XCLUSTER_T *ret = &bucket->elms[bucket->count];
        bucket->count++;
        
        xcluster_assign_sentinel(&bucket->elms[bucket->count], bucket);
        
        if(bucket->count == bucket->cap)
        {
            bucket->not_full_index = (size_t)-1;
            cls->not_full_buckets.count -= 1;
            // TODO what we can do here is check if bucket has bridge_next,
            // and merge with it if so
        }
        
        memcpy(ret, new_elm, sizeof(XCLUSTER_T));
#ifdef XCLUSTER_DEBUG
        xcluster_validate(cls);
#endif
        return ret;
    }
    if(cls->buckets_with_prev_bridges.count != 0)
    {
        cls->count++;
        
        xcluster_bucket_t *bucket = XCLUSTER_POP(cls->buckets_with_prev_bridges);
        
        xcluster_bucket_t *prev = bucket->bridge_prev;
        
        // they may be empty and unlinked, or full
        assert(prev != NULL);
        assert(prev->count == prev->cap || prev->count == 0);
        assert(bucket->count == bucket->cap || bucket->count == 0);
        assert((prev->elms + prev->cap + 1) == bucket->elms);
        
        size_t prevoldcap = prev->cap;
        size_t prevoldcount = prev->count;
        
        prev->cap = prev->cap + bucket->cap + 1;
        XCLUSTER_T *ret = &prev->elms[prev->count];
        
        prev->count += 1 + bucket->count;
        
        XCLUSTER_SENTINEL_SET_PTR((&prev->elms[prev->count]), (prev));
        
        prev->bridge_next = bucket->bridge_next;
        if(prev->bridge_next != NULL)
        {
            prev->bridge_next->bridge_prev = prev;
        }
        
        if(prev->next == NULL)
        {
            if(bucket->next != NULL) // reuse bucket's links
            {
                xcluster_steal_node_links(cls, prev, bucket);
            }
            else
            {
                xcluster_link_node(cls, prev);
            }
        }
        else if(bucket->next != NULL)
        {
            xcluster_unlink_bucket(cls, bucket);
        }
        
        if(prev->count < prev->cap)
        {
            xcluster_push_not_full_bucket(cls, prev);
        }
        
        // I understand the issue.
        // the prev can be a node that's not full
        // the solution is, put it as a not_full_bucket, even if unlinked
        memcpy(ret, new_elm, sizeof(XCLUSTER_T));
#ifdef XCLUSTER_DEBUG
        xcluster_validate(cls);
#endif
        return ret;
    }
    
    XCLUSTER_MAYBE_GROW(cls->allocations, 2);
    
    xcluster_bucket_t *new_bucket = (xcluster_bucket_t*) malloc(sizeof(xcluster_bucket_t));
    XCLUSTER_PUSH(cls->allocations, new_bucket);
    
    memset(new_bucket, 0, sizeof(xcluster_bucket_t));
    new_bucket->not_full_index = (size_t)-1;
    new_bucket->cap = cls->prev_cap * 2;
    cls->prev_cap *= 2;
    
    new_bucket->elms = (XCLUSTER_T*) malloc(sizeof(XCLUSTER_T) * (new_bucket->cap + 1));
    XCLUSTER_PUSH(cls->allocations, new_bucket->elms);
    
    XCLUSTER_T *ret = &new_bucket->elms[0];
    
    xcluster_assign_sentinel(&new_bucket->elms[1], new_bucket);
    
    new_bucket->count = 1;
    
    if(cls->count == 0)
    {
        cls->head = new_bucket;
    }
    else
    {
        cls->tail->next = new_bucket;
        new_bucket->prev = cls->tail;
    }
    cls->tail = new_bucket;
    cls->end_sentinel->prev = new_bucket;
    new_bucket->next = cls->end_sentinel;
    
    xcluster_push_not_full_bucket(cls, new_bucket);
    cls->count += 1;
    
    memcpy(ret, new_elm, sizeof(XCLUSTER_T));
#ifdef XCLUSTER_DEBUG
    xcluster_validate(cls);
#endif
    return ret;
}

XCLUSTER_T *xcluster_put(XCLUSTER_NAME *cls, XCLUSTER_T elm)
{
    XCLUSTER_T *ptr = xcluster_put_ptr(cls, &elm);
    *ptr = elm;
    return ptr;
}

xcluster_bucket_t *xcluster_alloc_node(XCLUSTER_NAME *cls)
{
    xcluster_bucket_t *new_bucket = (xcluster_bucket_t*) malloc(sizeof(xcluster_bucket_t));
    memset(new_bucket, 0, sizeof(xcluster_bucket_t));
    new_bucket->not_full_index = (size_t)-1;
    
    XCLUSTER_MAYBE_GROW(cls->allocations);
    XCLUSTER_PUSH(cls->allocations, new_bucket);
    
    return new_bucket;
}

size_t xcluster_bridges_with_prev_index(XCLUSTER_NAME *cls, xcluster_bucket_t *bucket)
{
    for(size_t i = 0 ; i < cls->buckets_with_prev_bridges.count ; i++)
    {
        if(cls->buckets_with_prev_bridges.array[i] == bucket)
        {
            return i;
        }
    }
    return (size_t)-1;
}

void xcluster_steal_node_not_full_index(XCLUSTER_NAME *cls, xcluster_bucket_t *new_node, xcluster_bucket_t *old_node)
{
    assert(old_node->not_full_index != (size_t)-1);
    
    new_node->not_full_index = old_node->not_full_index;
    cls->not_full_buckets.array[new_node->not_full_index] = new_node;
}

XCLUSTER_T *xcluster_del(XCLUSTER_NAME *cls, XCLUSTER_T *elm)
{
    cls->count -= 1;
    
    XCLUSTER_T *end = elm;
    while( !XCLUSTER_IS_SENTINEL(end) )
    {
        end += 1;
    }
    
    xcluster_bucket_t *bp = (xcluster_bucket_t*) XCLUSTER_SENTINEL_GET_PTR(end);
    xcluster_bucket_t *bp_next_node = bp->next;
    xcluster_bucket_t *bp_prev_node = bp->prev;
    
    size_t deleted_index = elm - bp->elms;
    
    // TODO handle cases of not_full_buckets
    // if a node is no longer linked, it should be (size_t)-1 (NO)
    
    if(deleted_index == 0)
    {
        // current bucket has 0 elms, unlink it from the chain, but keep its as a bridge_prev for the new node
        // BUT. if it has a prev_bridge, merge with it to reduce number of nodes
        
        xcluster_bucket_t *new_bucket = xcluster_alloc_node(cls);
        
        new_bucket->elms = bp->elms + 1;
        new_bucket->count = bp->count - 1;
        new_bucket->cap = bp->cap - 1;
        
        bp->count = 0;
        bp->cap = 0;
        
        // OK so here's actually what should happen:
        // if bp has a bridge_prev, merge with it: if bp->not_full_index!=-1 then reuse its slot, if it is -1 then push the bridge_prev
        // when should new_bucket steal bp's not_full_index? when it merges with bucket_prev and bucket_prev has a not_full_index
        // otherwise new_bucket should be pushed and get its own index
        
        if(bp->bridge_prev != NULL)
        {
            xcluster_bucket_t *bp_bridge_prev = bp->bridge_prev;
            assert((bp_bridge_prev->elms + bp_bridge_prev->cap + 1) == bp->elms);
            
            bp_bridge_prev->cap = bp_bridge_prev->cap + 1;
            
            if(bp_bridge_prev->not_full_index == (size_t)-1)
            {
                if(bp->not_full_index != (size_t)-1)
                {
                    xcluster_steal_node_not_full_index(cls, bp_bridge_prev, bp);
                    bp->not_full_index = (size_t)-1;
                }
                else
                {
                    xcluster_push_not_full_bucket(cls, bp_bridge_prev);
                }
            }
            
            if(bp_bridge_prev->count != 0)
            {
                assert(bp->next != NULL);
                xcluster_steal_node_links(cls, bp_bridge_prev, bp);
            }
            
            bp_bridge_prev->bridge_next = new_bucket;
            new_bucket->bridge_prev = bp_bridge_prev;
            
            size_t index = xcluster_bridges_with_prev_index(cls, bp); // TODO maybe nodes should store with_prev_index
            assert(index != (size_t)-1);
            cls->buckets_with_prev_bridges.array[index] = new_bucket;
            
            assert(bp_bridge_prev->bridge_next == new_bucket);
            assert(new_bucket->bridge_prev == bp_bridge_prev);
            assert((bp_bridge_prev->elms + bp_bridge_prev->cap + 1) == new_bucket->elms);
            assert(new_bucket->bridge_prev != new_bucket);
            
            new_bucket->bridge_next = bp->bridge_next;
            if(new_bucket->bridge_next != NULL)
            {
                new_bucket->bridge_next->bridge_prev = new_bucket;
            }
            
            // TODO push bp to some kind of bucket_reserve here
        }
        else
        {
            new_bucket->bridge_prev = bp;
            assert(new_bucket->bridge_prev != new_bucket);
            
            XCLUSTER_MAYBE_GROW(cls->buckets_with_prev_bridges);
            XCLUSTER_PUSH(cls->buckets_with_prev_bridges, new_bucket);
            
            assert((bp->elms + bp->cap + 1) == new_bucket->elms);
            
            new_bucket->bridge_next = bp->bridge_next;
            if(new_bucket->bridge_next != NULL)
            {
                new_bucket->bridge_next->bridge_prev = new_bucket;
            }
            bp->bridge_next = new_bucket;
        }
        
        if(new_bucket->cap > new_bucket->count)
        {
            if(bp->not_full_index == (size_t)-1)
            {
                new_bucket->not_full_index = bp->not_full_index;
                cls->not_full_buckets.array[new_bucket->not_full_index] = new_bucket;
                bp->not_full_index = (size_t)-1;
            }
            else
            {
                xcluster_push_not_full_bucket(cls, new_bucket);
            }
            
            xcluster_assign_sentinel(elm, bp);
        }
        
        xcluster_assign_sentinel(&new_bucket->elms[new_bucket->count], new_bucket);
        
        XCLUSTER_T *ret = NULL;
        if(new_bucket->count != 0)
        {
            if(bp->next != NULL)
            {
                xcluster_steal_node_links(cls, new_bucket, bp);
            }
            else
            {
                xcluster_link_node(cls, new_bucket);
            }
            
            ret = new_bucket->elms;
        }
        else
        {
            ret = bp_next_node->elms;
            xcluster_unlink_bucket(cls, bp);
        }
        
        assert(
            XCLUSTER_SENTINEL_GET_PTR((&new_bucket->elms[new_bucket->count])) == bp
        );
        
        assert(new_bucket->bridge_prev != new_bucket);
#ifdef XCLUSTER_DEBUG
        xcluster_validate(cls); // error found in this branch
#endif
        return ret;
    }
    else if(deleted_index == bp->count - 1)
    {
        bp->count -= 1;
        xcluster_assign_sentinel(&bp->elms[bp->count], bp);
        
        if(bp->not_full_index == (size_t)-1)
        {
            xcluster_push_not_full_bucket(cls, bp);
        }
        
        return bp->next->elms;
    }
    else
    {
        xcluster_bucket_t *new_bucket = xcluster_alloc_node(cls);
        
        // let's say old_cap is 8, deleted index is 2
        // that means new_bucket will be starting at 3 through 8, so 0->5 so cap=5
        // let's say old_count is 6
        // new count is 2
        // new cap is 2
        // new_bucket count is 3
        new_bucket->count = bp->count - deleted_index - 1;
        new_bucket->cap   = bp->cap   - deleted_index - 1;
        
        bp->count = deleted_index;
        bp->cap = bp->count;
        
        new_bucket->next = bp->next;
        new_bucket->prev = bp;
        new_bucket->next->prev = new_bucket;
        new_bucket->prev->next = new_bucket;
        
        if(bp == cls->tail)
        {
            cls->tail = new_bucket;
        }
        
        new_bucket->bridge_next = bp->bridge_next;
        bp->bridge_next = new_bucket;
        new_bucket->bridge_prev = bp;
        
        if(new_bucket->bridge_next != NULL)
        {
            new_bucket->bridge_next->bridge_prev = new_bucket;
        }
        
        new_bucket->elms = bp->elms + bp->count + 1;
        
        assert((bp->elms + bp->cap + 1) == new_bucket->elms);
        
        // TODO this piece of code is repeated 2 times so far, refactor
        // lots of common code between these 3 branches
        if(bp->not_full_index != (size_t)-1)
        {
            new_bucket->not_full_index = bp->not_full_index;
            cls->not_full_buckets.array[new_bucket->not_full_index] = new_bucket;
            bp->not_full_index = (size_t)-1;
        }
        
        XCLUSTER_MAYBE_GROW(cls->buckets_with_prev_bridges);
        XCLUSTER_PUSH(cls->buckets_with_prev_bridges, new_bucket);
        
        xcluster_assign_sentinel(elm, bp);
        xcluster_assign_sentinel(&new_bucket->elms[new_bucket->count], new_bucket);
        
#ifdef XCLUSTER_DEBUG
        xcluster_validate(cls);
#endif
        return new_bucket->elms;
    }
}

void xcluster_deinit(XCLUSTER_NAME *cls)
{
    for(size_t i = 0 ; i < cls->allocations.count ; i++)
    {
        free(cls->allocations.array[i]);
    }
    free(cls->allocations.array);
    free(cls->end_sentinel);
    free(cls->not_full_buckets.array);
    free(cls->buckets_with_prev_bridges.array);
}

XCLUSTER_T *xcluster_begin(XCLUSTER_NAME *cls)
{
    return (XCLUSTER_T*){cls->head->elms};
}

XCLUSTER_T *xcluster_end(XCLUSTER_NAME *cls)
{
    return (XCLUSTER_T*){cls->end_sentinel->elms};
}

XCLUSTER_T *xcluster_next(XCLUSTER_T *it)
{
    it += 1;
    if(XCLUSTER_IS_SENTINEL(it))
    {
        xcluster_bucket_t *bucket = (xcluster_bucket_t*) XCLUSTER_SENTINEL_GET_PTR(it);
        return bucket->next->elms;
    }
    return it;
}

bool xcluster_validate(XCLUSTER_NAME *cls)
{
    xcluster_bucket_t *b = cls->head;
    xcluster_bucket_t *prev = NULL;
    size_t accum = 0;
    while(b != cls->end_sentinel)
    {
        accum += b->count;
        
        for(size_t i = 0 ; i < b->count ; i++)
        {
            assert(!XCLUSTER_IS_SENTINEL((&b->elms[i])));
        }
        assert(XCLUSTER_IS_SENTINEL((&b->elms[b->count])));
        xcluster_bucket_t *bb = (xcluster_bucket_t*) XCLUSTER_SENTINEL_GET_PTR((&b->elms[b->count]));
        assert(bb == b);
        
        assert(b->prev == prev);
        
        prev = b;
        b = b->next;
        
        assert(b != NULL);
    }
    
    for(size_t i = 0 ; i < cls->not_full_buckets.count ; i++)
    {
        xcluster_bucket_t *b = cls->not_full_buckets.array[i];
        assert(b->cap > b->count);
        assert(b->count > 0);
        assert(b->not_full_index == i);
        assert(b->next != NULL);
        assert(b == cls->head || b->prev != NULL);
    }
    
    for(size_t i = 0 ; i < cls->buckets_with_prev_bridges.count ; i++)
    {
        xcluster_bucket_t *bucket = cls->buckets_with_prev_bridges.array[i];
        xcluster_bucket_t *prev = bucket->bridge_prev;
        assert(prev != NULL);
        assert((prev->elms + prev->cap + 1) == bucket->elms);
        assert(prev->bridge_next == bucket);
        assert(XCLUSTER_SENTINEL_GET_PTR(&prev->elms[prev->count]) == prev);
    }
    
    assert(accum == cls->count);
    return true;
}

#endif

#undef XCLUSTER_CAT_
#undef XCLUSTER_CAT

#undef XCLUSTER_TYPEOF

#undef xcluster_bucket_t

#undef xcluster_init
#undef xcluster_put_ptr
#undef xcluster_put
#undef xcluster_del
#undef xcluster_deinit
#undef xcluster_begin
#undef xcluster_end
#undef xcluster_next

#undef xcluster_erase_not_full_bucket
#undef xcluster_push_not_full_bucket
#undef xcluster_assign_sentinel
#undef xcluster_unlink_bucket
#undef xcluster_steal_node_links
#undef xcluster_alloc_node
#undef xcluster_validate

#undef XCLUSTER_PUSH
#undef XCLUSTER_POP
#undef XCLUSTER_MAYBE_GROW
