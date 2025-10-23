import java.util.Map;
import java.util.Random;
import java.util.concurrent.locks.ReentrantLock;

public class WarehouseSystem {
    final int W; // warehouses
    final int P; // products
    final LockGranularity granularity;

    // inventory[w][p] == quantity of product p in warehouse w
    final int[][] inventory;

    // Locks
    final ReentrantLock globalLock = new ReentrantLock();
    final ReentrantLock[] warehouseLocks; // length W
    final ReentrantLock[] productLocks;   // length P

    // initial totals for invariant check
    final long[] initialTotals; // per-product total

    final Random rng = new Random(0xC0FFEE);

    public WarehouseSystem(int warehouses, int products, LockGranularity granularity) {
        this.W = warehouses;
        this.P = products;
        this.granularity = granularity;
        this.inventory = new int[W][P];
        this.warehouseLocks = new ReentrantLock[W];
        this.productLocks = new ReentrantLock[P];
        for (int i = 0; i < W; ++i) warehouseLocks[i] = new ReentrantLock();
        for (int j = 0; j < P; ++j) productLocks[j] = new ReentrantLock();

        for (int i = 0; i < W; ++i) {
            for (int j = 0; j < P; ++j) {
                // moderate initial stock
                inventory[i][j] = 100 + rng.nextInt(50);
            }
        }

        initialTotals = new long[P];
        for (int p = 0; p < P; ++p) {
            long s = 0;
            for (int w = 0; w < W; ++w) s += inventory[w][p];
            initialTotals[p] = s;
        }
    }


    public boolean move(int source, int dest, Map<Integer, Integer> moves) {
        if (source == dest) return true; // nothing to do

        switch (granularity) {
            case GLOBAL:
                globalLock.lock();
                try {
                    return moveUnprotected(source, dest, moves);
                } finally {
                    globalLock.unlock();
                }
            case WAREHOUSE:
                // acquire warehouse locks in id order
                int a = Math.min(source, dest);
                int b = Math.max(source, dest);
                warehouseLocks[a].lock();
                warehouseLocks[b].lock();
                try {
                    return moveUnprotected(source, dest, moves);
                } finally {
                    warehouseLocks[b].unlock();
                    warehouseLocks[a].unlock();
                }
            case PRODUCT:
                // lock products involved in ascending order
                int[] prods = moves.keySet()
                        .stream()
                        .mapToInt(i->i)
                        .sorted()
                        .toArray();

                for (int p : prods) productLocks[p].lock();

                try {
                    return moveUnprotected(source, dest, moves);
                } finally {
                    for (int i = prods.length - 1; i >= 0; --i) productLocks[prods[i]].unlock();
                }
            default:
                throw new IllegalStateException("Unknown granularity");
        }
    }

    // Assumes caller holds appropriate locks
    private boolean moveUnprotected(int source, int dest, Map<Integer, Integer> moves) {
        // verify enough stock in source for all products
        for (Map.Entry<Integer, Integer> e : moves.entrySet()) {
            int p = e.getKey();
            int amt = e.getValue();
            if (amt < 0) return false;
            if (inventory[source][p] < amt) return false;
        }

        // perform move
        for (Map.Entry<Integer, Integer> e : moves.entrySet()) {
            int p = e.getKey();
            int amt = e.getValue();
            inventory[source][p] -= amt;
            inventory[dest][p] += amt;
        }

        return true;
    }

    public boolean checkInvariants() {
        long[] totals = new long[P];
        switch (granularity) {
            case GLOBAL:
                globalLock.lock();
                try {
                    for (int p = 0; p < P; ++p) {
                        long s = 0;
                        for (int w = 0; w < W; ++w) s += inventory[w][p];
                        totals[p] = s;
                    }
                } finally { globalLock.unlock(); }
                break;
            case WAREHOUSE:
                // lock all warehouses in order
                for (int w = 0; w < W; ++w) warehouseLocks[w].lock();
                try {
                    for (int p = 0; p < P; ++p) {
                        long s = 0;
                        for (int w = 0; w < W; ++w) s += inventory[w][p];
                        totals[p] = s;
                    }
                } finally {
                    for (int w = W - 1; w >= 0; --w) warehouseLocks[w].unlock();
                }
                break;
            case PRODUCT:
                // lock all products in order
                for (int p = 0; p < P; ++p) productLocks[p].lock();
                try {
                    for (int p = 0; p < P; ++p) {
                        long s = 0;
                        for (int w = 0; w < W; ++w) s += inventory[w][p];
                        totals[p] = s;
                    }
                } finally {
                    for (int p = P - 1; p >= 0; --p) productLocks[p].unlock();
                }
                break;
            default:
                throw new IllegalStateException("unknown granularity");
        }

        for (int p = 0; p < P; ++p) {
            if (totals[p] != initialTotals[p]) {
                System.err.printf("Invariant violated for product %d: expected=%d actual=%d\n",
                        p, initialTotals[p], totals[p]);
                return false;
            }
        }
        return true;
    }

    public long sumAll() {
        long s = 0;
        for (int w = 0; w < W; ++w) for (int p = 0; p < P; ++p) s += inventory[w][p];
        return s;
    }
}