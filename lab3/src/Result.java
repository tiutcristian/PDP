public class Result {
    public final double[][] C;
    public final long nanos;

    public Result(double[][] C, long nanos) {
        this.C = C;
        this.nanos = nanos;
    }
}