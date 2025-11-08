// xcluster_tests.cpp
// xcluster.h tests (duplicates-enabled) — uses container.count for size checks

#include <bits/stdc++.h>
#include <chrono>
#include <random>
#include <iostream>
#include <unordered_map>
#include <cassert>

// ---------------------
// Configure xcluster for S
// ---------------------
struct S {
    int k;     // key; k == -1 is sentinel
    void *p;   // pointer field usable by sentinel
};

#define XCLUSTER_T S
#define XCLUSTER_NAME ss
#define XCLUSTER_MAKE_SENTINEL(a) ((a)->k = -1)
#define XCLUSTER_IS_SENTINEL(a) ((a)->k == -1)
#define XCLUSTER_SENTINEL_SET_PTR(a,pp) ((a)->p = (void*)(pp))
#define XCLUSTER_SENTINEL_GET_PTR(a) ((a)->p)
#define XCLUSTER_IMPL
#define XCLUSTER_DEBUG
#include "../xcluster.h" // ensure xcluster.h is on the include path

// ---------------------
// helpers
// ---------------------
static void die(const char *msg) {
    std::cerr << "FAILED: " << msg << std::endl;
    std::exit(1);
}
static void expect(bool cond, const char *msg) { if (!cond) die(msg); }

// return pointer to first element with key, or nullptr
static S *find_in_xcluster(ss *c, int key) {
    for (S *it = ss_begin(c); it != ss_end(c); it = ss_next(it)) {
        if (it->k == key) return it;
    }
    return nullptr;
}

// count occurrences of each key in the container by iterating
static std::unordered_map<int,int> counts_in_xcluster(ss *c) {
    std::unordered_map<int,int> m;
    for (S *it = ss_begin(c); it != ss_end(c); it = ss_next(it)) {
        m[it->k] += 1;
    }
    return m;
}

// return pointer to element at zero-based index idx (iteration order)
static S *nth_element(ss *c, size_t idx) {
    size_t i = 0;
    for (S *it = ss_begin(c); it != ss_end(c); it = ss_next(it)) {
        if (i == idx) return it;
        ++i;
    }
    return nullptr;
}

// ---------------------
// Tests
// ---------------------
static void test_empty_begin_end() {
    ss c; ss_init(&c);
    expect(ss_begin(&c) == ss_end(&c), "empty container: begin != end");
    expect((size_t)c.count == 0, "empty container: count != 0");
    ss_deinit(&c);
}

static void test_basic_insert_and_iterate() {
    ss c; ss_init(&c);
    const int N = 500;
    for (int i = 0; i < N; ++i) {
        S v; v.k = i + 1; v.p = nullptr;
        S *r = ss_put(&c, v);
        expect(r != nullptr, "ss_put returned null in basic test");
    }
    // use container.count to verify total element count
    expect((size_t)c.count == (size_t)N, "count mismatch after unique inserts");
    
    auto counts = counts_in_xcluster(&c);
    expect((int)counts.size() == N, "unexpected distinct-key count after unique inserts");
    for (int i = 1; i <= N; ++i) expect(counts[i] == 1, "missing or duplicated key in basic insert test");
    ss_deinit(&c);
}

static void test_put_ptr_and_returned_ptr_identity() {
    ss c; ss_init(&c);
    S tmp; tmp.k = 12345; tmp.p = (void*)0xdeadbeef;
    S *r = ss_put_ptr(&c, &tmp);
    expect(r != nullptr, "ss_put_ptr returned null");
    expect(r->k == 12345, "ss_put_ptr wrong key");
    r->p = (void*)0xfeedface;
    S *found = find_in_xcluster(&c, 12345);
    expect(found == r, "pointer identity mismatch after put_ptr");
    
    S v2; v2.k = 54321; v2.p = nullptr;
    S *r2 = ss_put(&c, v2);
    expect(r2 != nullptr, "ss_put returned null for v2");
    S *f2 = find_in_xcluster(&c, 54321);
    expect(f2 == r2, "ss_put pointer mismatch");
    
    // count should be 2
    expect((size_t)c.count == 2, "count mismatch after two inserts");
    
    ss_deinit(&c);
}

