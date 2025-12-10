import java.io.BufferedReader;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;
import java.util.StringTokenizer;

public class Main {
    private static Graph readGraphFromConfig(String configPath) throws IOException {
        int n, m;
        Graph g;
        try (BufferedReader br = Files.newBufferedReader(Paths.get(configPath), StandardCharsets.UTF_8)) {
            String line;
            line = br.readLine();
            n = Integer.parseInt(line.trim());
            line = br.readLine();
            m = Integer.parseInt(line.trim());
            g = new Graph(n);
            for (int i = 0; i < m; i++) {
                line = br.readLine();
                line = line.trim();
                StringTokenizer st = new StringTokenizer(line);
                int u = Integer.parseInt(st.nextToken());
                int v = Integer.parseInt(st.nextToken());
                g.addEdge(u, v);
            }
        }
        return g;
    }

    private static void printCycleAndTime(List<Integer> cycle, long timeMs) {
        if (cycle == null) {
            System.out.println("No Hamiltonian cycle found.");
        } else {
            List<Integer> fullCycle = new ArrayList<>(cycle);
            fullCycle.add(cycle.get(0)); // close the cycle visually
            System.out.println("Hamiltonian cycle: " + fullCycle);
        }
        System.out.println("Time: " + timeMs + " ms");
    }

    public static void main(String[] args) {
        // Configuration
        int numThreads = 10;
        int startVertex = 0;
        String configPath = "config.txt";

        // read graph
        Graph g;
        try {
            g = readGraphFromConfig(configPath);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }

        // ---- Run THREADS mode ----
        System.out.println("=== THREADS mode ===");
        ParallelThreadsSolver threadSolver = new ParallelThreadsSolver(g, startVertex, numThreads);
        long startTimeThreads = System.currentTimeMillis();
        List<Integer> cycleThreads = threadSolver.findHamiltonianCycle();
        long endTimeThreads = System.currentTimeMillis();
        printCycleAndTime(cycleThreads, endTimeThreads - startTimeThreads);

        // ---- Run FORKJOIN mode ----
        System.out.println("\n=== FORKJOIN mode ===");
        ForkJoinSolver forkJoinSolver = new ForkJoinSolver(g, startVertex, numThreads);
        long startTimeFJ = System.currentTimeMillis();
        List<Integer> cycleFJ = forkJoinSolver.findHamiltonianCycle();
        long endTimeFJ = System.currentTimeMillis();
        printCycleAndTime(cycleFJ, endTimeFJ - startTimeFJ);
    }
}