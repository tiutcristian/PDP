import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicBoolean;

public class Benchmark {
    final WarehouseSystem system;
    final int numThreads;
    final int opsPerThread;

    public Benchmark(WarehouseSystem system, int numThreads, int opsPerThread) {
        this.system = system;
        this.numThreads = numThreads;
        this.opsPerThread = opsPerThread;
    }

    public void runAll() throws Exception {
        System.out.println("Initial total sum: " + system.sumAll());

        ExecutorService ex = Executors.newFixedThreadPool(numThreads);
        List<Future<Integer>> futures = new ArrayList<>();

        long start = System.nanoTime();

        for (int t = 0; t < numThreads; ++t) {
            Worker w = new Worker(system, opsPerThread, 0xC0FFEE + t);
            futures.add(ex.submit(w));
        }

        // concurrently run occasional global inventory checks in a separate thread until completion
        ScheduledExecutorService checker = Executors.newSingleThreadScheduledExecutor();
        AtomicBoolean done = new AtomicBoolean(false);
        checker.scheduleAtFixedRate(() -> {
            if (done.get()) return;
            boolean ok = system.checkInvariants();
            if (!ok) {
                System.err.println("Invariant check failed during scheduled checks!");
                System.exit(2);
            }
        }, 200, 200, TimeUnit.MILLISECONDS);

        int totalSuccess = 0;
        for (Future<Integer> f : futures) totalSuccess += f.get();

        long end = System.nanoTime();
        done.set(true);
        checker.shutdownNow();
        ex.shutdownNow();

        double seconds = (end - start) / 1e9; // transform from nanos to s
        System.out.printf("Threads finished. Successful moves: %d/%d (%.2f sec)\n",
                totalSuccess, numThreads * opsPerThread, seconds);

        System.out.println("Final invariant check: " + (system.checkInvariants() ? "OK" : "FAILED"));
        System.out.println("Final total sum: " + system.sumAll());
    }
}