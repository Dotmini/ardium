public class Main {
    static final class Tree {
        final Tree left, right;
        Tree(Tree left, Tree right) { this.left = left; this.right = right; }
    }

    static Tree make(int depth) {
        if (depth == 0) return new Tree(null, null);
        return new Tree(make(depth - 1), make(depth - 1));
    }

    static int check(Tree t) {
        if (t.left != null && t.right != null) return 1 + check(t.left) + check(t.right);
        return 1;
    }

    public static void main(String[] args) {
        int maxDepth = args.length > 0 ? Integer.parseInt(args[0]) : 18;
        int minDepth = 4;
        int stretch = maxDepth + 1;
        System.out.println("stretch tree of depth " + stretch + "\t check: " + check(make(stretch)));
        Tree longLived = make(maxDepth);
        for (int d = minDepth; d <= maxDepth; d += 2) {
            int iterations = 1 << (maxDepth - d + minDepth);
            long total = 0;
            for (int i = 0; i < iterations; i++) total += check(make(d));
            System.out.println(iterations + "\t trees of depth " + d + "\t check: " + total);
        }
        System.out.println("long lived tree of depth " + maxDepth + "\t check: " + check(longLived));
    }
}
