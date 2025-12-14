#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include "mpi.h"

unsigned char *lerImagemPgm(char *path, int *width, int *height);
void salvarImagemPgm(const char *caminho, unsigned char *dados, int w, int h);
unsigned char getPixel(unsigned char *img, int w, int h, int x, int y);
void aplicaSobel(unsigned char *entrada, unsigned char *saida, int w, int h);

int main(int argc, char *argv[])
{
    int ret, size, rank, i, tag;
    int w, h;

    MPI_Status status;
    ret = MPI_Init(&argc, &argv);
    ret = MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    ret = MPI_Comm_size(MPI_COMM_WORLD, &size);
    tag = 100;
    
    size_t chunkSize;
    unsigned char *entrada = {};
    unsigned char *saida = {};

    if (rank == 0){

        // mestre
        entrada = lerImagemPgm("sample.pgm", &w, &h);
        size_t totalSize = w * h;
        unsigned char *saida = (unsigned char *)malloc(totalSize);
        chunkSize = totalSize / size;
        // percorre os processadores para enviar mensagem
        double t1 = MPI_Wtime();
        for (int i = 1; i<size; i++){
            
            unsigned char currentChunk[chunkSize];
            // copia so o pedaco da imagem que o outro processo precisa
            memcpy(currentChunk, entrada + (chunkSize * i), sizeof(currentChunk));
            printf("\n\n[0] - Enviando para %d\n", i);
            ret = MPI_Send(currentChunk, chunkSize, MPI_CHAR, i, tag, MPI_COMM_WORLD);
            
            unsigned char *buffer = (unsigned char *)malloc(chunkSize);
            ret = MPI_Recv(buffer, chunkSize, MPI_CHAR, i, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            printf("[0] - Recebi o pedaco do processo %d\n", i);
            memcpy(saida + (chunkSize * i), buffer, chunkSize);
            printf("[0] - Escrevi o pedaco do processo %d na saida\n", i);
            
        }

        unsigned char thisChunk[chunkSize] = {};
        unsigned char thisOutput[chunkSize] = {};

        memcpy(thisChunk, entrada, chunkSize);

        printf("\n[0] - Aplicando sobel no pedaco final\n");
        aplicaSobel(thisChunk, thisOutput, w, chunkSize/w);
        printf("\n[0] - Aplicou sobel no pedaco final\n");

        memcpy(saida, thisOutput, chunkSize);
        double t2 = MPI_Wtime();
        salvarImagemPgm("saida.pgm", saida, w, h);

        printf("\n Tempo gasto %f\n", t2 - t1);
        free(entrada);
        free(saida);
    }
    
    else
    {
        unsigned char myChunk[559104];
        unsigned char *myOutput = (unsigned char *)malloc(559104);
        
        ret = MPI_Recv(myChunk, 559104, MPI_CHAR, 0, tag, MPI_COMM_WORLD, &status);
        
        aplicaSobel(myChunk, myOutput, 2048, 559104/2048);
        
        printf("[%d] - Apliquei sobel.\n", rank);
        printf("[%d] - Devolvendo meu pedaco com sobel.\n", rank);

        ret = MPI_Send(myOutput, 559104, MPI_CHAR, 0, tag, MPI_COMM_WORLD);
       
    }

    MPI_Finalize();
    return 0;
}

unsigned char *lerImagemPgm(char *path, int *width, int *height)
{
    FILE *file = fopen(path, "rb");
    if (!file)
    {
        printf("Nao foi possivel encontrar o arquivo pelo caminho especificado");
        return NULL;
    }

    char buffer[16];
    int maxVal;
    fgets(buffer, sizeof(buffer), file);
    if (buffer[0] != 'P')
    {
        printf("Formato do arquivo deve ser .pgm");
        fclose(file);
        return NULL;
    }

    int c = getc(file);
    while (c == '#')
    {
        while (getc(file) != '\n')
            ;
        c = getc(file);
    }
    ungetc(c, file);

    if (fscanf(file, "%d %d", width, height) != 2)
    {
        printf("Erro ao ler dimensões da imagem");
        fclose(file);
        return NULL;
    }

    fscanf(file, "%d", &maxVal);
    fgetc(file);

    unsigned char *memoriaParaImg = (unsigned char *)malloc(*width * *height);
    if (memoriaParaImg == NULL)
    {
        printf("Erro ao alocar memória");
        fclose(file);
        return NULL;
    }

    if (fread(memoriaParaImg, 1, *width * *height, file) != (*width * *height))
    {
        printf("Erro ao ler imagem");
        free(memoriaParaImg);
        fclose(file);

        return NULL;
    }

    fclose(file);

    return memoriaParaImg;
}

void salvarImagemPgm(const char *caminho, unsigned char *dados, int w, int h)
{
    FILE *fp = fopen(caminho, "wb");
    if (!fp)
        return;

    fprintf(fp, "P5\n%d %d\n255\n", w, h);
    fwrite(dados, 1, w * h, fp);
    fclose(fp);
}

unsigned char getPixel(unsigned char *img, int w, int h, int x, int y)
{
    if (x < 0)
        x = 0;
    if (x >= w)
        x = w - 1;
    if (y < 0)
        y = 0;
    if (y >= h)
        y = h - 1;
    return img[y * w + x];
}

void aplicaSobel(unsigned char *entrada, unsigned char *saida, int w, int h)
{
    int Gx[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    int Gy[3][3] = {{-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};
    
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            float sumX = 0, sumY = 0;
            
            // Convolução 3x3
            for (int i = -1; i <= 1; i++)
            {
                for (int j = -1; j <= 1; j++)
                {
                    unsigned char val = getPixel(entrada, w, h, x + j, y + i);
                    sumX += val * Gx[i + 1][j + 1];
                    sumY += val * Gy[i + 1][j + 1];
                }
            }
            
            // Magnitude
            int mag = (int)sqrt(sumX * sumX + sumY * sumY);
            
            // Limitar entre 0 e 255
            if (mag > 255)
            mag = 255;
        if (mag < 0)
        mag = 0;
    
        saida[y * w + x] = (unsigned char)mag;
        }
    }
}