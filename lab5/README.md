# Polynomial Multiplication – Sequential and Parallel Implementations

## 1. Overview

This project implements multiplication of two polynomials in four variants:

1. **Naive O(n²) – Sequential**  
2. **Naive O(n²) – Parallel (ForkJoin)**  
3. **Karatsuba – Sequential**  
4. **Karatsuba – Parallel (ForkJoin)**

---

## 2. Algorithms

### 2.1 Naive O(n²) Multiplication (Sequential)

**Idea:** Each term of the first polynomial is multiplied by each term of the second polynomial, and products contributing to the same power are summed.

Given:
- `A(x) = sum( ai x^i )` (i = 0..n-1)  
- `B(x) = sum( bj x^j )` (j = 0..m-1)

Result:
- `C(x) = A(x) * B(x) = sum( ck x^k )`,  
  where `ck = sum( ai * bj )` for all `i, j` with `i + j = k`.

**Complexity:**  
- Time: Θ(n * m), for typical equal sizes Θ(n²)  
- Space: Θ(n + m)

---

### 2.2 Naive O(n²) Multiplication (Parallel)

**Idea:** Same algorithm, but rows are processed in parallel.

We split the range `[0, n)` of indices of `A` into subranges, and each task computes a partial result for its subrange of `i`. Partial results are then summed.

**Parallel strategy:**

- The work is executed by a **ForkJoinTask** (`NaiveTask`) with range `[start, end)` over indices of `A`.
- If the range length `end - start` is small (≤ `threshold`), the task:
  - Performs the naive O(n²) multiplication loop only for `i` in `[start, end)`.
  - Writes into its **own local result array**.
- If the range is larger, the task:
  - Is split into two subranges.
  - Forks the left subtask, computes the right subtask, then joins the left.
  - Merges the two partial result arrays by element-wise addition.

**Key points:**
- Associativity of addition allows summation of partial results without locking.
- Using local result arrays per task avoids write conflicts.

**Complexity:**
- Total arithmetic work remains Θ(n²).
- In an ideal parallel execution with `p` cores, the effective runtime can approach Θ(n² / p).

---

### 2.3 Karatsuba Multiplication (Sequential)

**Idea:** Use a divide-and-conquer method that reduces the number of multiplications from 4 to 3 per recursive step.

Assume equal-length padded arrays `A` and `B` of size `n`, where `n` is a power of 2. Split into halves:

- `A = a0 + a1 * x^m`, `B = b0 + b1 * x^m`, where `m = n/2`.

Compute:

- `z0 = a0 * b0`  
- `z1 = a1 * b1`  
- `z2 = (a0 + a1) * (b0 + b1)`

Then:

- `A * B = z0 + (z2 - z0 - z1) * x^m + z1 * x^(2m)`

**Recursive structure:**

1. Pad both coefficient arrays to a common length `n = 2^k ≥ max(len(A), len(B))`.
2. If `n` is small (`≤ CUTOFF`), fall back to **naive convolution**.
3. Otherwise:
   - Split A, B.
   - Recursively compute `z0`, `z1`, `z2`.
   - Combine the results as above.

**Complexity:**

- Recurrence: `T(n) = 3T(n/2) + O(n)`
- Solution: `T(n) = Θ(n^{log₂ 3}) ≈ Θ(n^{1.585})`
- Better asymptotically than Θ(n²) when `n` is large.

---

### 2.4 Karatsuba Multiplication (Parallel)

**Idea:** Same recursion, but some of the three recursive multiplications (`z0`, `z1`, `z2`) are executed in parallel.

**Parallel strategy:**

- A `KaratsubaTask` (`RecursiveTask<long[]>`) represents the multiplication of two coefficient arrays.
- If the problem size (`n`) is small (`≤ cutoff`), we fall back to naive sequential multiplication.
- For larger `n`:
  - Split `A` and `B` into halves.
  - Spawn parallel tasks for some of the products (e.g. `z0` and `z1`).
  - Compute one product (`z2`) in the current thread.
  - Combine the results.

