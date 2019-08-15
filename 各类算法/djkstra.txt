/*
	Dijkstra算法解决已知起点，去到其他任意终点的最短路径
	核心思想就是最短路径的最优子结构性质，即一条最短路径其子路径也一定是两点间的最短路径
	如果P(i,j)={Vi....Vk..Vs...Vj}是从顶点i到j的最短路径，k和s是这条路径上的一个中间顶点，那么P(k,s)必定是从k到s的最短路径。
*/

//前置条件
int node_cnt;					//节点数量
int matrix[node_cnt][node_cnt];	//任意两个节点之间的距离矩阵
#define DIST_MAX 99999

//计算过程中需要用到的
int dist[node_cnt];				//两点间的最短距离
bool visited[node_cnt];			//记录已经被计算过的点
int pre_node[node_cnt];			//最短路径中，每一个节点的前序节点id

//计算以start为起点，去到其他节点的最短路径
void Dijkstra()
{
	int i, j, k, tmp;

	//初始化，dist数组中保存的是以start节点为起点，去到其他任意节点的直连距离
	for (i = 0; i < node_cnt; i++) {
		dist[i] = matrix[start][i];
		visited[i] = false;
		pre_node[j] = -1;
	}

	dist[start] = 0;
	visited[start] = true;

	for (i = 0; i < node_cnt; i++) {
		//这个循环是为了找出距离start节点最近的节点k
		for (j = 0; j < node_cnt; j++) {
			int min_dist = DIST_MAX;
			if (!visited[j] && dist[j] < min_dist) {
				min_dist = dist[j];
				k = j;
			}
		}

		visited[k] = true;

		//这个循环是为了找出从start节点到除节点k以外的其他节点，之间的最短路径是否需要经过k
		//如果是就修改dist里的距离值，同时记录k节点作为前序节点
		for (j = 0; j < node_cnt; j++) {
			tmp = min_dist == DIST_MAX ? DIST_MAX : min_dist + matrix[k][j];
			if (!visited[j] && tmp < dist[j]) {
				dist[j] = tmp;
				pre_node[j] = k;
			}
		}
	}
}
