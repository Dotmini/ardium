import java.util.HashMap;
import java.util.Map;

public class Main {
    public static void main(String[] args) {
        int n = args.length > 0 ? Integer.parseInt(args[0]) : 3000000;
        HashMap<String, Integer> counts = new HashMap<>();
        long x = 12345;
        for (int i = 0; i < n; i++) {
            x = (x * 1103515245L + 12345) % 2147483648L;
            String key = "w" + (x % 50000);
            counts.merge(key, 1, Integer::sum);
        }
        int best = 0;
        String bestKey = "";
        for (Map.Entry<String, Integer> e : counts.entrySet()) {
            int v = e.getValue();
            String k = e.getKey();
            if (v > best || (v == best && k.compareTo(bestKey) < 0)) {
                best = v;
                bestKey = k;
            }
        }
        System.out.println(counts.size() + " " + bestKey + " " + best);
    }
}
