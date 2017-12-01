#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 3+1 //number of parameters have to receives from command line
#define BUF 10

/* It's receives from the command line 3 files, respectively: instanceXX.stu instanceXX.exm instanceXX.slo */

void free2d(int **matr, int n);

int main(int argc, char **argv)
{
    FILE *fp=NULL;
    int **table_schedule=NULL, **conflicts=NULL; //from instanceXX.stu and instanceXX.exm
    int tmax=0; //from instanceXX.slo
    int i=0, j=0, k=0, nexams=0, nstudents=-1, tmp_dim=611, examid=0, nstudconf=0;
    char buffer[BUF], studentid[BUF], student_tmp[BUF];

    if(argc!=N){
        fprintf(stderr, "Usage: <%s> <instanceXX.stu> <instanceXX.exm> <instanceXX.slo>", argv[0]);
        exit(0);
    }

    ///.EXM FILE
    fprintf(stdout, "Reading file %s...\n", argv[2]);
    fp = fopen(argv[2], "r");
    if(fp==NULL){
        fprintf(stderr, "Error: can't open file %s", argv[2]);
        exit(-2);
    }

    //reading of .exm to know how many exams there are
    while(fgets(buffer, BUF, fp)!=NULL)
        nexams++;

    fclose(fp);
    fprintf(stdout, "Done\n\n");

    ///.STU FILE
    fprintf(stdout, "Reading file %s...\n", argv[1]);
    fp = fopen(argv[1], "r");
    if(fp==NULL){
        fprintf(stderr, "Error: can't open file %s", argv[1]);
        exit(-1);
    }

    table_schedule = malloc(tmp_dim*sizeof(int*));
    for(i=0; i<tmp_dim; i++){
        table_schedule[i] = malloc(nexams*sizeof(int));
        for(j=0; j<nexams; j++)
            table_schedule[i][j]=0;
    }

    //reading of .stu to fill table_schedule
    while(fscanf(fp, "%s %d", studentid, &examid)!=EOF){
        if(strcmp(studentid, student_tmp)!=0)
            nstudents++;
        table_schedule[nstudents][examid]=1;
        strcpy(student_tmp, studentid);
    }

    fclose(fp);
    fprintf(stdout, "Done\n\n");

    ///.SLO FILE
    fprintf(stdout, "Reading file %s...\n", argv[3]);
    fp = fopen(argv[3], "r");
    if(fp==NULL){
        fprintf(stderr, "Error: can't open file %s", argv[3]);
        exit(-3);
    }

    //reading of .slo to know the maximum number of time slot
    fscanf(fp, "%d", &tmax);

    fclose(fp);
    fprintf(stdout, "Done\n\n");

    ///GENERATION OF CONFLICTS MATRIX
    conflicts = malloc(nexams*sizeof(int*));
    for(i=0; i<nexams; i++)
        conflicts[i] = malloc(nexams*sizeof(int));

    for(j=0; j<nexams; j++){
        for(k=0; k<nexams; k++){
            nstudconf=0;
            for(i=0; i<tmp_dim; i++){
                if(table_schedule[i][j]==1 && table_schedule[i][k]==1 && j!=k)
                    nstudconf++;
            }
            conflicts[j][k]=nstudconf;
        }
    }

    ///SOLUTION OF THE PROBLEM


    free2d(table_schedule, tmp_dim);
    free2d(conflicts, nexams);

    return 0;
}

void free2d(int **matr, int n){
    int i=0;

    for(i=0; i<n; i++){
        free(matr[i]);
    }
    free(matr);
}
