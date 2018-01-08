#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <time.h>
#include <sys/timeb.h>

#define N (3+1) // number of parameters have to receive from command line
#define BUF 100 // buffer size for string
#define TSI 500000 // max iteration for tabu search

/* It's receives from the command line 3 files, respectively: instancename -t timelimit
 * Instance files: instanceXX.stu instanceXX.exm instanceXX.slo */

typedef struct solution{
    int *e; // vectors says me if exam 'eX' is scheduled in the time slot 'i'. EX: sol[i].e[j]=1 mens exam 'e1' is scheduled in the time slot 'i'
    int dim; // allocation
    int currpos; // current position (real dim) of 'e'
} Solution;

int exmFile_read(char *filename);
int **stuFile_read(char *filename, int *nstudents, int nexams);
int sloFile_read(char *filename);

void quickSort(int *arr, int *arr2, int low, int high);
int partition(int *arr, int *arr2, int low, int high);
void swap(int *a, int *b);

void feasible_search(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nexams, int nstudents, int tmax);
void graph_coloring_greedy(Solution *sol, int **conflicts, int *stexams, int nexams, int tmax);
int tabu_search(Solution *sol, int **conflicts, int nexams, int tmax);
int is_forbidden(Solution *sol, int **conflicts, int exam, int timeslot, int *tabulist, int dimtabu);
double confl_exams(Solution *sol, int exam, int timeslot, int **conflicts);
void update_tabu(Solution *sol, int exam, int ts, int **conflicts, int *tabulist, int *next, int dimtabu, int nexams, int tmax);
int schedule_exam(Solution *sol, int **conflicts, int exam, int ts, int nexams, int tmax);

void simulated_annealing(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nexams, int nstudents, int tmax);
double cooling_schedule(double temperature, int iter);
double probabilty(double new_obj, double old_obj, double temperature);
void neighborhood_ssn(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nstudents, int nexams, int tmax, double temperature);
void neighborhood_swn(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nstudents, int nexams, int tmax, double temperature);
void local_search(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nstudents, int nexams, int tmax);
void swap_timeslots(Solution *sol, int ts1, int ts2);
int search_timeslot(Solution *sol, int **conflicts, int exam, int tmax, int *ets, int *epos);
void search_exam_pos(Solution *sol, int exam, int *ets, int *epos, int tmax);
int has_conflicts(Solution *sol, int **conflicts, int exam, int ts);

double check_best_sol(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nstudents, int tmax);
int power(int base, int exp);
void save_sol(Solution *sol, Solution **psol, int tmax);
void restore_sol(Solution *sol, Solution **psol, int tmax);
void print_bestsol(Solution **bsol, char *filename, int tmax);

void free2d(int **matr, int n);

struct timeb startTime;
double obj=INT_MAX;
int timelimit=0;

int main(int argc, char **argv)
{
    ftime(&startTime);

    Solution *sol, **bsol;
    int **table_schedule, **conflicts; //from instanceXX.stu and instanceXX.exm
    int tmax=0; //from instanceXX.slo
    int i=0, j=0, k=0, nexams=0, nstudents=-1, nstudconf=0;
    char instancename[BUF];

    srand((unsigned)time(NULL));

    if(argc!=N){
        fprintf(stderr, "Error. Usage: <%s> <instancename> -t <timelimit>\n", argv[0]);
        exit(0);
    }

    ///.EXM FILE
    strcpy(instancename, argv[1]);
    strcat(instancename, ".exm");
    nexams = exmFile_read(instancename);

    ///.STU FILE
    strcpy(instancename, argv[1]);
    strcat(instancename, ".stu");
    table_schedule = stuFile_read(instancename, &nstudents, nexams);

    ///.SLO FILE
    strcpy(instancename, argv[1]);
    strcat(instancename, ".slo");
    tmax = sloFile_read(instancename);

    ///TIME LIMIT
    timelimit = atoi(argv[3]);

    fprintf(stdout, "Total number of exams: %d\n"
                    "Total students: %d\n"
                    "Number of time slots: %d\n"
                    "Time limit set to: %d minutes\n\n", nexams, nstudents+1, tmax, timelimit/60);

    strcpy(instancename, argv[1]);
    strcat(instancename, "_OMAAL_group18.sol");

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
    bsol = malloc(tmax*sizeof(Solution*));
    sol = malloc((tmax+1)*sizeof(Solution)); // i generate an extra time slot for exams i can't schedule
    for(i=0; i<tmax+1; i++){
        sol[i].e = malloc(nexams*sizeof(int)); // i allocate for nexams to avoid realloc
        sol[i].dim = nexams;
        sol[i].currpos = 0;
    }

    fprintf(stdout, "Searching feasible solution...\n");
    feasible_search(sol, bsol, instancename, conflicts, nexams, nstudents, tmax);

    fprintf(stdout, "[+] Simulated Annealing...\n");
    //local_search(sol, bsol, instancename, conflicts, nstudents, nexams, tmax);
    simulated_annealing(sol, bsol, instancename, conflicts, nexams, nstudents, tmax);

    fprintf(stdout, "Best obj is: %f\n", obj);

    ///DEALLOCATION AND END OF PROGRAM
    for(i=0; i<tmax; i++)
        free(sol[i].e);
    free(sol);
    free(bsol);
    free2d(table_schedule, nstudents);
    free2d(conflicts, nexams);

    return 0;
}

