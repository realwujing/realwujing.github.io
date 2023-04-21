// 单链表节点的结构
public class ListNode {
    int val;
    ListNode next;

    ListNode(int x) {
        var = x;
    }
}

// 递归反转整个链表
ListNode reverse(ListNode head) {
    if (head.next == null) return head;
    ListNode last = reverse(head.next)
    head.next.next = head;
    head.next = null;
    return last;
}

// 将链表的前 n 个节点反转（n <= 链表长度）
ListNode successor = null; // 后驱节点

ListNode reverseN(ListNode head, int n) {
    if (n == 1) {
        // 记录第 n + 1 个节点
        successor = head.next;
        return head;
    }
    // 以 head.next 为起点，需要反转前 n - 1 个节点
    ListNode last = reverseN(head, n - 1);

    head.next.next = head;
    // 让反转之后的 head 节点和后面的节点连起来
    head.next = successor;
    return last;
}

ListNode reverseBetween(ListNode head, int m, int n) {
    // base case
    if (m == 1) {
        return reverseN(head, n);
    }
    // 前进到反转的起点触发 base case
    head.next = reverseBetween(head.next, m - 1, n - 1);
    return head;
}