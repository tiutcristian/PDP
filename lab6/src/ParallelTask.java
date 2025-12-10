import java.util.ArrayList;
import java.util.List;

public class ParallelTask implements Runnable {
    private final ParallelThreadsSolver solver;
    private final List<Integer> path;
    private final boolean[] visited;
    private final int current;
    private final int budget;  // max number of threads for this subtree

    public ParallelTask(ParallelThreadsSolver solver,
                        List<Integer> path,
                        boolean[] visited,
                        int current,
                        int budget) {
        this.solver = solver;
        this.path = path;
        this.visited = visited;
        this.current = current;
        this.budget = budget;
    }

    @Override
    public void run() {
        backtrack(path, visited, current, budget);
    }

    private void backtrack(List<Integer> path,
                           boolean[] visited,
                           int current,
                           int budget) {
        if (solver.found.get()) return;

        if (path.size() == solver.graph.n) {
            if (solver.graph.hasEdge(current, solver.startVertex)) {
                solver.setSolution(path);
            }
            return;
        }

        List<Integer> candidates = new ArrayList<>();
        for (int next : solver.graph.neighbors(current)) {
            if (!visited[next]) {
                candidates.add(next);
            }
        }

        if (candidates.isEmpty()) {
            return;
        }

        if (budget <= 1 || candidates.size() == 1) {
            solver.dfsSequential(new ArrayList<>(path), visited.clone(), current);
            return;
        }

        int k = candidates.size();
        int base = budget / k;
        int rem = budget % k;
        Thread[] threads = new Thread[k];

        for (int i = 0; i < k; i++) {
            int next = candidates.get(i);
            boolean[] visitedCopy = visited.clone();
            visitedCopy[next] = true;
            List<Integer> newPath = new ArrayList<>(path);
            newPath.add(next);
            int childBudget = base + (i < rem ? 1 : 0);
            if (childBudget <= 0) {
                childBudget = 1;
            }

            ParallelTask child = new ParallelTask(
                    solver,
                    newPath,
                    visitedCopy,
                    next,
                    childBudget
            );
            threads[i] = new Thread(child);
            threads[i].start();
        }

        for (Thread t : threads) {
            if (t == null) continue;
            try {
                t.join();
            } catch (InterruptedException ignored) {}
            if (solver.found.get()) break;
        }
    }
}