This leverages **parallelism at each recursive level**, while the cutoff prevents oversplitting.

**Complexity:**

- Total arithmetic work still Θ(n^{log₂ 3}).
- With `p` cores and proper threshold tuning, practical runtime can approach Θ(n^{log₂ 3} / p) for sufficiently large `n`.

---

## 3. Synchronization & Parallelization Details

### 3.1 Execution Environment: ForkJoinPool

Both parallel variants use:

- `java.util.concurrent.ForkJoinPool`
- Work units implemented as `RecursiveTask<long[]>`.

**Reasons for using ForkJoin:**

- Automatically reuses a **fixed pool** of worker threads, instead of creating new threads at every recursive call.
- Supports **work-stealing**:
  - If a worker thread blocks waiting for a `join()`, it can steal and execute other tasks.
  - This reduces the risk of deadlock and improves load balancing.

---

### 3.2 Avoiding Excessive Thread Creation

**Problem:**  
If we created a new thread for each recursive subproblem, the number of threads would explode and the overhead would overshadow any speedup.

**Solution:**

- Use a fixed-size pool (ForkJoin’s internal pool).
- Use a **cutoff / threshold** parameter:
  - Below this size, recursion **stops spawning new tasks** and runs sequential code instead.
  - This controls granularity and reduces overhead.

Relevant fields:

- Naive parallel: `threshold` – maximum number of indices of `A` handled in one task.
- Karatsuba parallel: `cutoff` – minimum array length to justify further parallel recursion.

---

### 3.4 Avoiding Deadlock with Thread Pools

**Potential issue:**

- With a fixed-size thread pool, tasks that submit other tasks to the **same pool** and then block on them (`Future.get()`) can deadlock if all threads are blocked waiting for tasks that cannot start.

**How ForkJoin avoids this:**

- `RecursiveTask` uses `fork()` and `join()` rather than blocking on `Future.get()` in a naive way.
- When a worker thread calls `join()`:
  - If the child task is not complete, the worker tries to **execute other available tasks** (work-stealing) instead of waiting idle.
- Combined with:
  - A reasonable pool size.
  - The usage of cutoffs.
- This design avoids deadlock and makes parallel divide-and-conquer practical.

---

## 4. Performance Measurements

### 4.1 Measurement Setup

**Benchmark harness:**

- Implemented in `Main.java`.
- Parameters:
  - `degree`: degree of the polynomials (default: 10000).
  - `repetitions`: how many times each algorithm is executed (default: 30).
- For given `degree` and `repetitions`:
  1. Generate two random polynomials `a` and `b` with integer coefficients.
  2. Compute a **reference result** using the naive sequential variant.
  3. For each multiplier:
     - Run `multiply(a, b)` `repetitions` times.
     - Measure elapsed time using `System.nanoTime()`.
     - Verify correctness by comparing to the reference result.

---

### 4.2 Example Results (Illustrative)

Assume:
- Degree: 10000  
- Repetitions: 30  

| Variant                        | Time (ms) |
|--------------------------------|-----------|
| Naive O(n²) – Sequential       | 1134.858  |
| Naive O(n²) – Parallel         | 379.612   |
| Karatsuba – Sequential         | 440.395   |
| Karatsuba – Parallel           | 197.985   |

---

## 5. Conclusion

This project demonstrates how:

- **Algorithm choice** (naive vs. Karatsuba) impacts asymptotic complexity.
- **Parallelization strategy** (task granularity, work-stealing, recursive decomposition) can significantly speed up polynomial multiplication on multi-core systems.
- **Synchronization** can be minimized:
  - By making tasks operate on local data.
  - Combining results in a tree-like fashion without explicit locks.
- **Practical performance** is a balance between:
  - Algorithmic complexity,
  - Parallel overhead,
  - Implementation details such as thresholds, memory usage, and JVM behavior.