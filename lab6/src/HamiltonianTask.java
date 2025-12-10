import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.RecursiveTask;

public class HamiltonianTask extends RecursiveTask<List<Integer>> {
    private final ForkJoinSolver solver;
    private final List<Integer> path;
    private final boolean[] visited;
    private final int current;

    public HamiltonianTask(ForkJoinSolver solver,
                           List<Integer> path,
                           boolean[] visited,
                           int current) {
        this.solver = solver;
        this.path = path;
        this.visited = visited;
        this.current = current;
    }

    @Override
    protected List<Integer> compute() {
        if (solver.found.get()) {
            return null;
        }

        if (path.size() == solver.graph.n) {
            if (solver.graph.hasEdge(current, solver.startVertex)) {
                solver.found.set(true);
                return new ArrayList<>(path);
            }
            return null;
        }

        List<Integer> candidates = new ArrayList<>();
        for (int next : solver.graph.neighbors(current)) {
            if (!visited[next]) {
                candidates.add(next);
            }
        }

        if (candidates.isEmpty()) {
            return null;
        }

        int k = candidates.size();

        if (k == 1) {
            int next = candidates.getFirst();
            boolean[] visitedCopy = visited.clone();
            visitedCopy[next] = true;
            List<Integer> newPath = new ArrayList<>(path);
            newPath.add(next);

            HamiltonianTask child = new HamiltonianTask(solver, newPath, visitedCopy, next);
            return child.compute();
        }

        List<HamiltonianTask> subtasks = new ArrayList<>(k);

        int first = candidates.getFirst();
        boolean[] visitedFirst = visited.clone();
        visitedFirst[first] = true;
        List<Integer> pathFirst = new ArrayList<>(path);
        pathFirst.add(first);
        HamiltonianTask firstTask = new HamiltonianTask(solver, pathFirst, visitedFirst, first);

        for (int i = 1; i < k; i++) {
            int next = candidates.get(i);
            boolean[] visitedCopy = visited.clone();
            visitedCopy[next] = true;
            List<Integer> newPath = new ArrayList<>(path);
            newPath.add(next);

            HamiltonianTask child = new HamiltonianTask(solver, newPath, visitedCopy, next);
            subtasks.add(child);
            child.fork();
        }

        List<Integer> result = firstTask.compute();
        if (result != null) {
            solver.found.set(true);
            return result;
        }

        for (HamiltonianTask t : subtasks) {
            List<Integer> childResult = t.join();
            if (childResult != null) {
                solver.found.set(true);
                return childResult;
            }
        }

        return null;
    }
}