/*归并排序
基本思想：参考
归并排序是建立在归并操作上的一种有效的排序算法。该算法是采用分治法的一个非常典型的应用。
首先考虑下如何将2个有序数列合并。这个非常简单，只要从比较2个数列的第一个数，谁小就先取谁，取了后就在对应数列中删除这个数。然后再进行比较，如果有数列为空，那直接将另一个数列的数据依次取出即可。
解决了上面的合并有序数列问题，再来看归并排序，其的基本思路就是将数组分成2组A，B，如果这2组组内的数据都是有序的，那么就可以很方便的将这2组数据进行排序。如何让这2组组内数据有序了？
可以将A，B组各自再分成2组。依次类推，当分出来的小组只有1个数据时，可以认为这个小组组内已经达到了有序，然后再合并相邻的2个小组就可以了。这样通过先递归的分解数列，再合并数列就完成了归并排序。
平均时间复杂度：O(NlogN)
*/
#include<stdio.h>
#include<stdlib.h>
void MergeArray(int arr[], int first, int middle, int end, int tmp[]);
void MergeSort(int arr[], int first, int last, int tmp[])
{
	if(first < last)
	{
		int middle = (first+last)/2;
		MergeSort(arr, first, middle, tmp);
		MergeSort(arr,middle, last, tmp);
		MergeArray(arr, first, middle, last, tmp);
	}
}

void MergeArray(int arr[], int first, int middle, int end, int tmp[])
{
	int i=first;
	int m=middle;
	int j=middle+1;
	int n=end;
	int k=0;
	int ii;
	printf("MergeArray\n");
	while(i<m && j<=n)
	{
		if(arr[i]<=arr[j])
		{
			tmp[k]=arr[i];
			k++;
			i++;
		}
		else
		{
			tmp[k]=arr[j];
			k++;
			j++;
		}
	}
	printf("i[%d],j[%d],k[%d]\n",i,j,k);
	while(i<=m)
	{
		tmp[k]=arr[i];
		k++;
		i++;
	}
	while(j<=n)
	{
		tmp[k]=arr[j];
		k++;
		j++;
	}
	printf("i[%d],j[%d],k[%d]\n",i,j,k);
	for(ii=0; ii<k; ii++)
	{
		arr[first+ii]=tmp[ii];
	}
}

int main()
{
	int arr[]={42,20,17,13,28,14,23,15};
	int len = sizeof(arr)/sizeof(int);
	int *tmp = (int *)malloc(2*len*sizeof(int));
	//int tmp[8] = {0};
	
	int i;
	MergeSort(arr,0,len,tmp);
	
	for(i=0; i<len; i++)
    {
        printf("arr[%d]=%d\n",i, arr[i]);
    }
    free(tmp);
    return 0;
}