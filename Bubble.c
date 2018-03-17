/*冒泡排序
基本思想：两个数比较大小，较大的数下沉，较小的数冒起来
过程：
比较相邻的两个数据，如果第二个数小，就交换位置。
从后向前两两比较，一直到比较最前两个数据。最终最小数被交换到起始的位置，这样第一个最小数的位置就排好了。
继续重复上述过程，依次将第2.3...n-1个最小数排好位置。
平均时间复杂度：O(n2)
*/
#include<stdio.h>
void BubbleSort(int arr[], int num)
{
    if(arr == NULL)
        return;
    int i,j,tmp;
    for(i=0; i<num-1; i++)
    {
        for(j=0; j<num-i-1; j++)
        {
            if(arr[j]<arr[j+1])
            {
                printf("change %d and %d\n", arr[j], arr[j+1]);
                tmp=arr[j];
                arr[j]=arr[j+1];
                arr[j+1]=tmp;
            }
        }
    }
}

int main()
{
    int i;
    int arr[7]={2,7,4,6,3,1,9};
    BubbleSort(arr, sizeof(arr)/sizeof(int));
    printf("length of arr is %ld\n", sizeof(arr)/sizeof(int));
    for(i=0; i<sizeof(arr)/sizeof(int); i++)
    {
        printf("arr[%d]=%d\n",i, arr[i]);
    }
    return 0;
}
