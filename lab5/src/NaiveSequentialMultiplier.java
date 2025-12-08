public class NaiveSequentialMultiplier implements PolynomialMultiplier {

    @Override
    public Polynomial multiply(Polynomial a, Polynomial b) {
        long[] A = a.getCoeffsCopy();
        long[] B = b.getCoeffsCopy();
        int n = A.length;
        int m = B.length;
        long[] res = new long[n + m - 1];

        for (int i = 0; i < n; i++) {
            long ai = A[i];
            for (int j = 0; j < m; j++) {
                res[i + j] += ai * B[j];
            }
        }

        return new Polynomial(res);
    }

    @Override
    public String name() {
        return "Naive O(n^2) - Sequential";
    }
}