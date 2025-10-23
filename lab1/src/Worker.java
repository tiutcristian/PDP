import java.util.HashMap;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.Callable;

public class Worker implements Callable<Integer> {
    final WarehouseSystem system;
    final int ops;
    final Random rng;

    public Worker(WarehouseSystem system, int ops, long seed) {
        this.system = system;
        this.ops = ops;
        this.rng = new Random(seed);
    }

    @Override
    public Integer call() {
        int success = 0;
        for (int i = 0; i < ops; ++i) {
            // choose random source, dest, and set of products
            int s = rng.nextInt(system.W);
            int d;
            do { d = rng.nextInt(system.W); } while (d == s);

            int numProducts = 1 + rng.nextInt(Math.min(system.P, 4));
            Map<Integer,Integer> moves = new HashMap<>();
            for (int k = 0; k < numProducts; ++k) {
                int p = rng.nextInt(system.P);
                int maxAmt;
                switch (system.granularity) {
                    case GLOBAL:
                        system.globalLock.lock();
                        try { maxAmt = system.inventory[s][p]; } finally { system.globalLock.unlock(); }
                        break;
                    case WAREHOUSE:
                        system.warehouseLocks[s].lock();
                        try { maxAmt = system.inventory[s][p]; } finally { system.warehouseLocks[s].unlock(); }
                        break;
                    case PRODUCT:
                        system.productLocks[p].lock();
                        try { maxAmt = system.inventory[s][p]; } finally { system.productLocks[p].unlock(); }
                        break;
                    default:
                        maxAmt = system.inventory[s][p];
                }
                if (maxAmt <= 0) continue; // can't move this product
                int amt = 1 + rng.nextInt(Math.max(1, maxAmt/2));
                moves.merge(p, amt, Integer::sum);
            }

            if (moves.isEmpty()) continue;
            boolean ok = system.move(s, d, moves);
            if (ok) ++success;

            if ((i & 0x1FFF) == 0) { // evaluates to true once every 8192 = 2^13 iterations of the loop
                boolean inv = system.checkInvariants();
                if (!inv) {
                    throw new IllegalStateException("Invariant failed during worker ops");
                }
            }
        }
        return success;
    }
}