int exmFile_read(char *filename)
{
    FILE *fp;
    int nexams=0, examid=0, nstudconf=0;

    fprintf(stdout, "Reading file %s...\n", filename);
    fp = fopen(filename, "r");
    if(fp==NULL){
        fprintf(stderr, "Error: can't open file %s\n", filename);
        exit(-1);
    }

    //reading of .exm to know how many exams there are
    while(fscanf(fp, "%d %d", &examid, &nstudconf)!=EOF)
        nexams++;

    fclose(fp);
    fprintf(stdout, "Done\n\n");

    return nexams;
}

int **stuFile_read(char *filename, int *nstudents, int nexams)
{
    FILE *fp;
    int i=0, j=0, examid=0, tmp_dim=1000, **table_schedule;
    char studentid[BUF], student_tmp[BUF]=" ";

    fprintf(stdout, "Reading file %s...\n", filename);
    fp = fopen(filename, "r");
    if(fp==NULL){
        fprintf(stderr, "Error: can't open file %s\n", filename);
        exit(-2);
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
            (*nstudents)++;
        if((*nstudents)==tmp_dim){ // menage reallocation
            tmp_dim = tmp_dim*2;
            table_schedule = realloc(table_schedule, tmp_dim*sizeof(int*));
            for(i=(*nstudents); i<tmp_dim; i++){
                table_schedule[i] = malloc(nexams*sizeof(int));
                for(j=0; j<nexams; j++)
                    table_schedule[i][j]=0;
            }
        }
        table_schedule[(*nstudents)][examid-1]=1;
        strcpy(student_tmp, studentid);
    }

    fclose(fp);
    fprintf(stdout, "Done\n\n");

    return table_schedule;
}

int sloFile_read(char *filename)
{
    FILE *fp;
    int tmax=0;

    fprintf(stdout, "Reading file %s...\n", filename);
    fp = fopen(filename, "r");
    if(fp==NULL){
        fprintf(stderr, "Error: can't open file %s\n", filename);
        exit(-3);
    }

    //reading of .slo to know the maximum number of time slot
    fscanf(fp, "%d", &tmax);

    fclose(fp);
    fprintf(stdout, "Done\n\n");

    return tmax;
}

