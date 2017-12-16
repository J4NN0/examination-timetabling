#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#define N 3+1 //number of parameters have to receives from command line
#define BUF 10

/* It's receives from the command line 3 files, respectively: instanceXX.stu instanceXX.exm instanceXX.slo */

typedef struct solution{
    int *e; //vectors says me if exam 'eX' is scheduled in the time slot 'i'. EX: sol[i].e[j]=1 mens exam 'e1' is scheduled in the time slot 'i'
    int dim; //allocated dim for vector e
    int currpos; //current position (real dim) of e
} Solution;

int power(int base, int exp);
void quickSort(int *arr, int *arr2, int low, int high);
int partition(int *arr, int *arr2, int low, int high);
void swap(int *a, int *b);
void graph_coloring(Solution *sol, int **conflicts, int *priority, int *stexams, int nexams, int tmax);
void perm(int pos, int **conflicts, Solution *sol, Solution **psol, int *timeslots, int *mark, int nexams, int nstudents, int tmax);
void check_best_sol(int **conflicts, Solution *sol, Solution **psol, int *timeslots, int nstudents, int tmax);
void free2d(int **matr, int n);

float obj=INT_MAX;

int main(int argc, char **argv)
{
    FILE *fp=NULL;

    Solution *sol, **psol;
    int *mark, *priority, *stexams, *timeslots;

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
        table_schedule[nstudents][examid-1]=1;
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
    psol = malloc(tmax*sizeof(Solution*));
    sol = malloc(tmax*sizeof(Solution));
    for(i=0; i<tmax; i++){
        sol[i].e = malloc((nexams/2)*sizeof(int));
        sol[i].dim=nexams/2;
        sol[i].currpos=0;
    }

    priority = calloc(nexams, sizeof(int)); //each indexes represent an exam and the value of that index represent the total number of conflicts between this exam and the others
    stexams = malloc(nexams*sizeof(int)); //it will contain a sorted number of indexes (each index represent an exam) based on the priority of the previous vector

    for(i=0; i<nexams; i++){ //TODO: merge with GENERATION OF CONFLICTS MATRIX
        for(j=0; j<nexams; j++)
            priority[i]+=conflicts[i][j];
        stexams[i]=i;
    }
    quickSort(priority, stexams, 0, nexams-1);

    fprintf(stdout, "Generation of feasible solution (Graph coloring)...\n");
    graph_coloring(sol, conflicts, priority, stexams, nexams, tmax);

    mark = calloc(tmax, sizeof(int));
    timeslots = malloc(tmax*sizeof(int));

    fprintf(stdout, "Swapping time slot and generation of a best solution...\n");
    perm(0, conflicts, sol, psol, timeslots, mark, nexams, nstudents, tmax);

    ///DEALLOCATION AND END OF PROGRAM
    free(sol);
    free(psol);
    free(mark);
    free(timeslots);
    free2d(table_schedule, tmp_dim);
    free2d(conflicts, nexams);

    return 0;
}

void graph_coloring(Solution *sol, int **conflicts, int *priority, int *stexams, int nexams, int tmax)
{
    int i=0, j=0, c=0, *colors, *available;

    colors = malloc(nexams*sizeof(int));
    for(i=0; i<nexams; i++)
        colors[i]=-1; // 'i'=exam and colors[i]=assigned color, if=-1 not yet assigned

    available = calloc(tmax, sizeof(int)); // 'i'=color and available[i]=0 if available, available[i]=1 unavailable

    sol[0].e[sol[0].currpos] = stexams[0]; //first color to first exam in priority
    sol[0].currpos++;
    colors[stexams[0]]=0;
    for(i=1; i<nexams; i++){ //stexams[i] is the exam i'm considering (priority array)
        for(j=0; j<nexams; j++){ // i search if my exam has conflict with other exams
            if(conflicts[stexams[i]][j]>0 && colors[j]>=0) // if there is a conflict and this exam 'j' has a color, so colors[j]!=-1
                available[colors[j]]=1; //set as unavailable
        }

        // find the first available color
        for(c=0; c<tmax; c++)
            if(available[c]==0)
                break;

        // assign the found color
        sol[c].e[sol[c].currpos] = stexams[i];
        sol[c].currpos++;
        colors[stexams[i]]=c;

        // reset available colors
        for(c=0; c<tmax; c++)
            available[c]=0;
    }

    free(colors);
    free(available);
}

