import java.util.Random;

public class Main {
    public static void main(String[] args) {
        int degree = 10000;
        int repetitions = 30;

        System.out.println("Degree: " + degree + ", repetitions: " + repetitions);

        Random rnd = new Random(42);
        Polynomial a = Polynomial.random(degree, 10, rnd);
        Polynomial b = Polynomial.random(degree, 10, rnd);

        PolynomialMultiplier[] multipliers = new PolynomialMultiplier[]{
                new NaiveSequentialMultiplier(),
                new NaiveParallelMultiplier(64),
                new KaratsubaSequentialMultiplier(),
                new KaratsubaParallelMultiplier(64)
        };

        // reference result for correctness check
        Polynomial reference = multipliers[0].multiply(a, b);

        for (PolynomialMultiplier m : multipliers) {
            long start = System.nanoTime();
            Polynomial last = null;
            for (int i = 0; i < repetitions; i++) {
                last = m.multiply(a, b);
            }
            long elapsed = System.nanoTime() - start;
            double ms = elapsed / 1_000_000.0;

            boolean ok = reference.equalsPolynomial(last);

            System.out.printf("%-30s time = %8.3f ms, correct = %s%n",
                    m.name(), ms, ok);
        }
    }
}