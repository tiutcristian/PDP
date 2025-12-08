import java.util.Arrays;
import java.util.concurrent.ForkJoinPool;
import java.util.concurrent.RecursiveTask;

public class KaratsubaParallelMultiplier implements PolynomialMultiplier {

    private final int cutoff;

    public KaratsubaParallelMultiplier(int cutoff) {
        this.cutoff = cutoff;
    }

    @Override
    public Polynomial multiply(Polynomial a, Polynomial b) {
        long[] A = a.getCoeffsCopy();
        long[] B = b.getCoeffsCopy();

        if (A.length == 0 || B.length == 0) {
            return new Polynomial(new long[]{0});
        }

        int n = 1;
        while (n < A.length || n < B.length) {
            n <<= 1;
        }

        long[] aPadded = Arrays.copyOf(A, n);
        long[] bPadded = Arrays.copyOf(B, n);

        KaratsubaTask root = new KaratsubaTask(aPadded, bPadded, cutoff);
        long[] full = ForkJoinPool.commonPool().invoke(root);

        int len = A.length + B.length - 1;
        long[] trimmed = Arrays.copyOf(full, len);
        return new Polynomial(trimmed);
    }

    @Override
    public String name() {
        return "Karatsuba - Parallel";
    }

    private static class KaratsubaTask extends RecursiveTask<long[]> {
        private final long[] A, B;
        private final int cutoff;

        KaratsubaTask(long[] A, long[] B, int cutoff) {
            this.A = A;
            this.B = B;
            this.cutoff = cutoff;
        }

        @Override
        protected long[] compute() {
            int n = A.length;
            if (n <= cutoff) {
                return KaratsubaSequentialMultiplier.naiveConvolution(A, B);
            }

            int m = n / 2;

            long[] a0 = Arrays.copyOfRange(A, 0, m);
            long[] a1 = Arrays.copyOfRange(A, m, n);
            long[] b0 = Arrays.copyOfRange(B, 0, m);
            long[] b1 = Arrays.copyOfRange(B, m, n);

            KaratsubaTask t0 = new KaratsubaTask(a0, b0, cutoff);
            KaratsubaTask t1 = new KaratsubaTask(a1, b1, cutoff);

            // run z0,z1 in parallel; z2 in current thread
            t0.fork();
            long[] z1 = t1.compute();
            long[] z0 = t0.join();

            long[] a0PlusA1 = new long[m];
            long[] b0PlusB1 = new long[m];
            for (int i = 0; i < m; i++) {
                a0PlusA1[i] = a0[i] + a1[i];
                b0PlusB1[i] = b0[i] + b1[i];
            }

            long[] z2 = new KaratsubaTask(a0PlusA1, b0PlusB1, cutoff).compute();

            for (int i = 0; i < z0.length; i++) {
                z2[i] -= z0[i];
            }
            for (int i = 0; i < z1.length; i++) {
                z2[i] -= z1[i];
            }

            long[] res = new long[2 * n - 1];
            for (int i = 0; i < z0.length; i++) {
                res[i] += z0[i];
            }
            for (int i = 0; i < z2.length; i++) {
                res[i + m] += z2[i];
            }
            for (int i = 0; i < z1.length; i++) {
                res[i + 2 * m] += z1[i];
            }
            return res;
        }
    }
}