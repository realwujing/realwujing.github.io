import java.util.*;

public class HouseRobber {
    // House Robber I
    // 主函数
    public int rob(int[] nums) {
        return dp(nums, 0);
    }

    // 返回 nums[start..] 能抢到的最大值
    private int dp(int[] nums, int start) {
        if (start >= nums.length) 
            return 0;

        int res = Math.max(
            // 不抢，去下家
            dp(nums, start + 1),
            // 抢，去下下家
            nums[start] + dp(nums, start + 2)
        );
        return res;
    }

    // 备忘录优化
    private int[] memo;
    // 主函数
    public int rob2(int[] nums) {
        // 初始化备忘录
        memo = new int[nums.length];
        Arrays.fill(memo, -1);
        // 强盗从第 0 间房子开启抢劫
        return dp2(nums, 0);
    }

    // 返回 dp2[start..] 能抢到的最大值
    private int dp2(int[] nums, int start) {
        if (start >= nums.length)  return 0;
        // 避免重复计算
        if(memo[start] != -1) return memo[start];
        int res = Math.max(dp2(nums, start + 1), nums[start] + dp(nums, start + 2));
        // 计入备忘录
        memo[start] = res;
        return res;
    }

    // 自底向上
    public int rob3(int[] nums) {
        int n = nums.length;
        /*
        dp[i] = x 表示：从第 i 间房子开始抢劫，最多能抢到的钱为 x
        base case: dp[n] = 0
        */
        int[] dp = new int[n + 2];
        for (int i = n - 1; i >= 0; i--) {
            dp[i] = Math.max(dp[i + 1], nums[i] + dp[i + 2]);
        }
        return dp[0];
    }

    // 将空降复杂度降低到 O(1)
    int rob4(int[] nums) {
        int n = nums.length;
        // 记录 dp[i+1] 和 dp[i+2]
        int dp_i_1 = 0, dp_i_2 = 0;
        // 记录 dp[i]
        int dp_i = 0;
        for (int i = n - 1; i >= 0; i--) {
            System.out.println("dp_i:" + dp_i + " dp_i_1:" + dp_i_1 + " dp_i_2:" + dp_i_2);
            dp_i = Math.max(dp_i_1, nums[i] + dp_i_2);
            dp_i_2 = dp_i_1;
            dp_i_1 = dp_i;
            System.out.println("dp_i:" + dp_i + " dp_i_1:" + dp_i_1 + " dp_i_2:" + dp_i_2);
            System.out.println("*******************************************");
        }
        return dp_i;
    }


    // House Robber II
    public int rob5(int[] nums) {
        int n = nums.length;
        if (n == 1) return nums[0];
        return Math.max(robRange(nums, 0, n -2), robRange(nums, 1, n - 1));
    }

    // 仅计算闭区间 [start, end] 的最优结果
    private int robRange(int[] nums, int start, int end) {
        int n = nums.length;
        int dp_i_1 = 0, dp_i_2 = 0;
        int dp_i = 0;
        for (int i = end; i >= start; i--) {
            dp_i = Math.max(dp_i_1, nums[i] + dp_i_2);
            dp_i_2 = dp_i_1;
            dp_i_1 = dp_i;
        }
        return dp_i;
    }

    // House Robber III
    Map<TreeNode, Integer> memo = new HashMap<>();
    public int rob6(TreeNode root) {
        if (root == null) return 0;
        // 利用备忘录消除重叠子问题
        if (memo.containsKey(root)) 
            return memo.get(root);
        // 抢，然后去下下家
        int do_it = root.val
            + (root.left == null ? 
                0 : rob6(root.left.left) + rob6(root.left.right))
            + (root.right == null ? 
                0 : rob6(root.right.left) + rob6(root.right.right));
        // 不抢，然后去下家
        int not_do = rob6(root.left) + rob6(root.right);

        int res = Math.max(do_it, not_do);
        memo.put(root, res);
        return res;
    }

    public static void main(String[] args) {
        HouseRobber houseRobber = new HouseRobber();
        // int[] nums = {1, 2, 3, 1};
        // System.out.println(houseRobber.rob(nums));
        // System.out.println(houseRobber.rob2(nums));
        // System.out.println(houseRobber.rob3(nums));
        // System.out.println(houseRobber.rob4(nums));

        int[] nums1 = {2, 3, 2};
        System.out.println(houseRobber.rob5(nums1));
    }
    
}
