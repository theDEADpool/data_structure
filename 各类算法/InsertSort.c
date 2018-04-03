/*插入排序
基本思想：
在要排序的一组数中，假定前n-1个数已经排好序，现在将第n个数插到前面的有序数列中，使得这n个数也是排好顺序的。如此反复循环，直到全部排好顺序。
平均时间复杂度：O(n2)
*/
#include<stdio.h>
void InsertSort(int arr[], int len)
{
	int tmp, i, j;
	for(i=0; i<len-1; i++)
	{
		for(j=i+1;j>0;j--)
		{
			if(arr[j]<arr[j-1])
			{
				tmp=arr[j-1];
				arr[j-1]=arr[j];
				arr[j]=tmp;
			}
			else
			{
				break;
			}
		}
	}
}

int main()
{
	int arr[]={42,20,17,13,28,14,23,15};
	int len = sizeof(arr)/sizeof(int);
	int i;
	InsertSort(arr,len);
	for(i=0; i<len; i++)
    {
        printf("arr[%d]=%d\n",i, arr[i]);
    }
	return 0;
}