static void test_deletion_and_duplicates() {
    ss c; ss_init(&c);
    // insert duplicates: keys 1..100 each twice
    for (int i = 1; i <= 100; ++i) {
        ss_put(&c, S{ .k = i, .p = nullptr });
        ss_put(&c, S{ .k = i, .p = nullptr });
        ss_validate(&c);
    }
    expect((size_t)c.count == 200, "count mismatch after inserting duplicates");
    
    auto counts = counts_in_xcluster(&c);
    for (int i = 1; i <= 100; ++i) expect(counts[i] == 2, "expected two duplicates after insert");
    
    // delete one instance of each key 1..100
    for (int i = 1; i <= 100; ++i) {
        // std::cout << " deled " << i << std::endl;
        S *it = find_in_xcluster(&c, i);
        expect(it != nullptr, "element to delete not found");
        ss_del(&c, it);
        ss_validate(&c);
    }
    expect((size_t)c.count == 100, "count mismatch after deleting one of each duplicate");
    
    auto counts2 = counts_in_xcluster(&c);
    for (int i = 1; i <= 100; ++i) {
        expect(counts2[i] == 1, "expected one instance remaining after deletion of one duplicate");
    }
    
    // delete remaining instances
    for (int i = 1; i <= 100; ++i) {
        S *it = find_in_xcluster(&c, i);
        expect(it != nullptr, "second instance to delete not found");
        ss_del(&c, it);
        ss_validate(&c);
    }
    expect((size_t)c.count == 0, "count mismatch after deleting all duplicates");
    auto counts3 = counts_in_xcluster(&c);
    expect(counts3.empty(), "expected no instances after deleting both duplicates");
    
    ss_deinit(&c);
}

static void test_pointer_stability_under_inserts() {
    ss c; ss_init(&c);
    const int base = 10000;
    const int keepCount = 1000;
    std::vector<S*> pointers; pointers.reserve(keepCount);
    for (int i = 0; i < keepCount; ++i) {
        int key = base + i;
        S *p = ss_put(&c, S{ .k = key, .p = nullptr });
        expect(p != nullptr, "put failed in pointer stability test");
        pointers.push_back(p);
    }
    // many extra inserts (unique new keys)
    const int additional = 10000;
    for (int i = 0; i < additional; ++i) {
        int key = base + keepCount + i + 1;
        ss_put(&c, S{ .k = key, .p = nullptr });
    }
    // count should be keepCount + additional
    expect((size_t)c.count == (size_t)(keepCount + additional), "count mismatch after many inserts");
    
    // verify pointers unchanged
    for (int i = 0; i < keepCount; ++i) {
        S *p = pointers[i];
        expect(p != nullptr, "saved pointer null");
        expect(p->k == base + i, "saved pointer's key changed");
        S *found = find_in_xcluster(&c, base + i);
        expect(found != nullptr, "didn't find element after many inserts");
        expect(found == p, "pointer stability violated");
    }
    ss_deinit(&c);
}

static void test_iteration_matches_put_pointers() {
    ss c; ss_init(&c);
    
    const int N = 2000;                       // number of inserts (creates duplicates of keys)
    std::vector<S*> inserted; inserted.reserve(N);
    
    // insert many elements (keys purposely duplicate to ensure duplicates supported)
    for (int i = 0; i < N; ++i) {
        S v; v.k = i % 50; v.p = nullptr;
        S *p = ss_put(&c, v);
        expect(p != nullptr, "ss_put returned null in iteration-match test");
        inserted.push_back(p);
    }
    
    // fast sanity: container.count must match number of inserts
    expect((size_t)c.count == inserted.size(), "count mismatch after inserts in iteration-match test");
    
    // build expected multiset of pointers (handles potential duplicate pointers if library ever returns same address multiple times)
    std::unordered_map<S*, int> expected;
    for (S *p : inserted) expected[p] += 1;
    
    // iterate and verify every pointer returned by ss_put is visited exactly the number of times it was returned
    size_t iter_count = 0;
    for (S *it = ss_begin(&c); it != ss_end(&c); it = ss_next(it)) {
        ++iter_count;
        auto fit = expected.find(it);
        if (fit == expected.end()) {
            std::cerr << "Iterator yielded pointer not returned by ss_put: " << (void*)it
            << " (k=" << it->k << ")\n";
            die("iterator yielded unknown pointer");
        }
        // consume one occurrence
        if (--fit->second == 0) expected.erase(fit);
    }
    
    expect(iter_count == inserted.size(), "iteration visited a different number of elements than were inserted");
    expect(expected.empty(), "some pointers returned by ss_put were not visited during iteration");
    
    ss_deinit(&c);
}


