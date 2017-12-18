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
    int dim; //allocated dim for vector 'e'
    int currpos; //current position (real dim) of 'e'
} Solution;

int power(int base, int exp);

void quickSort(int *arr, int *arr2, int low, int high);
int partition(int *arr, int *arr2, int low, int high);
void swap(int *a, int *b);

void graph_coloring(Solution *sol, int **conflicts, int *stexams, int nexams, int tmax);
void density_gen(int *density, int **conflicts, Solution *sol, int tmax);
void gauss_sorting(int *density, int *newdens, int *idcorr, int dim, int *mark);
int search_tmp_max(int *density, int *mark, int *id, int dim, int max_tmp);

void disp(int pos, int *valfreets, int **conflicts, Solution *sol, Solution **psol, int *mark, int *timeslots, int *freetimeslots, int nexams, int nstudents, int tmax, int freets);
//void perm(int pos, int **conflicts, Solution *sol, Solution **psol, int *timeslots, int *mark, int nexams, int nstudents, int tmax);
void check_best_sol(int **conflicts, Solution *sol, Solution **psol, int *timeslots, int *freetimeslots, int nstudents, int tmax, int freets);

void free2d(int **matr, int n);

float obj=INT_MAX;

int main(int argc, char **argv)
{
    FILE *fp=NULL;

    Solution *sol, **psol;
    int *mark, *priority, *stexams, *density, *newdens, *idcorr, *timeslots, *freetimeslots, *valfreets;

    int **table_schedule=NULL, **conflicts=NULL; //from instanceXX.stu and instanceXX.exm
    int tmax=0; //from instanceXX.slo

    int i=0, j=0, k=0, nexams=0, nstudents=-1, tmp_dim=1000, examid=0, nstudconf=0, freets=0;
    char buffer[BUF], studentid[BUF], student_tmp[BUF]=" ";

    if(argc!=N){
        fprintf(stderr, "Usage: <%s> <instanceXX.stu> <instanceXX.exm> <instanceXX.slo>\n", argv[0]);
        exit(0);
    }

    ///.EXM FILE
    fprintf(stdout, "Reading file %s...\n", argv[2]);
    fp = fopen(argv[2], "r");
    if(fp==NULL){
        fprintf(stderr, "Error: can't open file %s\n", argv[2]);
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
        fprintf(stderr, "Error: can't open file %s\n", argv[1]);
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
        if(nstudents==tmp_dim){ // menage reallocation
            tmp_dim = tmp_dim*2;
            table_schedule = realloc(table_schedule, tmp_dim*sizeof(int*));
            for(i=nstudents; i<tmp_dim; i++){
                table_schedule[i] = malloc(nexams*sizeof(int));
                for(j=0; j<nexams; j++)
                    table_schedule[i][j]=0;
            }
        }
        table_schedule[nstudents][examid-1]=1;
        strcpy(student_tmp, studentid);
    }

    fclose(fp);
    fprintf(stdout, "Done\n\n");

    ///.SLO FILE
    fprintf(stdout, "Reading file %s...\n", argv[3]);
    fp = fopen(argv[3], "r");
    if(fp==NULL){
        fprintf(stderr, "Error: can't open file %s\n", argv[3]);
        exit(-3);
    }

    //reading of .slo to know the maximum number of time slot
    fscanf(fp, "%d", &tmax);

    fclose(fp);
    fprintf(stdout, "Done\n\n");

    fprintf(stdout, "Total number of exams: %d\n"
                    "Total students: %d\n"
                    "Number of time slots: %d\n\n", nexams, nstudents+1, tmax);

    ///GENERATION OF CONFLICTS MATRIX
    conflicts = malloc(nexams*sizeof(int*));
    for(i=0; i<nexams; i++)
        conflicts[i] = malloc(nexams*sizeof(int));

    for(j=0; j<nexams; j++){
        for(k=0; k<nexams; k++){
            nstudconf=0;
            for(i=0; i<=nstudents; i++){
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

    for(i=0; i<nexams; i++){ // TO DO: merge with GENERATION OF CONFLICTS MATRIX
        for(j=0; j<nexams; j++)
            priority[i]+=conflicts[i][j];
        stexams[i]=i;
    }
    quickSort(priority, stexams, 0, nexams-1);

    fprintf(stdout, "Generation of a feasible solution (Graph coloring)...\n");
    graph_coloring(sol, conflicts, stexams, nexams, tmax);

    fprintf(stdout, "Generation of 'density array' (Density generation)...\n");
    density = calloc(tmax, sizeof(int));
    density_gen(density, conflicts, sol, tmax);

    fprintf(stdout, "Sorting of 'density array' (Gauss function)...\n");
    for(i=0; i<tmax; i++)
        if(density[i]==0)
            freets++; // count how much free time slots i have
    newdens = malloc((tmax-freets)*sizeof(int)); // newdens=gauss_sorting(density)
    idcorr = malloc((tmax-freets)*sizeof(int)); // it will contain the index corresponding to the value of: newdens[i]=value - idcorr[i]=idexams
    mark = calloc(tmax, sizeof(int));
    gauss_sorting(density, newdens, idcorr, tmax-freets, mark);

    //Reset 'mark' because i use it in the previous function
    for(i=0; i<tmax; i++)
        mark[i]=0;

    fprintf(stdout, "Searching for a better solution...\n\n");
    timeslots = malloc(tmax*sizeof(int));
    if(freets!=0){
        for(i=0; i<tmax-freets; i++) // set in 'timeslots' the new order of exams generated in 'gauss_sorting()'
            timeslots[i]=idcorr[i];
        freetimeslots = malloc((tmax-freets)*sizeof(int));
        valfreets = malloc(tmax*sizeof(int)); // this array'll contain the number of time slots from 0 to tmax-1
        for(i=0; i<tmax; i++) // initialization of 'valfreets', i'll put a disposition of these values in 'freetimeslots' that says me in which slot i have to put the free time slot
            valfreets[i]=i;
        disp(0, valfreets, conflicts, sol, psol, mark, timeslots, freetimeslots, nexams, nstudents, tmax, freets);
    }
    else{
        for(i=0; i<tmax; i++)
            timeslots[i]=idcorr[i];
        check_best_sol(conflicts, sol, psol, timeslots, NULL, nstudents, tmax, freets);
    }

//    fprintf(stdout, "Swapping time slot and searching for a better solution...\n");
//    timeslots = malloc(tmax*sizeof(int));
//    perm(0, conflicts, sol, psol, timeslots, mark, nexams, nstudents, tmax);

    ///DEALLOCATION AND END OF PROGRAM
    free(sol);
    free(psol);
    free(density);
    free(mark);
    free(timeslots);
    free(freetimeslots);
    free2d(table_schedule, tmp_dim);
    free2d(conflicts, nexams);

    return 0;
}

void graph_coloring(Solution *sol, int **conflicts, int *stexams, int nexams, int tmax)
{
    int i=0, j=0, c=0, *colors, *available;

    colors = malloc(nexams*sizeof(int));
    for(i=0; i<nexams; i++)
        colors[i]=-1; // 'i'=exam and colors[i]=assigned color, if=-1 not yet assigned

    available = calloc(tmax, sizeof(int)); // 'i'=color and available[i]=0 available, available[i]=1 unavailable

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

        printf("c=%d\n", c);

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

void density_gen(int *density, int **conflicts, Solution *sol, int tmax)
{
    int i=0, j=0, k=0, t=0;

    ///DENSITY 2^(5-i)*Ne,e'
    for(i=0; i<tmax; i++){ //cycle in time slot sol[0, ..., tmax-1]
        for(k=0; k<sol[i].currpos; k++){ //cycle in sol[i].e[0, ..., k] - exams i scheduled in time slot 'i'
            for(j=0; j<=i+5 && j<tmax; j++){ //after 5 i don't pay no kind of penalty - cycle to compare time slot 'i' with time slot 'j'
                for(t=0; t<sol[j].currpos; t++){ //i have to compare all exams
                    if(sol[i].e[k]!=sol[j].e[t]){
                        if(j>i)
                            density[i] += conflicts[sol[i].e[k]][sol[j].e[t]]*power(2, 5-(j-i));
                        else
                            density[i] += conflicts[sol[i].e[k]][sol[j].e[t]]*power(2, 5-(i-j));
                    }
                }
            }
        }
    }
}

void gauss_sorting(int *density, int *newdens, int *idcorr, int dim, int *mark)
{
    int i=0, id=0, max=-1, up=0, down=dim-1;

    for(i=0; i<dim; i=i+2){ // every cycle i have to search the first two new max values
        max = search_tmp_max(density, mark, &id, dim, -1);
        if(max!=-1){
            newdens[up]=max;
            idcorr[up]=id;
            up++;
        }
        max = search_tmp_max(density, mark, &id, dim, -1);
        if(max!=-1){
            newdens[down]=max;
            idcorr[down]=id;
            down--;
        }
    }
}

int search_tmp_max(int *density, int *mark, int *id, int dim, int max_tmp)
{
    int i=0, index=0, flag=0;

    for(i=0; i<dim; i++){
        if(mark[i]==0 && max_tmp<density[i]){
            flag=1;
            index=i;
            max_tmp = density[i];
        }
    }

    if(flag==1){
        mark[index]=1;
        (*id)=index;
    }

    return max_tmp;
}

void disp(int pos, int *valfreets, int **conflicts, Solution *sol, Solution **psol, int *mark, int *timeslots, int *freetimeslots, int nexams, int nstudents, int tmax, int freets)
{
    int i=0;

    if(pos>=freets){
        check_best_sol(conflicts, sol, psol, timeslots, freetimeslots, nstudents, tmax, freets);
        return ;
    }

    for(i=0; i<tmax; i++){
        if(mark[i]==0){
            mark[i]=1;
            freetimeslots[pos]=valfreets[i];
            disp(pos+1, valfreets, conflicts, sol, psol, mark, timeslots, freetimeslots, nexams, nstudents, tmax, freets);
            mark[i]=0;
        }
    }

    return ;
}

/*
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
*/

void check_best_sol(int **conflicts, Solution *sol, Solution **psol, int *timeslots, int *freetimeslots, int nstudents, int tmax, int freets)
{
    int i=0, j=0, k=0, t=0, flag=0, *inserted;
    float obj_tmp=0;

    if(freets!=0){
        inserted = calloc(tmax, sizeof(int));

        //Searching a free time slot
        for(i=0; i<tmax; i++)
            if(sol[i].currpos==0)
                break;

        //When i found it i put the address of the free time slot in the index i disposed in 'freetimeslots'
        for(j=0; j<tmax-freets; j++)
            psol[freetimeslots[j]] = &sol[i];

        //Generation of the new solution
        for(i=0; i<tmax; i++){
            flag=0;
            for(k=0; k<tmax-freets && flag==0; k++)
                if(i==freetimeslots[k])
                    flag=1;
            for(j=0; j<tmax-freets && flag==0; j++){
                if(inserted[j]==0){
                    inserted[j]=1;
                    psol[i] = &sol[timeslots[j]];
                    flag=1;
                }

            }
        }
    }
    else{
        for(i=0; i<tmax; i++)
            psol[i] = &sol[timeslots[i]];
    }

    ///MIN OBJ 2^(5-i)*Ne,e'/|S|
    for(i=0; i<tmax-1; i++){ //cycle in time slot sol[0, ..., tmax-1]
        for(k=0; k<psol[i]->currpos; k++){ //cycle in sol[i]->e[0, ..., k] - exams i scheduled in time slot 'i'
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

    free(inserted);
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
