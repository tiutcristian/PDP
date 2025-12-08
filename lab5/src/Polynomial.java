import java.util.Arrays;
import java.util.Random;

public class Polynomial {
    private final long[] coeffs;

    public Polynomial(long[] coeffs) {
        int n = coeffs.length;
        while (n > 1 && coeffs[n - 1] == 0L) {
            n--;
        }
        this.coeffs = Arrays.copyOf(coeffs, n);
    }

    public int size() {
        return coeffs.length;
    }

    public long[] getCoeffsCopy() {
        return Arrays.copyOf(coeffs, coeffs.length);
    }

    public static Polynomial random(int degree, int maxAbsCoeff, Random rnd) {
        long[] c = new long[degree + 1];
        for (int i = 0; i <= degree; i++) {
            c[i] = rnd.nextInt(2 * maxAbsCoeff + 1) - maxAbsCoeff;
        }
        return new Polynomial(c);
    }

    public boolean equalsPolynomial(Polynomial other) {
        return Arrays.equals(this.coeffs, other.coeffs);
    }
}