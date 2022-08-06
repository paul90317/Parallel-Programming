//#include "stdint.h"
#include "mpi.h"
#include <cstdlib>
#include <iostream>
#include <algorithm>

#define mymin(a,b) ((a>b)?b:a)
#define get_local_count(id) (all_count/num_procs+(id<(all_count%num_procs)))
#define mysend(data,n,id) MPI_Send(data, n, MPI_INT, id, 0, MPI_COMM_WORLD)
#define myrecv(data,n,id) MPI_Recv(data, n, MPI_INT, id, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE)

using namespace std;

void merge(int* out,int n_out,int* b,int n_b,int *c,int n_c){
    int ib=0,ic=0;
    for(int i=0;i<n_out;i++){
        if(ib==n_b){
            out[i]=c[ic++];
        }else if(ic==n_c){
            out[i]=b[ib++];
        }else if(b[ib]<c[ic]){
            out[i]=b[ib++];
        }else{
            out[i]=c[ic++];
        }
    }
}

void merge_back(int* out,int n_out,int* b,int n_b,int *c,int n_c){
    for(int i=n_out-1;i>=0;--i){
        if(n_b==0){
            out[i]=c[--n_c];
        }else if(n_c==0){
            out[i]=b[--n_b];
        }else if(b[n_b-1]>c[n_c-1]){
            out[i]=b[--n_b];
        }else{
            out[i]=c[--n_c];
        }
    }
}

void prt(int *a, int n)
{
    cout << "\nGlobal List[0]: ";
    for (int i = 0; i < n; i++)
    {
        cout << a[i] << " ";
    }
    cout << "\n";
}
void prt_local(int *glb_arr, int local_count, int *sendcounts, int num_procs)
{
    int sum = 0;
    for (int i = 0; i < num_procs; i++)
    {
        cout << "\nLocal List[" << i << "]: ";
        for (int j = 0; j < sendcounts[i]; j++)
        {
            cout << glb_arr[sum + j] << " ";
        }
        sum += sendcounts[i];
        cout << "\n";
    }
}
int main(int argc, char *argv[])
{
    int num_procs, id;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &id);
    int *recvcounts;
    int *displs;
    int *arr;
    int all_count = 0;
    int local_count;
    int sum;
    double time;
    int *result1, *result2;
    bool is_prt=true;
    srand(id+1);
    //輸入
    if (id == 0)
    {
        for(int i=0;i<argc;i++){
            if(argv[i][0]=='n'){
                is_prt=false;
                cout<<"no print mode\n";
                break;
            }
        }
        cout<<"input the list size:";
        cin >> all_count;
    }

    //把 n 廣播給其他 proc    
    MPI_Bcast(&all_count, 1, MPI_INT, 0, MPI_COMM_WORLD);
    //多餘的核心不能用
    int num_working=mymin(all_count,num_procs);

    MPI_Barrier(MPI_COMM_WORLD);
    time = MPI_Wtime();
    
    //造隨機陣列
    local_count = get_local_count(id);
    arr = (int *)malloc(sizeof(int) * local_count);
    for (int i = 0; i < local_count; i++)
    {
        arr[i] = rand() % 10000;
    }

    //計算任務量及偏移量
    recvcounts = (int *)malloc(sizeof(int) * (num_procs+2))+1;
    displs = (int *)malloc(sizeof(int) * num_procs);
    sum = 0;
    recvcounts[-1]=recvcounts[num_procs]=0;
    for (int i = 0; i < num_procs; i++)
    {
        recvcounts[i] = get_local_count(i);
        displs[i] = sum;
        sum += recvcounts[i];
    }
    
    //平行運算
    //sort local list
    if(id<num_working)
        sort(arr,arr+local_count);
    
    //合併結果
    if(id==0)
        result1=(int*)malloc(sizeof(int)*all_count);
    MPI_Gatherv(arr, local_count, MPI_INT, result1, recvcounts, displs, MPI_INT, 0, MPI_COMM_WORLD);
    
    //global 平行運算
    //有任務的處理器才能執行
    if(id<num_working){
        int* out=(int*)malloc(sizeof(int)*local_count);
        int* arr_recv=(int*)malloc(sizeof(int)*all_count);
        int partner,recv_count;
        bool have_out;

        for(int phase=0;phase<num_working;phase++){
            have_out=false;
            if(phase%2&&id%2||phase%2==0&&id%2==0){
                //傳右接右
                partner=id+1;
                recv_count=recvcounts[partner];
                if(recv_count){
                    mysend(arr,local_count,partner);
                    myrecv(arr_recv,recv_count,partner);
                    merge(out,local_count,arr,local_count,arr_recv,recv_count);
                    have_out=true;
                }
            }else{
                //接左傳左
                partner=id-1;
                recv_count=recvcounts[partner];
                if(recv_count){
                    myrecv(arr_recv,recv_count,partner);
                    mysend(arr,local_count,partner);
                    merge_back(out,local_count,arr,local_count,arr_recv,recv_count);
                    have_out=true;
                }
            }
            if(have_out)
                swap(out,arr);
        }
    }  
    //合併結果
    if(id==0)
        result2=(int*)malloc(sizeof(int)*all_count);
    MPI_Gatherv(arr, local_count, MPI_INT, result2, recvcounts, displs, MPI_INT, 0, MPI_COMM_WORLD);
    
    //計算時間
    MPI_Barrier(MPI_COMM_WORLD);
    time=MPI_Wtime()-time;

    MPI_Finalize();

    //印出結果
    if (id == 0)
    {
        if(is_prt){
            prt_local(result1, all_count, recvcounts, num_procs);
            prt(result2, all_count);
        }
        cout << "\nFinish in " << time << " secs\n";
    }
}