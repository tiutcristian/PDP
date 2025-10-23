import java.util.Random;

public class Main {

    private static final int N = 5_000_000;
    private static final int QUEUE_CAPACITY = 15;
    private static final int REPEATS = 5;

    public static void main(String[] args) throws Exception {
        System.out.printf("N=%d  repeats=%d \n", N, REPEATS);

        double[] a = new double[N];
        double[] b = new double[N];
        Random r = new Random(43);
        for (int i = 0; i < N; i++) {
            a[i] = r.nextDouble() - 0.5;
            b[i] = r.nextDouble() - 0.5;
        }

        double expected = dotProd(a, b);

        double last = 0.0;
        for (int k = 0; k < REPEATS; k++) last = runOnce(a, b);
        checkClose("fixed-capacity", expected, last);
    }

    private static double runOnce(double[] a, double[] b) throws InterruptedException {
        BoundedQueue q = new BoundedQueue(QUEUE_CAPACITY);
        Consumer consumer = new Consumer(q);

        Thread prod = new Thread(new Producer(a, b, q), "producer");
        Thread cons = new Thread(consumer, "consumer");

        long t0 = System.nanoTime();
        prod.start();
        cons.start();
        prod.join();
        cons.join();
        long t1 = System.nanoTime();

        double result = consumer.getSum();
        double millis = (t1 - t0) / 1_000_000.0;
        System.out.printf("Queue=%-6d  time=%8.3f ms  result=%.6f%n", Main.QUEUE_CAPACITY, millis, result);
        return result;
    }

    private static double dotProd(double[] a, double[] b) {
        double s = 0.0;
        for (int i = 0; i < a.length; i++) s += a[i] * b[i];
        return s;
    }

    private static void checkClose(String tag, double expected, double actual) {
        double eps = Math.max(1e-9, Math.abs(expected) * 1e-12);
        if (Math.abs(expected - actual) > eps) {
            throw new AssertionError(tag + " mismatch: expected=" + expected + " actual=" + actual);
        }
    }
}