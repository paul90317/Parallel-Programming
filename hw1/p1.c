#include <stdio.h>  // printf()
#include <limits.h> // UINT_MAX
#include "mpi.h"

int checkCircuit(int, int);
int My_Sum(int id, int val, int n)
{
    //初始 mask(判斷發送接收) 為 id，partner 位移是 1(移動1個 id 即可找到 partner)
    int mask = id, partner_x = 1;
    int t;
    while (1)
    {
        if (mask % 2)
        {
            //當 mask 為奇數時回傳結果給 partner ，結束。
            MPI_Send(&val, 1, MPI_INT, id - partner_x, 0, MPI_COMM_WORLD);
            break;
        }
        else if (id + partner_x < n)
        {
            //當 mask 為偶數時，且 partner 存在，接收並加總騎回傳值。
            //不存在 partner 時則跳過。
            MPI_Recv(&t, 1, MPI_INT, id + partner_x, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            val += t;
        }
        else if (id == 0)
        {
            //當是 proc0 沒有 partner 時，回傳加總結果。
            break;
        }
        mask >>= 1;      //mask 除2
        partner_x <<= 1; //partner位移 乘2
    }
    return val;
}

int main(int argc, char *argv[])
{
    //MPI 參數設定
    int num_procs, id;
    int i, all_test = 0;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &id);

    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] == 'a')
        {
            all_test = 1;
        }
    }

    int count = 0, all_count;
    double startTime = 0.0, totalTime = 0.0, taskTime = 0.0, communicationTime = 0.0;

    //平行運算起點
    if (all_test)
    {
        MPI_Barrier(MPI_COMM_WORLD);
        startTime = MPI_Wtime();
        for (i = id; i <= USHRT_MAX; i += num_procs)
        {
            count += checkCircuit(id, i);
        }
        taskTime = MPI_Wtime() - startTime;
        MPI_Barrier(MPI_COMM_WORLD);
    }
    else
    {
        if (id == 0)
            startTime = MPI_Wtime();
        for (i = id; i <= USHRT_MAX; i += num_procs)
        {
            count += checkCircuit(id, i);
        }
    }

    //communicaton
    if (all_test && id == 0)
    {
        communicationTime = MPI_Wtime();
        all_count = My_Sum(id, count, num_procs);
        communicationTime = MPI_Wtime() - communicationTime;
    }
    else
    {
        all_count = My_Sum(id, count, num_procs);
    }

    //end time 輸出
    if (id == 0)
    {
        totalTime = MPI_Wtime() - startTime;
        if (all_test)
        {
            printf("Task %d finished in time %f secs.\n", id, taskTime);
            printf("Communication finished in time %f secs.\n", communicationTime);
        }
        printf("Process %d finished in time %f secs.\n", id, totalTime);
        printf("\nA total of %d solutions were found.\n\n", all_count);
    }
    else if (all_test)
    {
        printf("Task %d finished in time %f secs.\n", id, taskTime);
    }

    MPI_Finalize();
    return 0;
}

#define EXTRACT_BIT(n, i) ((n & (1 << i)) ? 1 : 0)

#define SIZE 16

int checkCircuit(int id, int bits)
{
    int v[SIZE]; /* Each element is a bit of bits */
    int i;

    for (i = 0; i < SIZE; i++)
        v[i] = EXTRACT_BIT(bits, i);

    if ((v[0] || v[1]) && (!v[1] || !v[3]) && (v[2] || v[3]) && (!v[3] || !v[4]) && (v[4] || !v[5]) && (v[5] || !v[6]) && (v[5] || v[6]) && (v[6] || !v[15]) && (v[7] || !v[8]) && (!v[7] || !v[13]) && (v[8] || v[9]) && (v[8] || !v[9]) && (!v[9] || !v[10]) && (v[9] || v[11]) && (v[10] || v[11]) && (v[12] || v[13]) && (v[13] || !v[14]) && (v[14] || v[15]))
    {
        /*printf("%d) %d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d \n", id,
               v[15], v[14], v[13], v[12],
               v[11], v[10], v[9], v[8], v[7], v[6], v[5], v[4], v[3], v[2], v[1], v[0]);
        fflush(stdout);*/
        return 1;
    }
    else
    {
        return 0;
    }
}
