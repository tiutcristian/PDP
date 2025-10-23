import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.ReentrantLock;

public class BoundedQueue {
    private final Double[] buf;
    private int head = 0, tail = 0, count = 0;
    private boolean closed = false;

    private final ReentrantLock lock = new ReentrantLock();
    private final Condition notEmpty = lock.newCondition();
    private final Condition notFull  = lock.newCondition();

    public BoundedQueue(int capacity) {
        this.buf = new Double[capacity];
    }

    public void put(Double x) throws InterruptedException {
        lock.lock();
        try {
            while (count == buf.length && !closed) {
                notFull.await();
            }
            if (closed) throw new IllegalStateException("queue closed");
            buf[tail] = x;
            tail = (tail + 1) % buf.length;
            count++;
            notEmpty.signal();
        } finally {
            lock.unlock();
        }
    }

    public Double take() throws InterruptedException {
        lock.lock();
        try {
            while (count == 0) {
                if (closed) return null;
                notEmpty.await();
            }
            Double x = buf[head];
            buf[head] = null;
            head = (head + 1) % buf.length;
            count--;
            notFull.signal();
            return x;
        } finally {
            lock.unlock();
        }
    }

    public void close() {
        lock.lock();
        try {
            closed = true;
            notEmpty.signalAll();
            notFull.signalAll();
        } finally {
            lock.unlock();
        }
    }
}