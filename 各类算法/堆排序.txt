/*
堆排序是将一个数组抽象成一颗完全二叉树，通过调整二叉树父节点与叶子节点之间的大小关系，来达到排序的效果。
二叉堆有两种形式：
最大堆：除根节点外的任意节点，其父节点的值都要大于该节点的值。
最小堆：除根节点外的任意节点，其父节点的值都要小于该节点的值。
堆排序的时间复杂度为O(nlgn)。

除根节点以外的任意节点i：
1)其父节点序号为i/2。
2)其左叶子节点序号为2i。
3)其右叶子节点序号为2i+1。
注意：此处的i并不是数组下标，从0开始计数。而是从1开始计数，相当于数组下标+1。
*/
#include <stdio.h>
#include <string.h>

#define LEFT(i) (2 * (i + 1) - 1)
#define RIGHT(i) (2 * (i + 1))

void max_heapify(int *nums, int len, int i)
{
    int left_i = LEFT(i);
    int right_i = RIGHT(i);
    int largest_i = i;
    
    if (left_i < len && nums[left_i] > nums[i]) {
        largest_i = left_i;
    }
    
    if (right_i < len && nums[right_i] > nums[largest_i]) {
        largest_i = right_i;
    }
    
    if (largest_i != i) {
        int tmp = nums[i];
        nums[i] = nums[largest_i];
        nums[largest_i] = tmp;
        
        max_heapify(nums, len, largest_i);
    }
}

void build_max_heap(int *nums, int len) {
    // 取到最后一个节点的父节点下标
    int i = len / 2 - 1;
    
    while (i >= 0) {
        max_heapify(nums, len, i);
        i--;
    }
}

void min_heap_sort(int *nums, int len)
{    
    build_max_heap(nums, len);

    int i = len - 1;
    while(i >= 1) {
        int tmp = nums[0];
        nums[0] = nums[i];
        nums[i] = tmp;
        len--;
        max_heapify(nums, len, 0);
        i--;
    }
}

int main()
{
    int nums[] = {1, 23, 452, 2351, 241, 7647, 95652, 5534};
    int len = sizeof(nums) / sizeof(int);
    
    max_heap_sort(nums, len);
    
    int i = 0;
    for (i = 0; i < len; i++) {
        printf("%d ", nums[i]);
    }
    printf("\n");
}