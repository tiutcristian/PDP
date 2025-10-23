public final class Consumer implements Runnable {
    private final BoundedQueue q;
    private double sum = 0.0;

    public Consumer(BoundedQueue q) {
        this.q = q;
    }

    public double getSum() {
        return sum;
    }

    @Override public void run() {
        try {
            while (true) {
                Double v = q.take();
                if (v == null) {
                    break; // queue closed & empty
                }
                sum += v;
            }
        } catch (InterruptedException ie) {
            Thread.currentThread().interrupt();
        }
    }
}