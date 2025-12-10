import java.util.ArrayList;
import java.util.List;

public class Graph {
    final int n;
    final List<Integer>[] adj;

    @SuppressWarnings("unchecked")
    public Graph(int n) {
        this.n = n;
        this.adj = new ArrayList[n];
        for (int i = 0; i < n; i++) {
            adj[i] = new ArrayList<>();
        }
    }

    public void addEdge(int u, int v) {
        adj[u].add(v);
    }

    public boolean hasEdge(int u, int v) {
        return adj[u].contains(v);
    }

    public List<Integer> neighbors(int u) {
        return adj[u];
    }
}