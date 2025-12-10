import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ForkJoinPool;
import java.util.concurrent.atomic.AtomicBoolean;

public class ForkJoinSolver {
    final Graph graph;
    final int startVertex;
    final ForkJoinPool pool;
    final AtomicBoolean found = new AtomicBoolean(false);

    public ForkJoinSolver(Graph graph, int startVertex, int parallelism) {
        this.graph = graph;
        this.startVertex = startVertex;
        this.pool = new ForkJoinPool(Math.max(1, parallelism));
    }

    public List<Integer> findHamiltonianCycle() {
        boolean[] visited = new boolean[graph.n];
        List<Integer> path = new ArrayList<>();
        visited[startVertex] = true;
        path.add(startVertex);

        HamiltonianTask root = new HamiltonianTask(this, path, visited, startVertex);
        return pool.invoke(root);
    }
}