void feasible_search(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nexams, int nstudents, int tmax)
{
    int i=0, j=0;
    int *priority, *stexams;

    priority = calloc(nexams, sizeof(int)); // each indexes represent an exam and the value of that index represent the total number of conflicts between this exam and the others
    stexams = malloc(nexams*sizeof(int)); // it will contain a sorted number of indexes (each index represent an exam) based on the priority of the previous array

    for(i=0; i<nexams; i++){
        for(j=0; j<nexams; j++)
            if(conflicts[i][j]!=0)
                priority[i]++;
        stexams[i]=i;
    }
    quickSort(priority, stexams, 0, nexams-1); // decreasing sorting

    fprintf(stdout, "[+] Graph coloring on sorted priority array of exams...\n");
    graph_coloring_greedy(sol, conflicts, stexams, nexams, tmax);
    if(sol[tmax].currpos>0){ // if i have exams in my extra time slot i have to schedule those exams
        fprintf(stdout, "[+] Can't found it. Apllying Tabù Search with %d exams in extra time slot...\n", sol[tmax].currpos);
        if(tabu_search(sol, conflicts, nexams, tmax)==0)
            check_best_sol(sol, bsol, filename, conflicts, nstudents, tmax);
        else
            fprintf(stdout, "Aborting Tabù Search...\n\n");
    }
    else
        check_best_sol(sol, bsol, filename, conflicts, nstudents, tmax);

    free(priority);
    free(stexams);
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

    swap(&arr[i+1], &arr[high]);
    swap(&arr2[i+1], &arr2[high]);

    return (i+1);
}

void swap(int *a, int *b)
{
    int t=*a;
    *a=*b;
    *b=t;
}

void graph_coloring_greedy(Solution *sol, int **conflicts, int *stexams, int nexams, int tmax)
{
    int i=0, j=0, c=0, flag=0, *colors, *available;

    colors = malloc(nexams*sizeof(int));
    for(i=0; i<nexams; i++)
        colors[i]=-1; // 'i'=exam and colors[i]=assigned color, if=-1 not yet assigned

    available = calloc(tmax, sizeof(int)); // 'i'=color and available[i]=0 available, available[i]=1 unavailable

    sol[0].e[sol[0].currpos] = stexams[0]; // first color to first exam in priority
    sol[0].currpos++;
    colors[stexams[0]]=0;
    for(i=1; i<nexams; i++){ // stexams[i] is the exam i'm considering (priority array)
        for(j=0; j<nexams; j++){ // i search if my exam has conflict with other exams
            if(conflicts[stexams[i]][j]>0 && colors[j]>=0) // if there is a conflict and this exam 'j' has a color, so colors[j]!=-1
                available[colors[j]]=1; //set as unavailable
        }

        // find the first available color
        flag=0;
        for(c=0; c<tmax && flag==0; c++)
            if(available[c]==0)
                flag=1;
        c--;

        if(flag==0){ // if i can't find a color for that exam i put it in my extra time slot and i'll schedule it after
            sol[tmax].e[sol[tmax].currpos] = stexams[i];
            sol[tmax].currpos++;
        }
        else{ // assign the found color
            sol[c].e[sol[c].currpos] = stexams[i];
            sol[c].currpos++;
            colors[stexams[i]]=c;
        }

        // reset available colors
        for(c=0; c<tmax; c++)
            available[c]=0;
    }

    free(colors);
    free(available);
}

int tabu_search(Solution *sol, int **conflicts, int nexams, int tmax)
{
    int i=0, j=0, k=0, ts=-1, *tabulist, dimtabu=tmax, next=0;
    double confl_min=INT_MAX, confl_tmp=0;

    tabulist = malloc(dimtabu*sizeof(int));
    for(i=0; i<dimtabu; i++)
        tabulist[i]=-1; // '-1' means empty

    for(i=0, k=0; i<sol[tmax].currpos; i++, k++){ // i use this cycle to take exams i put in the extra time slot
        confl_min=INT_MAX;
        ts=-1;
        for(j=0; j<tmax; j++){ // 'j' is the hypothetical time slot in which i want to put my exam sol[tmax].e[i]
            if(is_forbidden(sol, conflicts, sol[tmax].e[i], j, tabulist, dimtabu)==0){
                confl_tmp = confl_exams(sol, sol[tmax].e[i], j, conflicts); // i calculate for that time slot the number of conflicts
                if(confl_tmp<confl_min){ // if this 'confl' is lower than the previous i remember the time slot in which my 'confl' it the lowest
                    confl_min = confl_tmp;
                    ts = j;
                }
            }
        }
        if(ts==-1) // all moves are forbidden
            return 1;
        if(k==TSI) // i've reached the max allowed iteration
            return 1;
        update_tabu(sol, sol[tmax].e[i], ts, conflicts, tabulist, &next, dimtabu, nexams, tmax);
    }

    sol[tmax].currpos=0;

    free(tabulist);

    return 0;
}

