#include <mpi.h>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cmath>
#include <omp.h>

using namespace std;

int** d;//distance
int NC=80; //calculate times
int M=150; //#ant
float alpha=1.1f; //for pheromone
float beta2=0.8f; //for w
float Q=36.0f;// pheromone per ant
int N;//#city
float evap=0.23f;//evaporation rate


inline bool check_not_end(char* p){
    int i=0;
    while(p[i]){
        if(p[i]>='0'&&p[i]<='9'){
            return true;
        }
        i++;
    }
    return false;
    
}

void updateP(vector<vector<int>> &P,vector<vector<float>> &pheromone){//將費洛蒙矩陣、距離矩陣組合成機率矩陣
    for(int i=0;i<N;i++){
        vector<float> Pt(N,0.0f);
        float Psum=0.0f;
        for(int j=0;j<N;j++){
            if(d[i][j]==0){
                continue;
            }
            Pt[j]=pow(pheromone[i][j],alpha)*pow(1.0f/(float)d[i][j],beta2);
            Psum+=Pt[j];
        }
        if(Psum==0){
            int kk=10000/N;
            for(int j=0;j<N;j++){
                P[i][j]=kk;
            }
            continue;
        }
        Psum=10000.0f/Psum;//want to equally divided P to 10000
        for(int j=0;j<N;j++){
            P[i][j]=Pt[j]*Psum;
        }
    }
    return;
}

int random_city(vector<int> &P,vector<bool> &Sp){//從對應機率挑選一個還沒被走過的城市
    int Pall=0;//sum of valide city's probablity
    int validC=0;//#valid city we can choose
    for(int i=0;i<N;i++){
        if(Sp[i]){
            validC++;
            Pall+=P[i];
        }
    }
    
    if(Pall==0){//若機率值太小=0,那就隨機挑一個
        int Pr=rand()%validC;
        for(int i=0;i<N;i++){
            if(Sp[i]){
                Pr--;
                if(Pr<=0)return i;
            }
        }
        return N-1;//no use
    }
    int Pr=rand()%Pall;
    for(int i=0;i<N;i++){
        if(Sp[i]){
            Pr-=P[i];
            if(Pr<=0)return i;
        }
    }
    return N-1;//no use
}

template<typename T>
T* array(int n){
    return (T*)malloc(sizeof(T)*n);
}

template<typename T>
T** matrix(int r,int c){
    T** ret=(T**)malloc(sizeof(T*)*r);
    T* vals=(T*)malloc(sizeof(T)*r*c);
    for(int i=0;i<r;i++){
        ret[i]=vals+c*i;
    }
    return ret;
}

void get_distance(char* fname){
    fstream fin;
    char buf[10000];
    fin.open(fname,ios::in);
    char* p;
    vector<vector<int>> dt;
    while(fin.getline(buf,sizeof(buf))){
        p=buf;
        if(check_not_end(p)){
            dt.push_back(vector<int>()); 
        }
        while(check_not_end(p)){
            dt.back().push_back(strtol(p,&p,10));
        }
    }
    fin.close();
    N=dt.size();//#city
    d=matrix<int>(N,N);
    for(int i=0;i<N;i++){
        for(int j=0;j<N;j++){
            d[i][j]=dt[i][j];
        }
    }
}

int main(int argc,char* argv[]){
    //mpi init
    int num_procs, mpi_id;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_id);
    char processor_name[100];
    int namelen;
    MPI_Get_processor_name(processor_name, &namelen);
    cout<<processor_name<<"\n";
    //read argv
    char* fname=NULL;
    for(int i=0;i<argc;i++){
        if(argv[i][0]=='-'&&i<argc-1){
            switch(argv[i][1]){
            case 'r':
                NC=strtol(argv[++i],NULL,10);
                break;
            case 'a':
                alpha=strtof(argv[++i],NULL);
                break;
            case 'b':
                beta2=strtof(argv[++i],NULL);
                break;
            case 'e':
                evap=strtof(argv[++i],NULL);
                break;
            case 'm':
                M=strtol(argv[++i],NULL,10);
                break;
            }
        }else{
            fname=argv[i];
        }
    }
    if(fname==NULL){
        cout<<"Please input filename in argv.\n";
        return 0;
    }

    //get distance(d)
    if(mpi_id==0){
        get_distance(fname);
    }
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if(mpi_id){
        d=matrix<int>(N,N);
    }
    MPI_Bcast(d[0], N*N, MPI_INT, 0, MPI_COMM_WORLD);
    
    //init global
    int* Tglobal=array<int>(N+1);
    int Lglobal=10000000;
    float** global_pheromone=matrix<float>(N,N);//pheromone
    for(int i=0;i<N;i++){
        for(int j=0;j<N;j++){
            global_pheromone[i][j]=1;
        }
    }
    float** tmp=matrix<float>(N,N);//for avg pheromone
    srand(mpi_id+2);

    //P calculate
