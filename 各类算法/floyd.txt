#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void floyd(const int n, unsigned int dist[n][n], unsigned int path[n][n])
{
	int i, j, k;
	for (k = 0; k < n; k++) {
		for (i = 0; i < n; i++) {
			for (j = 0; j < n; j++) {
				if (dist[i][j] > dist[i][k] + dist[k][j]) {
					dist[i][j] = dist[i][k] + dist[k][j];
					path[i][j] = k + 1;
				}
			}
		}
	}
}

unsigned int combine_path(const int n, unsigned int path[n][n], int i, int j)
{
	unsigned int mid = path[i][j];
	unsigned int mid_tmp = 0;
	if (0 != mid) {
		mid_tmp = combine_path(n , path, i, mid - 1);
		if (0 != mid_tmp)
			printf("%u ", mid_tmp);
		printf("%u ", mid);
		mid_tmp = combine_path(n, path, mid - 1, j);
		if (0 != mid_tmp)
			printf("%u ", mid_tmp);
	}
}

void print_path(const int n, unsigned int path[n][n])
{
	int i, j;
	unsigned int id = 0;

	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			if (i == j) {
				continue;
			}
			printf("%d->%d: %d ", i + 1, j + 1, i + 1);
			combine_path(n, path, i, j);
			printf("%d\n", j + 1);
		}
	}
}

int main()
{
	unsigned int dist[4][4];
	unsigned int path[4][4];
	int i, j, tmp;

	srand(time(NULL));
	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			path[i][j] = 0;
			if (i == j) {
				dist[i][j] = 0;
				continue;
			}

			tmp = rand() % 20;
			if (0 == tmp || 10 == tmp) {
				dist[i][j] = 99999;
			} else {
				dist[i][j] = tmp;
			}
		}
	}

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			printf("%10u ", dist[i][j]);
		}
		printf("\n");
	}

	floyd(4, dist, path);

	printf("after floyd\n");

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			printf("%10u ", dist[i][j]);
		}
		printf("\n");
	}	

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			printf("%u ", path[i][j]);
		}
		printf("\n");
	}

	print_path(4, path);
	return 0;
}
