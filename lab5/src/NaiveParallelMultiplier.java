import java.util.concurrent.ForkJoinPool;
import java.util.concurrent.RecursiveTask;

public class NaiveParallelMultiplier implements PolynomialMultiplier {

    private final int threshold; // rows of A per task

    public NaiveParallelMultiplier(int threshold) {
        this.threshold = threshold;
    }

    @Override
    public Polynomial multiply(Polynomial a, Polynomial b) {
        long[] A = a.getCoeffsCopy();
        long[] B = b.getCoeffsCopy();

        NaiveTask root = new NaiveTask(A, B, 0, A.length, threshold);
        long[] result = ForkJoinPool.commonPool().invoke(root);
        return new Polynomial(result);
    }

    @Override
    public String name() {
        return "Naive O(n^2) - Parallel";
    }

    private static class NaiveTask extends RecursiveTask<long[]> {
        private final long[] A, B;
        private final int start, end;
        private final int threshold;

        NaiveTask(long[] A, long[] B, int start, int end, int threshold) {
            this.A = A;
            this.B = B;
            this.start = start;
            this.end = end;
            this.threshold = threshold;
        }

        @Override
        protected long[] compute() {
            int lenA = end - start;
            int nRes = A.length + B.length - 1;
            if (lenA <= threshold) {
                long[] res = new long[nRes];
                for (int i = start; i < end; i++) {
                    long ai = A[i];
                    for (int j = 0; j < B.length; j++) {
                        res[i + j] += ai * B[j];
                    }
                }
                return res;
            } else {
                int mid = (start + end) / 2;
                NaiveTask left = new NaiveTask(A, B, start, mid, threshold);
                NaiveTask right = new NaiveTask(A, B, mid, end, threshold);
                left.fork();
                long[] rightRes = right.compute();
                long[] leftRes = left.join();

                for (int k = 0; k < nRes; k++) {
                    rightRes[k] += leftRes[k];
                }
                return rightRes;
            }
        }
    }
}