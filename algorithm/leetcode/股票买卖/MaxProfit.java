// 一个方法团灭 LeetCode 股票买卖问题
public class MaxProfit {

    // 121. 买卖股票的最佳时机
    public static int maxProfit(int[] prices) {
        int n = prices.length;
        int[][] dp = new int[n][2];
        for (int i = 0; i < n; i++) {
            if (i - 1 == -1) {
                dp[i][0] = 0;
                dp[i][1] = -prices[i];
                continue;
            }
            dp[i][0] = Math.max(dp[i-1][0], dp[i-1][1] + prices[i]);
            dp[i][1] = Math.max(dp[i-1][1], -prices[i]);
        }
        return dp[n-1][0];
    }

    // 121. 买卖股票的最佳时机 把空间复杂度降到 O(1)
    // k = 1
    public static int maxProfit_k_1(int[] prices) {
        int n = prices.length;
        // base case: dp[-1][0] = 0, dp[-1][1] = -infinity
        int dp_i_0 = 0, dp_i_1 = Integer.MIN_VALUE;
        for (int i = 0; i < n; i++) {
            // dp[i][0] = max(dp[i-1][0], dp[i-1][1] + prices[i])
            dp_i_0 = Math.max(dp_i_0, dp_i_1 + prices[i]);
            // dp[i][1] = max(dp[i-1][1], -prices[i])
            dp_i_1 = Math.max(dp_i_1, -prices[i]);    
        }
        return dp_i_0;
    }

    // 122. 买卖股票的最佳时机 II
    public static int maxProfit_k_inf(int[] prices) {
        int n = prices.length;
        int dp_i_0 = 0, dp_i_1 = Integer.MIN_VALUE;
        for (int i = 0; i < n; i++) {
            int temp = dp_i_0;
            dp_i_0 = Math.max(dp_i_0, dp_i_1 + prices[i]);
            dp_i_1 = Math.max(dp_i_1, temp - prices[i]);
        }
        return dp_i_0;
    }

    // 123. 买卖股票的最佳时机 III
    /*
    dp[i][2][0] = max(dp[i-1][2][0], dp[i-1][2][1] + prices[i])
    dp[i][2][1] = max(dp[i-1][2][1], dp[i-1][1][0] - prices[i])
    dp[i][1][0] = max(dp[i-1][1][0], dp[i-1][1][1] + prices[i])
    dp[i][1][1] = max(dp[i-1][1][1], -prices[i])
    */

    public static int maxProfit_k_2(int[] prices) {
        int dp_i10 = 0, dp_i11 = Integer.MIN_VALUE;
        int dp_i20 = 0, dp_i21 = Integer.MIN_VALUE;
        for (int price : prices) {
            dp_i20 = Math.max(dp_i20, dp_i21 + price);
            dp_i21 = Math.max(dp_i21, dp_i10 - price);
            dp_i10 = Math.max(dp_i10, dp_i11 + price);
            dp_i11 = Math.max(dp_i11, -price);
        }
        return dp_i20;
    }

    // 309. 最佳买卖股票时机含冷冻期
    public static int maxProfit_with_cool(int[] prices) {
        int n = prices.length;
        int dp_i_0 = 0, dp_i_1 = Integer.MIN_VALUE;
        int dp_pre_0 = 0; // 代表 dp[i-2][0]
        for (int i = 0; i < n; i++) {
            int temp = dp_i_0;
            dp_i_0 = Math.max(dp_i_0, dp_i_1 + prices[i]);
            dp_i_1 = Math.max(dp_i_1, dp_pre_0 - prices[i]);
            dp_pre_0 = temp;
        }
        return dp_i_0;
    }

    // 714. 买卖股票的最佳时机含手续费
    public static int maxProfit_with_fee(int[] prices, int fee) {
        int n = prices.length;
        int dp_i_0 = 0, dp_i_1 = Integer.MIN_VALUE;
        for (int i = 0; i < n; i++) {
            int temp = dp_i_0;
            dp_i_0 = Math.max(dp_i_0, dp_i_1 + prices[i]);
            dp_i_1 = Math.max(dp_i_1, temp - prices[i] - fee);
        }
        return dp_i_0;
    }

    // 188. 买卖股票的最佳时机 IV
    public static int maxProfit_k_any(int max_k, int[] prices) {
        int n = prices.length;
        if (max_k > n / 2) 
            return maxProfit_k_inf(prices);

        int[][][] dp = new int[n][max_k + 1][2];
        for (int i = 0; i < n; i++) 
            for (int k = max_k; k >= 1; k--) {
                if (i - 1 == -1) { /* 处理 base case */ }
                dp[i][k][0] = Math.max(dp[i-1][k][0], dp[i-1][k][1] + prices[i]);
                dp[i][k][1] = Math.max(dp[i-1][k][1], dp[i-1][k-1][0] - prices[i]);     
            }
        return dp[n - 1][max_k][0];
    }

    public static void main(String[] args) {
        int[] prices = {7,1,5,3,6,4};
        int fee = 2;
        int k = 4;
        System.out.println(maxProfit(prices));
        System.out.println(maxProfit_k_1(prices));
        System.out.println(maxProfit_k_inf(prices));
        System.out.println(maxProfit_k_2(prices));
        System.out.println(maxProfit_with_cool(prices));
        System.out.println(maxProfit_with_fee(prices, fee));
        System.out.println(maxProfit_k_any(k, prices));
    }
}


