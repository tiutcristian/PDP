import java.util.Random;

public class Main {

    public static double[][] randMat(int rows, int cols, Random rnd) {
        double[][] M = new double[rows][cols];
        for (int i = 0; i < rows; i++)
            for (int j = 0; j < cols; j++)
                M[i][j] = rnd.nextDouble() * 2 - 1; // [-1,1)
        return M;
    }

    public static void main(String[] args) throws Exception {
        // === Configuration ===
        int m = 900, n = 900, p = 900;
        int threads = 4;
        Strategy strategy = Strategy.COL_CHUNKS;
        int runs = 5;
        // =====================

        System.out.printf(
                "Matrix %dx%d * %dx%d | threads=%d | strategy=%s%n",
                m, n, n, p, threads, strategy);

        Random rnd = new Random(42);
        double[][] A = randMat(m, n, rnd);
        double[][] B = randMat(n, p, rnd);

        for (int run = 1; run <= runs; run++) {
            Result r = MatrixMultiplier.multiply(A, B, threads, strategy);

            System.out.printf("%.3f ms%n", r.nanos / 1e6);
        }
    }
}