int is_forbidden(Solution *sol, int **conflicts, int exam, int timeslot, int *tabulist, int dimtabu)
{
    int i=0, j=0;

    for(i=0; i<dimtabu; i++){
        if(tabulist[i]!=-1){
            for(j=0; j<sol[timeslot].currpos; j++){
                if(tabulist[i]==sol[timeslot].e[j] && conflicts[exam][tabulist[i]]>0){
                    // search for each exam in the tabulist if it is presents in the timeslot i'm considering
                    // if it is presents and it has conflict with exam i want to schedule return
                    return 1;
                }
            }
        }
    }

    return 0;
}

double confl_exams(Solution *sol, int exam, int timeslot, int **conflicts)
{
    int i=0, confl=0;

    for(i=0; i<sol[timeslot].currpos; i++){
        if(conflicts[exam][sol[timeslot].e[i]]>0)
            confl += conflicts[exam][sol[timeslot].e[i]];
    }

    return confl;
}

void update_tabu(Solution *sol, int exam, int ts, int **conflicts, int *tabulist, int *next, int dimtabu, int nexams, int tmax)
{
    schedule_exam(sol, conflicts, exam, ts, nexams, tmax); // i schedule my exam in time slot 'ts' which has the lowest temporary 'confl'

    tabulist[(*next)]=exam; // save move as tabù
    (*next)++;
    if((*next)>=dimtabu)
        (*next)=0;
}

int schedule_exam(Solution *sol, int **conflicts, int exam, int ts, int nexams, int tmax)
{
    int i=0, j=0, nconfl=0, flag=0, *idel;

    idel = malloc(nexams*sizeof(int));

    for(i=0; i<sol[ts].currpos; i++){ // searching for conflicting exams with exam i want to schedule in time slot 'ts'
        if(conflicts[exam][sol[ts].e[i]]>0){
            sol[tmax].e[sol[tmax].currpos] = sol[ts].e[i]; // put conflicting exam in the extra time slot
            sol[tmax].currpos++;

            if(sol[tmax].currpos>=sol[tmax].dim){ // i need to realloc sol[tmax].e, because of it was allocate for 'nexams'
                sol[tmax].dim = sol[tmax].dim * 2;
                sol[tmax].e = realloc(sol[tmax].e, sol[tmax].dim*sizeof(int));
            }

            sol[ts].e[i] = -1; // delete exam from that time slot
            idel[nconfl]=i; // remember index corresponding delete exam
            nconfl++; // count how many conflicting exams there are with my exam
        }
    }

    if(nconfl>0){
        for(i=0; i<sol[ts].currpos && flag==0; i++){
            if(sol[ts].e[sol[ts].currpos-1-i]!=-1){ // from the last value of sol[ts].e[last] search for a not delete exam
                sol[ts].e[idel[j]] = sol[ts].e[sol[ts].currpos-1-i]; // put it in the first position contains delete exam
                j++;
                if(j==nconfl) // i replaced all delete exam
                    flag = 1;
            }
        }
    }

    sol[ts].currpos = sol[ts].currpos-nconfl+1; // resize 'currpos' of time slot 'ts'
    sol[ts].e[sol[ts].currpos-1] = exam; // add exam i want to schedule in time slot 'ts'

    free(idel);

    return nconfl;
}

void simulated_annealing(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nexams, int nstudents, int tmax)
{
    struct timeb now;
    int i=0, j=0;
    double t0=20000, t=t0, nreset=5, n=1;

    ftime(&now);

    while((int)((now.time-startTime.time))<timelimit){
        for(i=0; i<n; i++){
            // Simple Searching Neighborhood (SSN)
            neighborhood_ssn(sol, bsol, filename, conflicts, nstudents, nexams, tmax, t);

            // Swapping Neighborhood (SWN)
            neighborhood_swn(sol, bsol, filename, conflicts, nstudents, nexams, tmax, t);
        }

        t = cooling_schedule(t, i);

        if(t<0.0001){ // t->0 => local minimum
            t=t0; // reset temperature to move in other feasible space
            j++;
            if(j>=nreset){ // after n-reset the temperature and the size of the neighborhood is increased
                j=0;
                t0=t0*2.5;
                nreset+=0.05;
                n+=0.1;
            }
            local_search(sol, bsol, filename, conflicts, nstudents, nexams, tmax); // trying to improve the actual solution
        }

        ftime(&now);
    }
}

