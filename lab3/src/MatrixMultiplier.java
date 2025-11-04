import java.util.*;
import java.util.concurrent.*;

public class MatrixMultiplier {

    public static Result multiply(double[][] A, double[][] B, int threads,
                                  Strategy strategy) throws InterruptedException {
        int m = A.length, n = A[0].length, p = B[0].length;
        if (B.length != n)
            throw new IllegalArgumentException("Incompatible matrix shapes");

        double[][] C = new double[m][p];
        int total = m * p;
        ExecutorService pool = Executors.newFixedThreadPool(threads);
        List<Callable<Void>> jobs = new ArrayList<>();

        switch (strategy) {
            case ROW_CHUNKS, COL_CHUNKS -> {
                int base = total / threads;
                int rem = total % threads;
                int start = 0;
                for (int t = 0; t < threads; t++) {
                    int len = base + (t < rem ? 1 : 0);
                    jobs.add(new ComputeTask(A, B, C, start, start + len,
                            m, p , t, strategy));
                    start += len;
                }
            }
            case STRIDED -> {
                for (int t = 0; t < threads; t++) {
                    // For STRIDED, we pass total as endIdx so call() knows the step
                    jobs.add(new ComputeTask(A, B, C, t, threads,
                            m, p, t, strategy));
                }
            }
        }

        long t0 = System.nanoTime();
        pool.invokeAll(jobs);
        pool.shutdown();
        pool.awaitTermination(1, TimeUnit.HOURS);
        long t1 = System.nanoTime();

        return new Result(C, t1 - t0);
    }
}