import java.io.BufferedWriter;
import java.io.FileWriter;
import java.io.IOException;

public class GenerateConfig {
    public static void main(String[] args) throws IOException {
        int n = 40;
        int m = 2 * n;

        try (BufferedWriter bw = new BufferedWriter(new FileWriter("config.txt"))) {
            bw.write(Integer.toString(n));
            bw.newLine();
            bw.write(Integer.toString(m));
            bw.newLine();

            for (int u = 0; u < n; u++) {
                int v1 = (u + 2) % n; // misleading edge
                int v2 = (u + 1) % n; // Hamiltonian cycle edge

                bw.write(u + " " + v1);
                bw.newLine();
                bw.write(u + " " + v2);
                bw.newLine();
            }
        }
        System.out.println("config.txt generated.");
    }
}