double cooling_schedule(double temperature, int iter)
{
    //temperature = temperature*0.9;
    temperature = temperature/(1+500*temperature);

    return  temperature;
}

double probabilty(double new_obj, double old_obj, double temperature)
{
    double e=2.71828183, p = pow(e, -((new_obj-old_obj)/temperature)); // p = exp^(-(F(x_new)-F(x_old))/T)

    return p;
}

void neighborhood_ssn(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nstudents, int nexams, int tmax, double temperature)
{
    int i=0, exam=0, ets=0, epos=0, ts=-1, flag=0, max=0;
    double obj_tmp=0, p=0, prand=0;

    while(ts==-1 && max<nexams){
        exam = rand() % nexams;
        flag=0;
        for(i=0; i<nexams && flag==0; i++)
            if(conflicts[exam][i]>0) // check if 'exam' has at least one conflict
                flag=1;
        if(flag==1)
            ts = search_timeslot(sol, conflicts, exam, tmax, &ets, &epos);
        max++;
    }
    if(ts==-1) {
        //printf("ciao1 %d\n", max);
        return;
    }

    sol[ts].e[sol[ts].currpos] = exam; // scheduling 'exam' in time slot 'ts'
    sol[ts].currpos++;
    sol[ets].e[epos] = sol[ets].e[sol[ets].currpos-1]; // deleting 'exam' from its previous time slot
    sol[ets].currpos--;

    if((obj_tmp=check_best_sol(sol, bsol, filename, conflicts, nstudents, tmax))>obj){
        // solution is not better from the previous and i need to do probability calculation
        p = probabilty(obj_tmp, obj, temperature);
        prand = (double)(rand() % 2);
        if(prand>p){ // i need to backtrack the scheduled exam
            sol[ts].currpos--; // deleting 'exam' from time slot 'ts'
            sol[ets].e[sol[ets].currpos] = exam; // rescheduling 'exam' in its previous time slot
            sol[ets].currpos++;
        }
    }
}

void neighborhood_swn(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nstudents, int nexams, int tmax, double temperature)
{
    int i=0, c=0, exam1=0, exam2=0, ets1=0, ets2=0, epos1=0, epos2=0, flag=0, tmp=0, max=0;
    double obj_tmp=0, p=0, prand=0;

    while(flag==0 && max<nexams){
        exam1 = rand() % nexams;
        c=0;
        for(i=0; i<nexams && c==0; i++)
            if(conflicts[exam1][i]>0) // check if 'exam1' has at least one conflict
                c=1;
        if(c==1){
            search_exam_pos(sol, exam1, &ets1, &epos1, tmax);
            exam2 = rand() % nexams;
            while(exam2==exam1)
                exam2 = rand() % nexams;
            for(i=0; i<nexams && c==1; i++)
                if(conflicts[exam2][i]>0) // check if 'exam2' has at least one conflict
                    c=2;
            if(c==2){
                search_exam_pos(sol, exam2, &ets2, &epos2, tmax);
                if(ets1!=ets2)
                    if(has_conflicts(sol, conflicts, exam1, ets2)==0 && has_conflicts(sol, conflicts, exam2, ets1)==0)
                        flag = 1;
            }
        }
        max++;
    }
    if(flag==0) {
        //printf("ciao2 %d\n", max);
        return;
    }

    tmp = sol[ets1].e[epos1];
    sol[ets1].e[epos1] = sol[ets2].e[epos2]; // scheduling 'exam2' in time slot 'ets1'
    sol[ets2].e[epos2] = tmp; // scheduling 'exam1' in time slot 'ets2'

    if((obj_tmp=check_best_sol(sol, bsol, filename, conflicts, nstudents, tmax))>obj){
        // solution is not better from the previous and i need to do probability calculation
        p = probabilty(obj_tmp, obj, temperature);
        prand = (double)(rand() % 2);
        if(prand>p){ // i need to backtrack the scheduled exam
            tmp = sol[ets1].e[epos1];
            sol[ets1].e[epos1] = sol[ets2].e[epos2]; // scheduling 'exam1' in time slot 'ets2'
            sol[ets2].e[epos2] = tmp; // scheduling 'exam2' in time slot 'ets1'
        }
    }
}

