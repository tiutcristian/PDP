import java.util.Arrays;

public class KaratsubaSequentialMultiplier implements PolynomialMultiplier {

    private static final int CUTOFF = 32; // below this, use naive

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

        long[] full = karatsuba(aPadded, bPadded);
        int len = A.length + B.length - 1;
        long[] trimmed = Arrays.copyOf(full, len);
        return new Polynomial(trimmed);
    }

    @Override
    public String name() {
        return "Karatsuba - Sequential";
    }

    private static long[] karatsuba(long[] A, long[] B) {
        int n = A.length;
        if (n <= CUTOFF) {
            return naiveConvolution(A, B);
        }

        int m = n / 2;

        long[] a0 = Arrays.copyOfRange(A, 0, m);
        long[] a1 = Arrays.copyOfRange(A, m, n);
        long[] b0 = Arrays.copyOfRange(B, 0, m);
        long[] b1 = Arrays.copyOfRange(B, m, n);

        long[] z0 = karatsuba(a0, b0);
        long[] z1 = karatsuba(a1, b1);

        long[] a0PlusA1 = new long[m];
        long[] b0PlusB1 = new long[m];
        for (int i = 0; i < m; i++) {
            a0PlusA1[i] = a0[i] + a1[i];
            b0PlusB1[i] = b0[i] + b1[i];
        }

        long[] z2 = karatsuba(a0PlusA1, b0PlusB1);

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

    static long[] naiveConvolution(long[] A, long[] B) {
        long[] res = new long[A.length + B.length - 1];
        for (int i = 0; i < A.length; i++) {
            long ai = A[i];
            for (int j = 0; j < B.length; j++) {
                res[i + j] += ai * B[j];
            }
        }
        return res;
    }
}