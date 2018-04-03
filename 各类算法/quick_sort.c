#include<stdio.h>

static int
_Patition(int *num, int start, int end) {
    int i, j, tmp;

	i = start - 1;
    for (j = start; j < end; j++) {
        if (num[j] <= num[end]) {
            i++;
            tmp = num[i];
            num[i] = num[j];
            num[j] = tmp;
        }
    }
    
    tmp = num[i + 1];
    num[i + 1] = num[end];
    num[end] = tmp;
    
    return i + 1;
}

void
QuickSort(int *num, int start, int end) {
    if (start < end) {
        start = _Patition(num, start, end);
		QuickSort(num, 0, start - 1);
		QuickSort(num, start + 1, end);
    }
}

#if 0
int main()
{
    int num[] = {11, 24, 576, 42, 875, 45, 834, 21};
    int len = sizeof(num) / sizeof(int);
    
    QuickSort(num, 0, len - 1);
    
    int i;
    for (i = 0; i < len; i++) {
        printf("%d ", num[i]);
    }
    
    printf("\n");

    return 0;
}
#endif