void local_search(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nstudents, int nexams, int tmax)
{
    int i=0, j=0, k=0, t=0, tmp=0;
    int best_ts=0, *order_t, *used_t, flag=0, end=0;
    double obj_tmp=0;

    order_t = malloc(tmax*sizeof(int));
    for(i=0; i<tmax; i++)
        order_t[i] = i; // natural order of timeslots

    used_t = calloc(tmax, sizeof(int)); // 0 if time slot 'i' is free, 1 if it is used_t

    for(t=0; t<tmax; t++){
        for(i=0; i<tmax; i++){
            for(j=0; j<tmax; j++){
                if(used_t[j]==0){
                    swap_timeslots(sol, i, j);
                    if((obj_tmp=check_best_sol(sol, bsol, filename, conflicts, nstudents, tmax))==obj)
                        best_ts = j;
                    swap_timeslots(sol, j, i);
                }
            }
            swap_timeslots(sol, i, best_ts);
            used_t[i]=1;
        }

        for(i=0; i<tmax; i++) // resetting values
            used_t[i] = 0;
    }

    for(i=0; i<tmax; i++)
        used_t[i] = 0;

    for(t=0; t<tmax; t++){
        for(i=0; i<tmax && end==0; i++){
            flag=0;
            for(j=0; j<tmax && flag==0; j++){
                if(used_t[j]==0) // choose time slot with the priority
                    flag = 1;
            }
            if(flag==0)
                end = 1;
            else
                j--; // 'j' is the time slots can be swapped

            best_ts=-1;
            for(k=0; k<tmax && flag==1; k++){
                swap_timeslots(sol, j, k);
                if((obj_tmp=check_best_sol(sol, bsol, filename, conflicts, nstudents, tmax))==obj)
                    best_ts = k;
                swap_timeslots(sol, k, j);
            }

            if(best_ts!=-1){
                swap_timeslots(sol, j, best_ts);
                tmp = order_t[j];
                order_t[j] = order_t[best_ts];
                order_t[best_ts] = tmp;
                used_t[order_t[j]] = 1;
            }
        }

        for(i=0; i<tmax; i++){ // resetting values
            used_t[i] = 0;
            order_t[i] = i; // natural order of timeslots
            end=0;
        }
    }

    free(order_t);
    free(used_t);
}

void swap_timeslots(Solution *sol, int ts1, int ts2)
{
    int i=0, *tmp, tmp_size=0, dim=0;

    if(sol[ts1].currpos>sol[ts2].currpos)
        dim = sol[ts1].currpos;
    else
        dim = sol[ts2].currpos;
    tmp = malloc(dim*sizeof(int));

    for(i=0; i<sol[ts1].currpos; i++)
        tmp[i] = sol[ts1].e[i];
    tmp_size = sol[ts1].currpos;

    for(i=0; i<sol[ts2].currpos; i++)
        sol[ts1].e[i] = sol[ts2].e[i]; // copying 'ts2' in 'ts1'
    sol[ts1].currpos = sol[ts2].currpos;

    for(i=0; i<tmp_size; i++)
        sol[ts2].e[i] = tmp[i]; // copying 'ts1' in 'ts2'
    sol[ts2].currpos = tmp_size;

    free(tmp);
}

int search_timeslot(Solution *sol, int **conflicts, int exam, int tmax, int *ets, int *epos)
{
    int i=0, j=0, flag=0, available=0, nts=0, ts=-1, *notgranted;

    notgranted = calloc(tmax, sizeof(int)); // 0 if time slot is available, 1 otherwise

    for(i=0; i<tmax; i++){
        for(j=0; j<sol[i].currpos; j++){
            if(sol[i].e[j]==exam){
                notgranted[i] = 1;
                (*ets) = i; // save time slot where is 'exam'
                (*epos) = j; // save position of 'exam' in time slot
            }
            if(conflicts[exam][sol[i].e[j]]>0)
                notgranted[i] = 1;
        }
    }

    for(i=0; i<tmax; i++) // check if there is at least one available time slot
        if(notgranted[i]==0)
            available++;

    if(available==0)
        return -1; // no available time slot

    available = rand() % available;
    for(i=0; i<tmax && flag==0; i++){ // random choice of the available time slots
        if(notgranted[i]==0){
            if(nts==available) {
                flag = 1;
                ts = i;
            }
            nts++;
        }
    }

    free(notgranted);

    return ts;
}

