import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

public class Main {
    static final String[] CITIES = {"Bangkok", "ChiangMai", "Phuket", "KhonKaen", "HatYai", "Pattaya", "UdonThani", "Korat", "HuaHin", "Ayutthaya"};

    public static void main(String[] args) {
        int n = args.length > 0 ? Integer.parseInt(args[0]) : 1000000;
        ArrayList<String> parts = new ArrayList<>();
        long x = 7;
        for (int i = 0; i < n; i++) {
            x = (x * 1103515245L + 12345) % 2147483648L;
            String city = CITIES[(int) ((x / 1000) % 10)];
            parts.add("user" + (x % 1000) + "," + city + "," + ((x / 10) % 10000));
        }
        String data = String.join("\n", parts);
        HashMap<String, Long> totals = new HashMap<>();
        HashMap<String, Long> users = new HashMap<>();
        long grand = 0;
        String[] lines = data.split("\n");
        for (String line : lines) {
            String[] f = line.split(",");
            long amount = Long.parseLong(f[2]);
            totals.merge(f[1], amount, Long::sum);
            users.merge(f[0], 1L, Long::sum);
            grand += amount;
        }
        String best = "";
        long bestTotal = -1;
        for (Map.Entry<String, Long> e : totals.entrySet()) {
            long t = e.getValue();
            String city = e.getKey();
            if (t > bestTotal || (t == bestTotal && city.compareTo(best) < 0)) {
                best = city;
                bestTotal = t;
            }
        }
        System.out.println(lines.length + " " + users.size() + " " + best + " " + bestTotal + " " + grand);
    }
}
