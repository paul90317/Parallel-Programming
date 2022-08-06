#include <mpi.h>
#include <iostream>
#include <string>
#include <fstream>
#include <stdio.h>
#include <cstring>
#include <cstdlib>
#include "bmp.h"

using namespace std;

//定義平滑運算的次數
int NSmooth;

/*********************************************************/
/*變數宣告：                                             */
/*  bmpHeader    ： BMP檔的標頭                          */
/*  bmpInfo      ： BMP檔的資訊                          */
/*  **BMPSaveData： 儲存要被寫入的像素資料               */
/*  **BMPData    ： 暫時儲存要被寫入的像素資料           */
/*********************************************************/
BMPHEADER bmpHeader;
BMPINFO bmpInfo;
RGBTRIPLE **BMPSaveData = NULL;
RGBTRIPLE **BMPData = NULL;

/*********************************************************/
/*函數宣告：                                             */
/*  readBMP    ： 讀取圖檔，並把像素資料儲存在BMPSaveData*/
/*  saveBMP    ： 寫入圖檔，並把像素資料BMPSaveData寫入  */
/*  swap       ： 交換二個指標                           */
/*  **alloc_memory： 動態分配一個Y * X矩陣               */
/*********************************************************/
int readBMP(char *fileName); //read file
int saveBMP(char *fileName); //save file
RGBTRIPLE **alloc_memory(int Y, int X); //allocate memory

