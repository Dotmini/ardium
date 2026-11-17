import java.util.Arrays;

public class Main {
    public static void main(String[] args) {
        int n = args.length > 0 ? Integer.parseInt(args[0]) : 5000000;
        long[] xs = new long[n];
        long x = 42;
        for (int i = 0; i < n; i++) {
            x = (x * 1103515245L + 12345) % 2147483648L;
            xs[i] = x % 1000000000L;
        }
        Arrays.sort(xs);
        long checksum = 0;
        for (int i = 0; i < n; i++) checksum = (checksum * 31 + xs[i]) % 1000000007L;
        System.out.println(xs[0] + " " + xs[n / 2] + " " + xs[n - 1] + " " + checksum);
    }
}
