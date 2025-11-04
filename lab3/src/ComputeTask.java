import java.util.concurrent.Callable;

public class ComputeTask implements Callable<Void> {
    private final double[][] A;
    private final double[][] B;
    private final double[][] C;
    private final int startIdx;
    private final int endIdx;
    private final int p;
    private final int m;
    private final int threadId;
    private final Strategy strategy;

    public ComputeTask(double[][] A, double[][] B, double[][] C,
                       int startIdx, int endIdx,
                       int m, int p, int threadId,
                       Strategy strategy) {
        this.A = A;
        this.B = B;
        this.C = C;
        this.startIdx = startIdx;
        this.endIdx = endIdx;
        this.m = m;
        this.p = p;
        this.threadId = threadId;
        this.strategy = strategy;
    }

    @Override
    public Void call() {
        int total = m * p;
        switch (strategy) {
            case ROW_CHUNKS -> computeRowChunks();
            case COL_CHUNKS -> computeColChunks();
            case STRIDED -> computeStrided(total);
        }
        return null;
    }

    private double computeElement(int i, int j) {
        int n = A[0].length;
        double sum = 0.0;
        for (int k = 0; k < n; k++) {
            sum += A[i][k] * B[k][j];
        }
//        System.out.printf("Thread %d -> C[%d,%d]%n", threadId, i, j);
        return sum;
    }

    private void computeRowChunks() {
        for (int idx = startIdx; idx < endIdx; idx++) {
            int i = idx / p;
            int j = idx % p;
            C[i][j] = computeElement(i, j);
        }
    }

    private void computeColChunks() {
        for (int idx = startIdx; idx < endIdx; idx++) {
            int j = idx / m;
            int i = idx % m;
            C[i][j] = computeElement(i, j);
        }
    }

    private void computeStrided(int total) {
        for (int idx = startIdx; idx < total; idx += endIdx) {
            int i = idx / p;
            int j = idx % p;
            C[i][j] = computeElement(i, j);
        }
    }
}