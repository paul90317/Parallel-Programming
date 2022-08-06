#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

void Count_sort(int a[], int len, int np)
{
	int count, id;
	int *disp, *cnts;
	int *temp = (int *)malloc(len * sizeof(int));
#pragma omp parallel for num_threads(np) default(none) private(count) \
	shared(len) shared(a) shared(temp)
	for (int i = 0; i < len; i++)
	{
		count = 0;
		for (int j = 0; j < len; j++)
			if (a[j] < a[i])
				count++;
			else if (a[j] == a[i] && j < i)
				count++;
		temp[count] = a[i];
	}
	disp = (int *)malloc(np * sizeof(int));
	cnts = (int *)malloc(np * sizeof(int));
	disp[0] = 0;
	cnts[0] = len / np + (id < (len % np));
	for (int i = 1; i < np; i++)
	{
		cnts[i] = len / np + (id < (len % np));
		disp[i] = disp[i - 1] + cnts[i - 1];
	}
#pragma omp parallel num_threads(np) default(none) \
	shared(a) shared(temp) shared(len) private(id) shared(cnts) shared(disp)
	{
		id = omp_get_thread_num();
		memcpy(a + disp[id], temp + disp[id], cnts[id] * sizeof(int));
	}
	free(disp);
	free(cnts);
	free(temp);
}

int cmp(const void *a, const void *b)
{
	return *(int *)a > *(int *)b;
}

int main(int argc, char *argv[])
{
	int len = 10000, np = 4, isquick = 0;
	for (int i = 0; i < argc; i++)
	{
		if (argv[i][0] == '-' && i + 1 < argc)
		{
			switch (argv[i][1])
			{
			case 'l':
				len = strtol(argv[++i], NULL, 10);
				break;
			case 'n':
				np = strtol(argv[++i], NULL, 10);
				break;
			case 'q':
				isquick = 1;
				break;
			}
		}
	}
	srand(1);
	int *a = (int *)malloc(len * sizeof(int));
	for (int i = 0; i < len; i++)
	{
		a[i] = rand() % 1000;
	}

	//計時
	struct timeval begin, end;
	gettimeofday(&begin, 0);

	if (isquick)
	{
		qsort(a, len, sizeof(int), cmp);
	}
	else
	{
		Count_sort(a, len, np);
	}

	//結束計時
	gettimeofday(&end, 0);
	long seconds = end.tv_sec - begin.tv_sec;
	long microseconds = end.tv_usec - begin.tv_usec;
	double elapsed = seconds + microseconds * 1e-6;

	for (int i = 0; i < len; i++)
	{
		printf("%d ", a[i]);
	}
	printf("\n");
	printf("The execution time = %lf\n", elapsed);
}