void perm(int pos, int **conflicts, Solution *sol, Solution **psol, int *timeslots, int *mark, int nexams, int nstudents, int tmax)
{
    int i=0;

    if(pos>=tmax){
        check_best_sol(conflicts, sol, psol, timeslots, nstudents, tmax);
        return ;
    }

    for(i=0; i<tmax; i++){
        if(mark[i]==0){
            mark[i]=1;
            timeslots[i]=pos;
            perm(pos+1, conflicts, sol, psol, timeslots, mark, nexams, nstudents, tmax);
            mark[i]=0;
        }
    }

    return ;
}

void check_best_sol(int **conflicts, Solution *sol, Solution **psol, int *timeslots, int nstudents, int tmax)
{
    int i=0, j=0, k=0, t=0;
    float obj_tmp=0;

    for(i=0; i<tmax; i++)
        psol[i] = &sol[timeslots[i]];

    ///MIN OBJ 2^(5-i)*Ne,e'/|S|
    for(i=0; i<tmax-1; i++){ //cycle in time slot sol[0, ..., tmax-1]
        for(k=0; k<psol[i]->currpos; k++){ //cycle in sol[i].e[0, ..., k] - exams i scheduled in time slot 'i'
            for(j=i; j<=i+5 && j<tmax; j++){ //after 5 i don't pay no kind of penalty - cycle to compare time slot 'i' with time slot 'j'
                if(i==j){ //if i'm considering the same time slot i must avoid to compare eX - eY and then eY - eX
                    for(t=k+1; t<psol[j]->currpos; t++) //so i start from the same value+1
                        obj_tmp += conflicts[psol[i]->e[k]][psol[j]->e[t]]*power(2, 5-(j-i));
                }
                else{ //otherwise i have to start from zero to compare all exams
                    for(t=0; t<psol[j]->currpos; t++)
                        if(psol[i]->e[k]!=psol[j]->e[t])
                            obj_tmp += conflicts[psol[i]->e[k]][psol[j]->e[t]]*power(2, 5-(j-i));
                }
            }
        }
    }

    if((obj_tmp/nstudents)<obj){
        fprintf(stdout, "New obj found. NEW: %f\n", obj_tmp/nstudents);
        obj = obj_tmp/nstudents;
    }
}

void quickSort(int *arr, int *arr2, int low, int high)
{
    if(low<high){
        int pi = partition(arr, arr2, low, high);
        quickSort(arr, arr2, low, pi - 1);
        quickSort(arr, arr2, pi + 1, high);
    }
}

int partition(int *arr, int *arr2, int low, int high)
{
    int pivot=arr[high];
    int i=(low - 1);
    int j=low;

    for(j=low; j<=high-1; j++){
        if (arr[j] >= pivot){
            i++;
            swap(&arr[i], &arr[j]);
            swap(&arr2[i], &arr2[j]);
        }
    }

    swap(&arr[i + 1], &arr[high]);
    swap(&arr2[i+1], &arr2[high]);

    return (i+1);
}

void swap(int *a, int *b)
{
    int t=*a;
    *a=*b;
    *b=t;
}

int power(int base, int exp)
{
    int i=0, res=1;

    for(i=0; i<exp; i++)
        res *= base;

    return res;
}

void free2d(int **matr, int n)
{
    int i=0;

    for(i=0; i<n; i++)
        free(matr[i]);

    free(matr);
}
