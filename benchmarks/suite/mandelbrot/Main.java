public class Main {
    public static void main(String[] args) {
        int size = args.length > 0 ? Integer.parseInt(args[0]) : 2000;
        int maxIter = 100;
        long inside = 0;
        for (int y = 0; y < size; y++) {
            double ci = -1.5 + 3.0 * y / size;
            for (int x = 0; x < size; x++) {
                double cr = -2.0 + 3.0 * x / size;
                double zr = 0.0, zi = 0.0;
                boolean escaped = false;
                for (int i = 0; i < maxIter; i++) {
                    double zr2 = zr * zr;
                    double zi2 = zi * zi;
                    if (zr2 + zi2 > 4.0) {
                        escaped = true;
                        break;
                    }
                    zi = 2.0 * zr * zi + ci;
                    zr = zr2 - zi2 + cr;
                }
                if (!escaped) inside++;
            }
        }
        System.out.println(inside);
    }
}
