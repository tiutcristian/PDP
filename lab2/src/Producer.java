public final class Producer implements Runnable {
    private final double[] a, b;
    private final BoundedQueue q;

    public Producer(double[] a, double[] b, BoundedQueue q) {
        this.a = a;
        this.b = b;
        this.q = q;
    }

    @Override public void run() {
        try {
            for (int i = 0; i < a.length; i++) {
                q.put(a[i] * b[i]);
            }
        } catch (InterruptedException ie) {
            Thread.currentThread().interrupt();
        } finally {
            q.close();
        }
    }
}