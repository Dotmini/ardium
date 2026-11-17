public class Main {
    public static void main(String[] args) {
        int n = args.length > 0 ? Integer.parseInt(args[0]) : 10;
        int[] perm1 = new int[n];
        for (int i = 0; i < n; i++) perm1[i] = i;
        int[] perm = new int[n];
        int[] count = new int[n];
        int maxFlips = 0, checksum = 0, permCount = 0;
        int r = n;
        while (true) {
            while (r != 1) {
                count[r - 1] = r;
                r--;
            }
            System.arraycopy(perm1, 0, perm, 0, n);
            int flips = 0;
            int k = perm[0];
            while (k != 0) {
                for (int i = 0, j = k; i < j; i++, j--) {
                    int t = perm[i];
                    perm[i] = perm[j];
                    perm[j] = t;
                }
                flips++;
                k = perm[0];
            }
            if (flips > maxFlips) maxFlips = flips;
            if (permCount % 2 == 0) checksum += flips; else checksum -= flips;
            boolean done = false;
            while (true) {
                if (r == n) {
                    done = true;
                    break;
                }
                int p0 = perm1[0];
                for (int i = 0; i < r; i++) perm1[i] = perm1[i + 1];
                perm1[r] = p0;
                count[r]--;
                if (count[r] > 0) break;
                r++;
            }
            if (done) break;
            permCount++;
        }
        System.out.println(checksum);
        System.out.println("Pfannkuchen(" + n + ") = " + maxFlips);
    }
}