void swap_edge(RGBTRIPLE **dataLines, int local_count, int width, int id, int num_procs)
{
    if(num_procs==1){
        for (int j = 0; j < width; j++)
        {
            dataLines[0][j] = dataLines[local_count][j];
            dataLines[local_count + 1][j] = dataLines[1][j];
        }
        return;
    }
    //得出上下 proc id
    int id_up = id == 0 ? num_procs - 1 : id - 1;
    int id_down = id == num_procs - 1 ? 0 : id + 1;
    //將下界送給下 proc 之上界
    if (id % 2)
    {
        MPI_Send(dataLines[local_count], width * 3, MPI_BYTE, id_down, 0, MPI_COMM_WORLD);
        MPI_Recv(dataLines[0], width * 3, MPI_BYTE, id_up, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    else
    {
        MPI_Recv(dataLines[0], width * 3, MPI_BYTE, id_up, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Send(dataLines[local_count], width * 3, MPI_BYTE, id_down, 0, MPI_COMM_WORLD);
    }
    //將上界送給上 proc 之下界
    if (id % 2)
    {
        MPI_Send(dataLines[1], width * 3, MPI_BYTE, id_up, 0, MPI_COMM_WORLD);
        MPI_Recv(dataLines[local_count + 1], width * 3, MPI_BYTE, id_down, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    else
    {
        MPI_Recv(dataLines[local_count + 1], width * 3, MPI_BYTE, id_down, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Send(dataLines[1], width * 3, MPI_BYTE, id_up, 0, MPI_COMM_WORLD);
    }
}

int main(int argc, char *argv[])
{
    /*********************************************************/
    /*變數宣告：                                             */
    /*  *infileName  ： 讀取檔名                             */
    /*  *outfileName ： 寫入檔名                             */
    /*  startwtime   ： 記錄開始時間                         */
    /*  endwtime     ： 記錄結束時間                         */
    /*********************************************************/
    char *infileName = "input.bmp";
    char *outfileName = "output.bmp";
    double startwtime = 0.0, endwtime = 0;

    //MPI_Init(&argc,&argv);
    int num_procs, id;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &id);

    if (id == 0)
    {
        //讀取檔案
        if (readBMP(infileName))
            cout << "Read file successfully!!" << endl;
        else
            cout << "Read file fails!!" << endl;
    }
    
    //記錄開始時間
    MPI_Barrier(MPI_COMM_WORLD);
    startwtime = MPI_Wtime();

    NSmooth=1000;
    if(argc==2){
        NSmooth=strtol(argv[1],NULL,10);
    }

    //資料通訊預處理
    int all_count = bmpInfo.biHeight;
    int width = bmpInfo.biWidth;
    MPI_Bcast(&all_count, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&width, 1, MPI_INT, 0, MPI_COMM_WORLD);
    int rem = all_count % num_procs;
    int local_count = all_count / num_procs + (id < rem);

    int *sendcounts = NULL;
    int *displs = NULL;

    if (id == 0)
    {
        //分配任務預處理
        int sum = 0;
        sendcounts = (int *)malloc(sizeof(int) * num_procs);
        displs = (int *)malloc(sizeof(int) * num_procs);
        for (int i = 0; i < num_procs; i++)
        {
            sendcounts[i] = all_count / num_procs + (i < rem);
            displs[i] = sum;
            sum += sendcounts[i];
            sendcounts[i] *= width * 3;
            displs[i] *= width * 3;
        }
    }
    else
    {
        //其他 proc 挖陣列
        BMPSaveData = alloc_memory(local_count + 2, width);
    }
    BMPData = alloc_memory(local_count + 2, width);
    //分配 BMPData 到 BMPSaveData
    MPI_Scatterv(*BMPSaveData, sendcounts, displs, MPI_BYTE, BMPData[1], local_count * width * 3, MPI_BYTE, 0, MPI_COMM_WORLD);
    swap(BMPSaveData,BMPData);

    //進行多次的平滑運算
    for (int count = 0; count < NSmooth; count++)
    {
        //把像素資料與暫存指標做交換
        swap(BMPSaveData, BMPData);

        //交換邊界
        swap_edge(BMPData,local_count,width,id,num_procs);

        //進行平滑運算
        for (int i = 1; i <= local_count; i++)
        {
            for (int j = 0; j < width; j++)
            {
                /*********************************************************/
                /*設定上下左右像素的位置                                 */
                /*********************************************************/
                int Top = i - 1;
                int Down = i + 1;
                int Left = j > 0 ? j - 1 : width - 1;
                int Right = j < width - 1 ? j + 1 : 0;
                /*********************************************************/
                /*與上下左右像素做平均，並四捨五入                       */
                /*********************************************************/
                BMPSaveData[i][j].rgbBlue = (double)(BMPData[i][j].rgbBlue + BMPData[Top][j].rgbBlue + BMPData[Top][Left].rgbBlue + BMPData[Top][Right].rgbBlue + BMPData[Down][j].rgbBlue + BMPData[Down][Left].rgbBlue + BMPData[Down][Right].rgbBlue + BMPData[i][Left].rgbBlue + BMPData[i][Right].rgbBlue) / 9 + 0.5;
                BMPSaveData[i][j].rgbGreen = (double)(BMPData[i][j].rgbGreen + BMPData[Top][j].rgbGreen + BMPData[Top][Left].rgbGreen + BMPData[Top][Right].rgbGreen + BMPData[Down][j].rgbGreen + BMPData[Down][Left].rgbGreen + BMPData[Down][Right].rgbGreen + BMPData[i][Left].rgbGreen + BMPData[i][Right].rgbGreen) / 9 + 0.5;
                BMPSaveData[i][j].rgbRed = (double)(BMPData[i][j].rgbRed + BMPData[Top][j].rgbRed + BMPData[Top][Left].rgbRed + BMPData[Top][Right].rgbRed + BMPData[Down][j].rgbRed + BMPData[Down][Left].rgbRed + BMPData[Down][Right].rgbRed + BMPData[i][Left].rgbRed + BMPData[i][Right].rgbRed) / 9 + 0.5;
            }
        }
    }
    //合併結果
    int *recvcounts = sendcounts;
    MPI_Gatherv(BMPSaveData[1],local_count*width*3,MPI_BYTE,*BMPData,recvcounts,displs,MPI_BYTE,0,MPI_COMM_WORLD);
    swap(BMPSaveData,BMPData);

    //得到結束時間
    MPI_Barrier(MPI_COMM_WORLD);
    endwtime = MPI_Wtime();

    MPI_Finalize();

    if (id == 0)
    {
        //印出執行時間
        cout << "The execution time = "<< endwtime-startwtime <<endl ;

        //寫入檔案
        if (saveBMP(outfileName))
            cout << "Save file successfully!!" << endl;
        else
            cout << "Save file fails!!" << endl;
    }
    
    return 0;
}

/*********************************************************/
/* 讀取圖檔                                              */
/*********************************************************/
int readBMP(char *fileName)
{
    //建立輸入檔案物件
    ifstream bmpFile(fileName, ios::in | ios::binary);

    //檔案無法開啟
    if (!bmpFile)
    {
        cout << "It can't open file!!" << endl;
        return 0;
    }

    //讀取BMP圖檔的標頭資料
    bmpFile.read((char *)&bmpHeader, sizeof(BMPHEADER));

    //判決是否為BMP圖檔
    if (bmpHeader.bfType != 0x4d42)
    {
        cout << "This file is not .BMP!!" << endl;
        return 0;
    }

    //讀取BMP的資訊
    bmpFile.read((char *)&bmpInfo, sizeof(BMPINFO));

    //判斷位元深度是否為24 bits
    if (bmpInfo.biBitCount != 24)
    {
        cout << "The file is not 24 bits!!" << endl;
        return 0;
    }

    //修正圖片的寬度為4的倍數
    while (bmpInfo.biWidth % 4 != 0)
        bmpInfo.biWidth++;

    //動態分配記憶體
    BMPSaveData = alloc_memory(bmpInfo.biHeight+2, bmpInfo.biWidth);//+2 做上下兩排的 extend

    //讀取像素資料
    //for(int i = 0; i < bmpInfo.biHeight; i++)
    //	bmpFile.read( (char* )BMPSaveData[i], bmpInfo.biWidth*sizeof(RGBTRIPLE));
    bmpFile.read((char *)BMPSaveData[0], bmpInfo.biWidth * sizeof(RGBTRIPLE) * bmpInfo.biHeight);

    //關閉檔案
    bmpFile.close();

    return 1;
}
/*********************************************************/
/* 儲存圖檔                                              */
/*********************************************************/
int saveBMP(char *fileName)
{
    //判決是否為BMP圖檔
    if (bmpHeader.bfType != 0x4d42)
    {
        cout << "This file is not .BMP!!" << endl;
        return 0;
    }

    //建立輸出檔案物件
    ofstream newFile(fileName, ios::out | ios::binary);

    //檔案無法建立
    if (!newFile)
    {
        cout << "The File can't create!!" << endl;
        return 0;
    }

    //寫入BMP圖檔的標頭資料
    newFile.write((char *)&bmpHeader, sizeof(BMPHEADER));

    //寫入BMP的資訊
    newFile.write((char *)&bmpInfo, sizeof(BMPINFO));

    //寫入像素資料
    //for( int i = 0; i < bmpInfo.biHeight; i++ )
    //        newFile.write( ( char* )BMPSaveData[i], bmpInfo.biWidth*sizeof(RGBTRIPLE) );
    newFile.write((char *)BMPSaveData[0], bmpInfo.biWidth * sizeof(RGBTRIPLE) * bmpInfo.biHeight);

    //寫入檔案
    newFile.close();

    return 1;
}

/*********************************************************/
/* 分配記憶體：回傳為Y*X的矩陣                           */
/*********************************************************/
RGBTRIPLE **alloc_memory(int Y, int X)
{
    //建立長度為Y的指標陣列
    RGBTRIPLE **temp = new RGBTRIPLE *[Y];
    RGBTRIPLE *temp2 = new RGBTRIPLE[Y * X];
    memset(temp, 0, sizeof(RGBTRIPLE) * Y);
    memset(temp2, 0, sizeof(RGBTRIPLE) * Y * X);

    //對每個指標陣列裡的指標宣告一個長度為X的陣列
    for (int i = 0; i < Y; i++)
    {
        temp[i] = &temp2[i * X];
    }

    return temp;
}
