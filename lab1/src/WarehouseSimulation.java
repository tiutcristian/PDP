import java.io.FileWriter;
import java.io.IOException;

public class WarehouseSimulation {
    // ------------------ Configuration / entrypoint ------------------
    public static void main(String[] args) throws Exception {
        int[] warehouses = {4, 8, 16};
        int[] products = {100, 500};
        int[] threads = {1, 2, 4, 8};
        int[] ops = {5000, 20000};
        LockGranularity[] granularities = {
                LockGranularity.GLOBAL,
                LockGranularity.WAREHOUSE,
                LockGranularity.PRODUCT
        };

        String outputFile = "results.csv";

        try (FileWriter writer = new FileWriter(outputFile)) {
            writer.write("granularity,numWarehouses,numProducts,numThreads,opsPerThread,seconds\n");
            System.out.println("granularity,numWarehouses,numProducts,numThreads,opsPerThread,seconds");

            for (LockGranularity gran : granularities) {
                for (int w : warehouses) {
                    for (int p : products) {
                        for (int t : threads) {
                            for (int o : ops) {
                                WarehouseSystem system = new WarehouseSystem(w, p, gran);

                                Benchmark bench = new Benchmark(system, t, o);

                                long start = System.nanoTime();
                                bench.runAll();
                                long end = System.nanoTime();

                                double seconds = (end - start) / 1e9;
                                writer.write(String.format("%s,%d,%d,%d,%d,%.3f\n",
                                        gran, w, p, t, o, seconds));
                                System.out.printf("%s,%d,%d,%d,%d,%.3f\n",
                                        gran, w, p, t, o, seconds);
                                writer.flush();
                            }
                        }
                    }
                }
            }
        } catch (IOException e) {
            System.err.println("Error writing CSV: " + e.getMessage());
        }

        System.out.println("Benchmark complete. Results saved to results.csv");
    }
}
