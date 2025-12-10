import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

public class ParallelThreadsSolver {
    final Graph graph;
    final int startVertex;
    final int maxThreads;

    final AtomicBoolean found = new AtomicBoolean(false);
    volatile List<Integer> solution = null;

    public ParallelThreadsSolver(Graph graph, int startVertex, int maxThreads) {
        this.graph = graph;
        this.startVertex = startVertex;
        this.maxThreads = Math.max(1, maxThreads);
    }

    public List<Integer> findHamiltonianCycle() {
        boolean[] visited = new boolean[graph.n];
        List<Integer> path = new ArrayList<>();

        visited[startVertex] = true;
        path.add(startVertex);

        if (maxThreads == 1) {
            dfsSequential(path, visited, startVertex);
        } else {
            ParallelTask root = new ParallelTask(this, path, visited, startVertex, maxThreads);
            root.run();
        }

        return solution;
    }

    void setSolution(List<Integer> path) {
        if (found.compareAndSet(false, true)) {
            solution = new ArrayList<>(path);
        }
    }

    void dfsSequential(List<Integer> path, boolean[] visited, int current) {
        if (found.get()) return;

        if (path.size() == graph.n) {
            if (graph.hasEdge(current, startVertex)) {
                setSolution(path);
            }
            return;
        }

        for (int next : graph.neighbors(current)) {
            if (found.get()) return;
            if (!visited[next]) {
                visited[next] = true;
                path.add(next);

                dfsSequential(path, visited, next);

                path.removeLast();
                visited[next] = false;
            }
        }
    }
}