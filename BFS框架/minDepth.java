public class MinDepth {

    public int minDepth(TreeNode root) {
        if(root == null) return 0;
        Queue<TreeNode> q = new LinkedList<>();
        q.offer(root);
        // root 本身就是一层，depth 初始化为 1
        int depth = 1;

        while(!q.isEmpty()) {
            int sz = q.size();
            // 将当前队列中的所有节点向四周扩散
            for (int i = 0; i < sz; i++) {
                TreeNode cur = q.poll();
                // 判断是否到达终点
                if(cur.left == null && cur.right == null)
                    return depth;
                // 将 cur 的相邻节点加入队列
                if(cur.left != null)
                    q.offer(cur.left);
                if(cur.right != null)
                    q.offer(cur.right);
            }

            // 这里增加步数
            depth++;
        }

        return depth;
    }

    public static void main(String[] args) {
        int depth = minDepth([3, 9 ,20 , null, null, 15, 7])
        System.out.println(depth)
    }
}