void search_exam_pos(Solution *sol, int exam, int *ets, int *epos, int tmax)
{
    int i=0, j=0, flag=0;

    for(i=0; i<tmax && flag==0; i++){
        for(j=0; j<sol[i].currpos && flag==0; j++){
            if(sol[i].e[j]==exam){
                flag=1;
                (*ets) = i; // save time slot where 'exam' is located
                (*epos) = j; // save position of 'exam' in time slot
            }
        }
    }
}

int has_conflicts(Solution *sol, int **conflicts, int exam, int ts)
{
    int i=0;

    for(i=0; i<sol[ts].currpos; i++)
        if(conflicts[exam][sol[ts].e[i]]>0)
            return 1;

    return 0;
}

double check_best_sol(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nstudents, int tmax)
{
    int i=0, j=0, k=0, t=0;
    double obj_tmp=0;

    ///MIN OBJ 2^(5-i)*Ne,e'/|S|
    for(i=0; i<tmax-1; i++){ //cycle in time slot sol[0, ..., tmax-1]
        for(k=0; k<sol[i].currpos; k++){ //cycle in sol[i].e[0, ..., k] - exams i scheduled in time slot 'i'
            for(j=i; j<=i+5 && j<tmax; j++){ //after 5 i don't pay no kind of penalty - cycle to compare time slot 'i' with time slot 'j'
                if(i==j){ //if i'm considering the same time slot i must avoid to compare eX - eY and then eY - eX
                    for(t=k+1; t<sol[j].currpos; t++) //so i start from the same value+1
                        obj_tmp += conflicts[sol[i].e[k]][sol[j].e[t]]*power(2, 5-(j-i));
                }
                else{ //otherwise i have to start from zero to compare all exams
                    for(t=0; t<sol[j].currpos; t++)
                        if(sol[i].e[k]!=sol[j].e[t])
                            obj_tmp += conflicts[sol[i].e[k]][sol[j].e[t]]*power(2, 5-(j-i));
                }
            }
        }
    }
    obj_tmp = obj_tmp/nstudents;

    if(obj_tmp<obj){
        fprintf(stdout, "[!] New obj found. NEW: %f\n", obj_tmp);
        obj = obj_tmp;

        save_sol(sol, bsol, tmax); // updating new best sol
        print_bestsol(bsol, filename, tmax);
    }

    return obj_tmp;
}

int power(int base, int exp)
{
    int i=0, res=1;

    if(exp==0)
        return 1;

    for(i=0; i<exp; i++)
        res *= base;

    return res;
}

void save_sol(Solution *sol, Solution **psol, int tmax)
{
    int i=0;

    for(i=0; i<tmax; i++)
        psol[i] = &sol[i];
}

void restore_sol(Solution *sol, Solution **psol, int tmax)
{
    int i=0, j=0;

    for(i=0; i<tmax; i++){
        sol[i].currpos = psol[i]->currpos;
        for(j=0; j<psol[i]->currpos; j++){
            sol[i].e[j] = psol[i]->e[j];
        }
    }
}

void print_bestsol(Solution **bsol, char *filename, int tmax)
{
    FILE *fp;
    int i=0, j=0;

    fp = fopen(filename, "w");

    for(i=0; i<tmax; i++)
        for(j=0; j<bsol[i]->currpos; j++)
            fprintf(fp, "%d %d\n", bsol[i]->e[j]+1, i+1);

    fprintf(stdout, "# New best solution printed\n");

    fclose(fp);
}

void free2d(int **matr, int n)
{
    int i=0;

    for(i=0; i<n; i++)
        free(matr[i]);

    free(matr);
}
