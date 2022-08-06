#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cmath>

using namespace std;

int** d;
int NC=150; //calculate times
int M=1000; //#ant
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

void updateP(vector<vector<int>> &P,vector<vector<float>> &pheromone){
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
        Psum=10000.0f/Psum;//want to equally divided P to 10000 
        for(int j=0;j<N;j++){
            P[i][j]=Pt[j]*Psum;
        }
    }
    return;
}

int random_city(vector<int> &P,vector<bool> &Sp){
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

int* array(int n){
    return (int*)malloc(sizeof(int)*n);
}

int** matrix(int r,int c){
    int** ret=(int**)malloc(sizeof(int*)*r);
    int* vals=(int*)malloc(sizeof(int)*r*c);
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
    d=matrix(N,N);
    for(int i=0;i<N;i++){
        for(int j=0;j<N;j++){
            d[i][j]=dt[i][j];
        }
    }
}

int main(int argc,char* argv[]){
    char* fname;
    if(argc>=2&&argv[1][0]!='-'){
        fname=argv[1];
    }else{
        cout<<"Please input filename in argv.\n";
        return 0;
    }

    //read distance
    get_distance(fname);
    
    //init other
    vector<vector<float>> pheromone(N,vector<float>(N,1.0f));//pheromone
    vector<vector<int>> P(N,vector<int>(N));//probability
    int Lbest=10000000;//min length of cycle
    int* Tbest=array(N+1);//the cycle have min length
    int** T=matrix(M,N+1);//cycle per ant
    vector<int> L(M);//L per ant
    srand(2);

    //P calculate
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
    }
    
    //print cycle with min L
    cout<<"Filename is "<<fname<<"\n";
    cout<<"The min length of cycle is "<<Lbest<<"\n";
    cout<<"The cycle with min length is:\n";
    for(int i=0;i<N;i++){
        cout<<Tbest[i]<<" -> ";
    }
    cout<<Tbest[N]<<"\n";
}