#pragma omp parallel num_threads(4) default(none)\
 shared(Tglobal,Lglobal,d,N,M,evap,NC,mpi_id,cout,Q,global_pheromone,tmp,num_procs)
    {
        vector<vector<int>> P(N,vector<int>(N));//probability
        int Lbest=10000000;//min length of cycle
        int* Tbest=array<int>(N+1);//the cycle have min length
        int** T=matrix<int>(M,N+1);//cycle per ant
        vector<int> L(M);//L per ant
        int tid = omp_get_thread_num();
        vector<vector<float>> pheromone(N,vector<float>(N,1));
        for(int t=0;t<NC;t++){
            updateP(P,pheromone);
            for(int k=0;k<M;k++){// per ant
                vector<bool> Sp(N,true);//city un visted per ant
                int Ck=rand()%N;//which city have the ant start
                T[k][0]=Ck;
                L[k]=0;
                Sp[Ck]=false;
                for(int i=1;i<N;i++){// for #city
                    Ck=random_city(P[Ck],Sp);
                    Sp[Ck]=false;
                    L[k]+=d[T[k][i-1]][Ck];
                    T[k][i]=Ck;
                }
                T[k][N]=T[k][0];
                L[k]+=d[T[k][N-1]][T[k][N]];
                //updata Best
                if(L[k]<Lbest){
                    Lbest=L[k];
                    memcpy(Tbest,T[k],sizeof(int)*(N+1));
                }
            }
            //you got Lk for each ant
            //updata pheromone below
            for(int i=0;i<N;i++){
                for(int j=0;j<N;j++){
                    pheromone[i][j]*=(1-evap);
                }
            }
            for(int k=0;k<M;k++){
                for(int i=0;i<N;i++){
                    pheromone[T[k][i]][T[k][i+1]]+=Q/L[k];
                }
            }
            if(t%3==2){//3 round share omp 蟻巢費洛蒙
                for(int i=tid;i<N;i+=4){
                    for(int j=0;j<N;j++){
                        global_pheromone[i][j]=0;
                    }
                }
                #pragma omp barrier//等待初始化 global_peronmone={0}
                int ring=tid;//4 個蟻巢進行錯位加總
                do{
                    for(int i=ring;i<N;i+=4){
                        for(int j=0;j<N;j++){
                            global_pheromone[i][j]+=pheromone[i][j]/4;
                        }
                    }
                    ring=(ring+1)%4;
                    #pragma omp barrier//等待該 ring 結束
                }while(ring!=tid);
            }
            if(t%6==5){//6 round share mpi 蟻巢費洛蒙
                if(tid==0){
                    MPI_Allreduce(*global_pheromone,*tmp,N*N,MPI_FLOAT,MPI_SUM,MPI_COMM_WORLD);
                    swap(global_pheromone,tmp);
                    for(int i=0;i<N;i++){
                        for(int j=0;j<N;j++){
                            global_pheromone[i][j]/=num_procs;
                        }
                    }
                }
                #pragma omp barrier// 等待 tid0 完成任務
            }
            if(t%3==2){//3 round,6 round
                //將 global pheromone 複製到自己的矩陣
                for(int i=0;i<N;i++){
                    for(int j=0;j<N;j++){
                        pheromone[i][j]=global_pheromone[i][j];
                    }
                }
            }
        }
    
        #pragma omp critical
        {
            //cout<<mpi_id*4+tid<<" "<<Lbest<<"\n";
            if(Lbest<Lglobal){
                Lglobal=Lbest;
                memcpy(Tglobal,Tbest,sizeof(int)*(N+1));
            }
        }
    }

    //get global min
    struct {
        int cost;
        int rank;
    } loc_data, global_data;

    loc_data.cost = Lglobal;
    loc_data.rank = mpi_id;
    
    MPI_Allreduce(&loc_data, &global_data, 1, MPI_2INT, MPI_MINLOC, MPI_COMM_WORLD);

    if (global_data.rank){
        if (mpi_id == 0)
            MPI_Recv(Tglobal,N+1,MPI_INT,global_data.rank,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
        else if (mpi_id == global_data. rank)
            MPI_Send(Tglobal,N+1,MPI_INT,0,0,MPI_COMM_WORLD);
    }
    Lglobal=global_data.cost;

    //print cycle with min L
    if(mpi_id==0){
        cout<<"Filename is "<<fname<<"\n";
        cout<<"The min length of cycle is "<<Lglobal<<"\n";
        cout<<"The cycle with min length is:\n";
        for(int i=0;i<N;i++){
            cout<<Tglobal[i]<<" -> ";
        }
        cout<<Tglobal[N]<<"\n";
    }
    
    MPI_Finalize();
}