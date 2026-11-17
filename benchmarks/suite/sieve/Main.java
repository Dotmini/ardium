public class Main {
    public static void main(String[] args) {
        int n = args.length > 0 ? Integer.parseInt(args[0]) : 50000000;
        boolean[] sieve = new boolean[n + 1];
        java.util.Arrays.fill(sieve, true);
        sieve[0] = false;
        sieve[1] = false;
        for (long i = 2; i * i <= n; i++) {
            if (sieve[(int) i]) {
                for (long j = i * i; j <= n; j += i) sieve[(int) j] = false;
            }
        }
        int count = 0;
        for (int k = 0; k <= n; k++) if (sieve[k]) count++;
        System.out.println(count);
    }
}
