#include <stdio.h>	// printf()
#include <stdlib.h> // strtoll()
#include "mpi.h"

#define PI25DT 3.141592653589793238462643
#define MY_RMAX 10000

long long int My_Sum(int id, long long int val, int n)
{
	int mask = id, partner_x = 1;
	long long int t;
	while (1)
	{
		if (mask % 2)
		{
			MPI_Send(&val, 1, MPI_LONG_LONG, id - partner_x, 0, MPI_COMM_WORLD);
			break;
		}
		else if (id + partner_x < n)
		{
			MPI_Recv(&t, 1, MPI_LONG_LONG, id + partner_x, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			val += t;
		}
		else if (id == 0)
		{
			break;
		}
		mask >>= 1;
		partner_x <<= 1;
	}
	return val;
}
long long int My_Bcast(int id, long long int val, int n)
{
	int dis = 0;
	int tmp = id; //算對數用的
	if (id)
	{
		while (tmp /= 2)
			dis++;		//dis=log(id);
		dis = 1 << dis; //dis=2^(log(id))
		//接受 partner 的 val (只收一次)
		MPI_Recv(&val, 1, MPI_LONG_LONG, id - dis, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		dis *= 2;
	}
	else
	{
		//由此 id==0 開始傳播
		dis = 1;
	}

	while ((id + dis) < n)
	{
		MPI_Send(&val, 1, MPI_LONG_LONG, id + dis, 0, MPI_COMM_WORLD);
		dis *= 2; //下一層 partner
	}
	return val;
}
inline long long int sample()
{
	int x = rand() % MY_RMAX;
	int y = rand() % MY_RMAX;
	if (x * x + y * y < MY_RMAX * MY_RMAX)
	{
		return 1;
	}
	return 0;
}
int main(int argc, char *argv[])
{
	//MPI 設定
	int num_procs, id;
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
	MPI_Comm_rank(MPI_COMM_WORLD, &id);

	long long int sum = 0, num_tossed = 1000000, all_sum, i;
	int all_test = 0;
	double startTime = 0.0, totalTime = 0.0, taskTime = 0.0, communicationTime = 0.0;
	if (id == 0)
	{
		for (i = 1; i < argc; i++)
		{
			if (argv[i][0] == 'a')
			{
				all_test = 1;
			}
			else
			{
				num_tossed = strtoll(argv[i], NULL, 10);
			}
		}
	}
	//開始時間
	MPI_Barrier(MPI_COMM_WORLD);
	startTime = MPI_Wtime();

	//亂數種
	srand(id + 2);

	//廣播 number of tossed
	num_tossed = My_Bcast(id, num_tossed, num_procs);
	all_test = My_Bcast(id, all_test, num_procs);

	//平行運算
	if (all_test)
	{
		for (i = id; i < num_tossed; i += num_procs)
		{
			sum += sample();
		}

		//每個 proc 個自平行運算結束時間
		taskTime = MPI_Wtime() - startTime;
		MPI_Barrier(MPI_COMM_WORLD);
	}
	else
	{
		for (i = id; i < num_tossed; i += num_procs)
		{
			sum += sample();
		}
	}

	//communication
	if (all_test && id == 0)
	{
		communicationTime = MPI_Wtime();
		all_sum = My_Sum(id, sum, num_procs);
		communicationTime = MPI_Wtime() - communicationTime;
	}
	else
	{
		all_sum = My_Sum(id, sum, num_procs);
	}

	//end time 輸出
	if (id == 0)
	{
		totalTime = MPI_Wtime() - startTime;
		float pi = 4.0 * all_sum / num_tossed;
		if (all_test)
		{
			printf("Task %d finished in time %f secs.\n", id, taskTime);
			printf("Communication finished in time %f secs.\n", id, communicationTime);
		}
		printf("pi is approximately %.16f, Error is %.16f\n", pi, pi - PI25DT);
		printf("wall clock time = %f\n", totalTime);
	}
	else if (all_test)
	{
		printf("Task %d finished in time %f secs.\n", id, taskTime);
	}

	MPI_Finalize();
	return 0;
}