static void test_randomized_stress_allowing_duplicates_and_using_count() {
    ss c; ss_init(&c);
    
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<int> keyDist(1, 2000000);
    std::bernoulli_distribution opDist(0.55); // favor inserts slightly
    
    std::unordered_map<int,int> model; // key -> count
    size_t model_total = 0;
    
    const int OPS = 50000;
    for (int op = 0; op < OPS; ++op) {
        bool doInsert = opDist(rng);
        if (doInsert) {
            int k = keyDist(rng);
            ss_put(&c, S{ .k = k, .p = nullptr });
            model[k] += 1;
            model_total += 1;
        } else {
            if (model_total > 0) {
                // pick a random instance index from 0..model_total-1 and iterate to it
                // std::cout << "op=" << op << std::endl;
                size_t r = (size_t)(rng() % model_total);
                S *elem = nth_element(&c, r);
                if (!elem) die("nth_element returned null during delete step");
                int key = elem->k;
                ss_del(&c, elem);
                auto it = model.find(key);
                if (it == model.end()) die("model missing key that container had");
                it->second -= 1;
                model_total -= 1;
                if (it->second <= 0) model.erase(it);
            }
        }
        
        // occasional full validation using container.count for total size
        if ((op & 1023) == 0) {
            // check total count quickly from container.count
            expect((size_t)c.count == model_total, "count mismatch (fast) in randomized test");
            
            // also verify per-key counts by iterating
            auto counts = counts_in_xcluster(&c);
            if (counts.size() != model.size()) {
                std::cerr << "Mismatch distinct keys: container=" << counts.size() << " model=" << model.size() << " at op " << op << std::endl;
                die("distinct key count mismatch in randomized test");
            }
            for (auto &kv : model) {
                int k = kv.first;
                int expected = kv.second;
                auto it = counts.find(k);
                if (it == counts.end() || it->second != expected) {
                    std::cerr << "Key " << k << " mismatch at op " << op << " container_count=" << (it==counts.end()?0:it->second) << " expected=" << expected << std::endl;
                    die("per-key count mismatch in randomized test");
                }
            }
        }
    }
    
    // final validation
    expect((size_t)c.count == model_total, "final count mismatch after randomized stress");
    auto final_counts = counts_in_xcluster(&c);
    expect(final_counts.size() == model.size(), "final distinct key count mismatch randomized stress");
    for (auto &kv : model) {
        auto it = final_counts.find(kv.first);
        expect(it != final_counts.end(), "final missing key");
        expect(it->second == kv.second, "final per-key count mismatch");
    }
    
    ss_deinit(&c);
}

