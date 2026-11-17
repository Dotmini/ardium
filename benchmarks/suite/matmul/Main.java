public class Main {
    public static void main(String[] args) {
        int n = args.length > 0 ? Integer.parseInt(args[0]) : 400;
        double[][] a = new double[n][];
        double[][] b = new double[n][];
        for (int i = 0; i < n; i++) {
            double[] ra = new double[n];
            double[] rb = new double[n];
            for (int j = 0; j < n; j++) {
                ra[j] = (i * j) % 7 + 1;
                rb[j] = (i + j) % 5 + 1;
            }
            a[i] = ra;
            b[i] = rb;
        }
        double[][] c = new double[n][];
        for (int i = 0; i < n; i++) {
            double[] row = new double[n];
            double[] ai = a[i];
            for (int k = 0; k < n; k++) {
                double aik = ai[k];
                double[] bk = b[k];
                for (int j = 0; j < n; j++) row[j] += aik * bk[j];
            }
            c[i] = row;
        }
        double total = 0.0;
        for (int i = 0; i < n; i++) {
            double[] ci = c[i];
            for (int j = 0; j < n; j++) total += ci[j];
        }
        System.out.println((long) total + " " + (long) c[n / 2][n / 2]);
    }
}