static void test_super_stress() {
    ss c; ss_init(&c);
    
    // fixed seed for reproducibility
    std::mt19937_64 rng(123456789ULL);
    std::uniform_int_distribution<int> keyDist(1, 100000); // moderate key space to force duplicates
    // probabilities: insert 60%, delete 35%, put_ptr 5%
    std::discrete_distribution<int> opDist({60,35,5});
    
    std::unordered_map<int,int> model; // key -> count
    size_t model_total = 0;
    
    // Maintain a vector of live element pointers so deletes are O(1) (pick random index, swap-pop).
    // Reserve to avoid frequent reallocations.
    std::vector<S*> live;
    live.reserve(200000);
    
    const size_t OPS = 100000;
    const size_t LIGHT_VALIDATE_EVERY = 10000; // light validation periodically (cheap)
    const size_t SAMPLE_CHECKS = 256; // number of random samples on light validation
    
    for (size_t op = 0; op < OPS; ++op) {
        // std::cout << "op=" << op << std::endl;
        
        int opKind = opDist(rng);
        
        if (opKind == 0) { // insert by value
            int k = keyDist(rng);
            S *ret = ss_put(&c, S{ .k = k, .p = nullptr });
            if (!ret) die("ss_put returned null in super-stress-fast");
            model[k] += 1;
            model_total += 1;
            live.push_back(ret);
        } else if (opKind == 1) { // delete random instance if any
            if (model_total > 0 && !live.empty()) {
                size_t idx = (size_t)(rng() % live.size());
                S *elem = live[idx];
                int k = elem->k;
                // Remove from container
                ss_del(&c, elem);
                // Remove from live vector (swap-pop)
                live[idx] = live.back();
                live.pop_back();
                
                auto it = model.find(k);
                if (it == model.end()) die("model missing key that container had (super-stress-fast)");
                it->second -= 1;
                model_total -= 1;
                if (it->second == 0) model.erase(it);
            }
        } else { // put_ptr (small fraction)
            int k = keyDist(rng);
            S tmp; tmp.k = k; tmp.p = nullptr;
            S *ret = ss_put_ptr(&c, &tmp);
            if (!ret) die("ss_put_ptr returned null in super-stress-fast");
            model[k] += 1;
            model_total += 1;
            live.push_back(ret);
        }
        
        // periodic light validation (cheap): check count and sample a few live pointers
        if ((op + 1) % LIGHT_VALIDATE_EVERY == 0) {
            // fast check using count field
            if ((size_t)c.count != model_total) {
                std::cerr << "SUPER-STRESS-FAST: count mismatch at op " << (op + 1)
                << " container.count=" << c.count << " model_total=" << model_total << "\n";
                die("super-stress-fast: count mismatch (fast)");
            }
            
            // sample some live pointers and verify they exist in the model (cheap verification)
            if (!live.empty()) {
                size_t samples = std::min(SAMPLE_CHECKS, live.size());
                for (size_t s = 0; s < samples; ++s) {
                    size_t idx = (size_t)(rng() % live.size());
                    S *elem = live[idx];
                    // element key must be present in model with positive count
                    auto mit = model.find(elem->k);
                    if (mit == model.end() || mit->second <= 0) {
                        std::cerr << "SUPER-STRESS-FAST: sampled element key missing from model at op " << (op + 1)
                        << " key=" << elem->k << "\n";
                        die("super-stress-fast: sampled element missing from model");
                    }
                }
            }
        }
    }
    
    // final (full) validation — we perform the expensive full iteration only once at the end
    expect((size_t)c.count == model_total, "super-stress-final: count mismatch");
    
    // build full counts by iterating container
    auto final_counts = counts_in_xcluster(&c);
    if (final_counts.size() != model.size()) {
        std::cerr << "SUPER-STRESS-FAST: final distinct-key count mismatch container=" << final_counts.size()
        << " model=" << model.size() << "\n";
        die("super-stress-fast: final distinct-key count mismatch");
    }
    for (auto &kv : model) {
        auto it = final_counts.find(kv.first);
        if (it == final_counts.end() || it->second != kv.second) {
            std::cerr << "SUPER-STRESS-FAST: final per-key mismatch key=" << kv.first
            << " have=" << (it==final_counts.end()?0:it->second)
            << " expected=" << kv.second << "\n";
            die("super-stress-fast: final per-key mismatch");
        }
    }
    
    ss_deinit(&c);
}


int main() {
    std::cout << "xcluster.h tests (duplicates-enabled) starting...\n";
    
    test_empty_begin_end();
    std::cout << " - empty begin/end OK\n";
    
    test_basic_insert_and_iterate();
    std::cout << " - basic insert & iterate OK\n";
    
    test_put_ptr_and_returned_ptr_identity();
    std::cout << " - put_ptr & put identity OK\n";
    
    test_deletion_and_duplicates();
    std::cout << " - deletion with duplicates OK\n";
    
    test_pointer_stability_under_inserts();
    std::cout << " - pointer stability OK\n";
    
    test_iteration_matches_put_pointers();
    std::cout << " - iteration test OK\n";\
    
    test_randomized_stress_allowing_duplicates_and_using_count();
    std::cout << " - randomized stress (duplicates allowed) OK\n";
    
    test_super_stress();
    std::cout << " - super stress OK\n";
    
    std::cout << "ALL TESTS PASSED\n";
    return 0;
}
