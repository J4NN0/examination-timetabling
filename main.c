#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <time.h>
#include <sys/timeb.h>

#define N (3+1) // number of parameters have to receive from command line
#define BUF 100 // buffer size for string
#define MSMI 200 // multi-start metaheuristics
#define TSI 500000 // max iteration for tabu search
#define MAX_INITIAL_SOLUTION 200

/* It's receives from the command line 3 files, respectively: instancename -t timelimit
 * Instance files: instanceXX.stu instanceXX.exm instanceXX.slo */

typedef struct solution{
    int *e; // vectors says me if exam 'eX' is scheduled in the time slot 'i'. EX: sol[i].e[j]=1 mens exam 'e1' is scheduled in the time slot 'i'
    int dim; // allocation
    int currpos; // current position (real dim) of 'e'
    int timeslot;
} Solution;

Solution ***initialSolutions;
int *examWithConflicts;
int nOfInitialSol = 0;

int exam_local_search(Solution *sol, int **conflicts, int nexams, int nstudents, int tmax);
void local_search(Solution *sol, int **conflicts, int nexams, int nstudents, int tmax, char instancename[], Solution **bsol);

int exmFile_read(char *filename);
int **stuFile_read(char *filename, int *nstudents, int nexams);
int sloFile_read(char *filename);

int neighborhood_sts(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nstudents, int nexams, int tmax,
                     double temperature, int exam);

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

void simulated_annealing(int choice, Solution *sol, Solution **bsol, char *filename, int **conflicts, int nexams, int nstudents, int tmax);
void save_sol(Solution *sol, Solution **psol, int tmax);
void restore_sol(Solution *sol, Solution **psol, int tmax);
double cooling_schedule(double temperature, int iter);
int neighborhood_ssn(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nstudents, int nexams, int tmax, double temperature, int exam);
int neighborhood_swn(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nstudents, int nexams, int tmax, double temperature, int exam1);
int neighborhood_s3wn(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nstudents, int nexams, int tmax, double temperature, int exam1);
void search_exam_pos(Solution *sol, int exam, int *ets, int *epos, int tmax);
double probabilty(double new_obj, double old_obj, double temperature);

int neighborhood_local_search(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nstudents, int nexams, int tmax,
                              double temperature, int exam);

double check_best_sol(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nstudents, int tmax);
int power(int base, int exp);
void print_bestsol(Solution **bsol, char *filename, int tmax);

void free2d(int **matr, int n);

struct timeb startTime;
double obj=INT_MAX;
int timelimit=0;

int main(int argc, char **argv)
{
    ftime(&startTime);
    srand(time(NULL));

    initialSolutions = malloc(sizeof(Solution**)*MAX_INITIAL_SOLUTION);

    Solution *sol, **bsol;
    int **table_schedule, **conflicts; //from instanceXX.stu and instanceXX.exm
    int tmax=0; //from instanceXX.slo
    int i=0, j=0, k=0, nexams=0, nstudents=-1, nstudconf=0;
    char instancename[BUF];

    if(argc!=N){
        fprintf(stderr, "Error. Usage: <%s> <instancename> -t <timelimit>\n", argv[0]);
        exit(0);
    }

    ///.EXM FILE
    strcpy(instancename, argv[1]);
    strcat(instancename, ".exm");
    nexams = exmFile_read(instancename);

    examWithConflicts = calloc(nexams, sizeof(int));

    ///.STU FILE
    strcpy(instancename, argv[1]);
    strcat(instancename, ".stu");
    table_schedule = stuFile_read(instancename, &nstudents, nexams);

    ///.SLO FILE
    strcpy(instancename, argv[1]);
    strcat(instancename, ".slo");
    tmax = sloFile_read(instancename);

    fprintf(stdout, "Total number of exams: %d\n"
                    "Total students: %d\n"
                    "Number of time slots: %d\n\n", nexams, nstudents+1, tmax);

    strcpy(instancename, argv[1]);
    strcat(instancename, "_OMAAL_group18.sol");
    timelimit = atoi(argv[3]);

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
            examWithConflicts[j] = 1;
            examWithConflicts[k] = 1;
        }
    }

    ///SOLUTION OF THE PROBLEM
    bsol = malloc(tmax*sizeof(Solution*));
    sol = malloc((tmax+1)*sizeof(Solution)); // i generate an extra time slot for exams i can't schedule
    for(i=0; i<tmax+1; i++){
        sol[i].e = malloc(nexams*sizeof(int)); // i allocate for nexams to avoid realloc
        sol[i].dim = nexams;
        sol[i].currpos = 0;
        sol[i].timeslot = i;
    }

    fprintf(stdout, "Searching several feasible solution...\n");
    feasible_search(sol, bsol, instancename, conflicts, nexams, nstudents, tmax);

//    fprintf(stdout, "Simulated Annealing... TIMESLOTS\n");
//    for (i=0; i<nOfInitialSol; ++i) {
//        srand(time(NULL));
//
////        local_search(sol,conflicts,nexams,nstudconf,tmax,instancename,bsol);
//        printf("START %d/%d\n", i,nOfInitialSol);
//        restore_sol(sol, initialSolutions[i], tmax); // put in sol the actual best solution
//        simulated_annealing(3,sol, bsol, instancename, conflicts, nexams, nstudents, tmax);
//        printf("STOP %d/%d\n", i,nOfInitialSol);
//        save_sol(sol,initialSolutions[i],tmax);
////        local_search(sol,conflicts,nexams,nstudconf,tmax,instancename,bsol);
//    }

//    fprintf(stdout, "Simulated Annealing... EXAMS\n");
//    for (i=0; i<nOfInitialSol; ++i) {
//        srand(time(NULL));
//
//        printf("START %d/%d\n", i,nOfInitialSol);
//        restore_sol(sol, initialSolutions[i], tmax); // put in sol the actual best solution
//        simulated_annealing(4,sol, bsol, instancename, conflicts, nexams, nstudents, tmax);
//        printf("STOP %d/%d\n", i,nOfInitialSol);
////        save_sol(sol,initialSolutions[i],tmax);
//    }

    fprintf(stdout, "Best obj is: %f\n", obj);

    ///DEALLOCATION AND END OF PROGRAM
    for(i=0; i<tmax; i++)
        free(sol[i].e);
    free(sol);
    free(bsol);
    free2d(table_schedule, nstudents);
    free2d(conflicts, nexams);

    for(i=0; i<nOfInitialSol; i++)
        free(initialSolutions[i]);

    free(examWithConflicts);

    return 0;
}

void local_search(Solution *sol, int **conflicts, int nexams, int nstudents, int tmax, char instancename[], Solution **bsol) {
    int i;

//    fprintf(stdout, "Local searching...\n");
    for (i = 0; i < 1; ++i) {
        if(exam_local_search(sol, conflicts, nexams, nstudents, tmax)==0) {
            check_best_sol(sol, bsol, instancename, conflicts, nstudents, tmax);
        }
    }
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

int exam_local_search(Solution *sol, int **conflicts, int nexams, int nstudents, int tmax)
{
    int examId, j, k;
    int *slotsToAvoid = calloc(tmax, sizeof(int));

    for (j = 0; j < 1000; j++) {
        if (examWithConflicts[j] == 1){
            examId = rand() % nexams;   //id of random exam
            break;
        }
    }
    if (j==1000) {
        free(slotsToAvoid);
        return 1;
    }

    int examSlot;
    int examIndex;

    for (j = 0; j < tmax; j++){
        for (k = 0; k < sol[j].currpos; k++){
            if (sol[j].e[k] == examId) {    //salvo slot e index dell'esame per dopo
                examSlot = j;
                examIndex = k;
                slotsToAvoid[j] = 1;
            }

            if (conflicts[examId][sol[j].e[k]]>0) {
                slotsToAvoid[j] = 1;
            }
        }
    }

    for (j = 0; j < 1000; j++) {
        int randSlot = rand() % tmax;
        if (slotsToAvoid[randSlot] == 0){
            sol[randSlot].e[sol[randSlot].currpos] = examId;
            sol[randSlot].currpos++;
            break;
        }
    }
    if (j == 1000){
        free(slotsToAvoid);
        return 1;
    }

    sol[examSlot].e[examIndex] = sol[examSlot].e[sol[examSlot].currpos-1];
    sol[examSlot].currpos--;

    free(slotsToAvoid);

    return 0;
}

void feasible_search(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nexams, int nstudents, int tmax)
{
    int i=0, j=0, k=0, swapn=nexams/2, swi=0, swj=0, tmp=0;
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

    fprintf(stdout, "[+] Multi-start metaheuristics...\n");
    for(i=0; i<MSMI; i++){
        for(j=0; j<tmax+1; j++) // reset 'sol'
            sol[j].currpos=0;

        for(k=0; k<swapn; k++){
            swi = rand() % nexams;
            swj = rand() % nexams;

            tmp = stexams[swi];
            stexams[swi] = stexams[swj];
            stexams[swj] = tmp;
        }

        graph_coloring_greedy(sol, conflicts, stexams, nexams, tmax);
        if(sol[tmax].currpos>0){ // if i have exams in my extra time slot i have to schedule those exams
            if(tabu_search(sol, conflicts, nexams, tmax)==0) {
                check_best_sol(sol, bsol, filename, conflicts, nstudents, tmax);

                if (nOfInitialSol < MAX_INITIAL_SOLUTION) {
                    initialSolutions[nOfInitialSol] = malloc(sizeof(Solution*)*tmax);
//                    save_sol(sol, initialSolutions[nOfInitialSol++], tmax);

                    printf("START %d\n", nOfInitialSol);
//                    restore_sol(sol, initialSolutions[nOfInitialSol-1], tmax); // put in sol the actual best solution
                    simulated_annealing(3,sol, bsol, filename, conflicts, nexams, nstudents, tmax);
                    simulated_annealing(4,sol, bsol, filename, conflicts, nexams, nstudents, tmax);
                    printf("STOP %d\n", nOfInitialSol);
//                    local_search(sol,conflicts,nexams,nstudents,tmax,filename,bsol);
                    save_sol(sol,initialSolutions[nOfInitialSol++],tmax);
                }
            }else
                fprintf(stdout, "Aborting Tabù Search...\n\n");
        }
        else {
            if (nOfInitialSol < MAX_INITIAL_SOLUTION) {
                initialSolutions[nOfInitialSol] = malloc(sizeof(Solution*)*tmax);
//                save_sol(sol, initialSolutions[nOfInitialSol++], tmax);

                printf("START %d\n", nOfInitialSol);
//                restore_sol(sol, initialSolutions[nOfInitialSol-1], tmax); // put in sol the actual best solution
                simulated_annealing(3,sol, bsol, filename, conflicts, nexams, nstudents, tmax);
                simulated_annealing(4,sol, bsol, filename, conflicts, nexams, nstudents, tmax);
                printf("STOP %d\n", nOfInitialSol);
//                local_search(sol,conflicts,nexams,nstudents,tmax,filename,bsol);
                save_sol(sol,initialSolutions[nOfInitialSol++],tmax);
            }
//            check_best_sol(sol, bsol, filename, conflicts, nstudents, tmax);
        }
    }

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
                if(tabulist[i]==sol[timeslot].e[j] && conflicts[tabulist[i]][exam]>0){
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

void simulated_annealing(int choice, Solution *sol, Solution **bsol, char *filename, int **conflicts, int nexams, int nstudents, int tmax)
{
    Solution **tsol;
    struct timeb now;
    int i=0, exam=0;
//    double t=70000;
    double t=10000;

    tsol = malloc(tmax*sizeof(Solution*)); // temporary sol, to save the current sol and backtrack sol if necessary

    ftime(&now);

    while(i < 10000){
        if (now.time-startTime.time >= timelimit)
            exit(0);
        i++;

//        exam = rand() % nexams;
//        choice = rand() % 10;
//        choice = 1;

        save_sol(sol, tsol, tmax); // save actual solution in case of backtrack
        if(choice==0){ // Simple Searching Neighborhood (SSN)
            exam = rand() % nexams;
            if(neighborhood_ssn(sol, bsol, filename, conflicts, nstudents, nexams, tmax, t, exam)==1)
                restore_sol(sol, tsol, tmax);
        }else if(choice==1){ // Swapping Neighborhood (SWN)
            exam = rand() % nexams;
            if(neighborhood_swn(sol, bsol, filename, conflicts, nstudents, nexams, tmax, t, exam)==1)
                restore_sol(sol, tsol, tmax);
        }else if(choice==2){ // Simple Searching and Swapping Neighborhoods (S3WN)
            exam = rand() % nexams;
            if(neighborhood_s3wn(sol, bsol, filename, conflicts, nstudents, nexams, tmax, t, exam)==1)
                restore_sol(sol, tsol, tmax);
        }else if (choice==3){ // Simple Searching and Swapping Neighborhoods (S3WN)
            neighborhood_sts(sol,bsol,filename,conflicts,nstudents,nexams,tmax,t,exam);
        }else if (choice==4) {
            if (neighborhood_local_search(sol, bsol, filename, conflicts, nstudents, nexams, tmax, t, exam)==1)
                restore_sol(sol, tsol, tmax);

        }
        t = cooling_schedule(t, i);
        //printf("Temperatue %f - Iter %d\n", t, i);
        ftime(&now);
    }

    free(tsol);
}

int neighborhood_local_search(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nstudents, int nexams, int tmax,
                              double temperature, int exam) {
    double obj_tmp=0, p=0, prand=0;

    exam_local_search(sol,conflicts,nexams,nstudents,tmax);

    if((obj_tmp=check_best_sol(sol, bsol, filename, conflicts, nstudents, tmax))>obj){
        // solution is not better from the previous and i need to do probability calculation
        p = probabilty(obj_tmp, obj, temperature);
        prand = (float)rand()/(float)(RAND_MAX);
        if(prand>p) {// i need to backtrack the scheduled exam
            return 1;
        }
    }

    return 0;
}

int neighborhood_sts(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nstudents, int nexams, int tmax,
                     double temperature, int exam)
{
    int ts1 = rand()%tmax, ts2 = rand()%tmax;
    while (ts1 == ts2) {
        ts2 = rand()%tmax;
    }

    double obj_tmp=0, p=0, prand=0;

    sol[ts1].timeslot = ts2;
    sol[ts2].timeslot = ts1;

    if((obj_tmp=check_best_sol(sol, bsol, filename, conflicts, nstudents, tmax))>obj){
        // solution is not better from the previous and i need to do probability calculation
        p = probabilty(obj_tmp, obj, temperature);
        prand = (float)rand()/(float)(RAND_MAX);
        if(prand>p) {// i need to backtrack the scheduled exam
            sol[ts1].timeslot = ts1;
            sol[ts2].timeslot = ts2;
            return 1;
        }
    }

    return 0;
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

double cooling_schedule(double temperature, int iter)
{
    //temperature = temperature/(log(iter+1)); // T = To/(ln(I+1))
    temperature = temperature*0.9999;

    //t = αt
    //α = 1 − (ln(t) − ln(tf))/Nmove
    //α = 0.9 || 0.99

    return  temperature;
}

double probabilty(double new_obj, double old_obj, double temperature)
{
    double p = exp((old_obj-new_obj)/temperature); // p = exp^(-(F(x_new)-F(x_old))/T)

    //p = 1/(1+pow(e, ((new_tmp-old_obj)/temperature))); // p = 1/(1+exp^((F(x_old)-F(x_new))/T)

    return p;
}

int neighborhood_ssn(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nstudents, int nexams, int tmax,
                     double temperature, int exam)
{
    int ets=0, epos=0, ts=rand()%tmax;
    double obj_tmp=0, p=0, prand=0;

    search_exam_pos(sol, exam, &ets, &epos, tmax);
    sol[ets].e[epos] = sol[ets].e[sol[ets].currpos-1]; // unplanned 'exam' from its previous time slot
    sol[ets].currpos--;

    // scheduling 'exam' in time slot 'ts'
    if(schedule_exam(sol, conflicts, exam, ts, nexams, tmax)>0) // if i put exams in extra time slot i need to call tabù
        if(tabu_search(sol, conflicts, nexams, tmax)==1)
            printf("Aborting tabu search...\n");

    if((obj_tmp=check_best_sol(sol, bsol, filename, conflicts, nstudents, tmax))>obj){
        // solution is not better from the previous and i need to do probability calculation
        p = probabilty(obj_tmp, obj, temperature);
        prand = (float)rand()/(float)(RAND_MAX);
        if(prand>p) // i need to backtrack the scheduled exam
            return 1;
    }

    return 0;
}

int neighborhood_swn(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nstudents, int nexams, int tmax,
                     double temperature, int exam1)
{
    int ets1=0, ets2=0, epos=0, exam2=rand()%nexams;
    double obj_tmp=0, p=0, prand=0;

    while(exam2==exam1)
        exam2=rand() % nexams;

    search_exam_pos(sol, exam1, &ets1, &epos, tmax);
    sol[ets1].e[epos] = sol[ets1].e[sol[ets1].currpos-1]; // unplanned 'exam' from its previous time slot
    sol[ets1].currpos--;

    search_exam_pos(sol, exam2, &ets2, &epos, tmax);
    sol[ets2].e[epos] = sol[ets2].e[sol[ets2].currpos-1]; // unplanned 'exam2' from its previous time slot
    sol[ets2].currpos--;

    // scheduling 'exam1' in time slot 'ets2'
    if(schedule_exam(sol, conflicts, exam1, ets2, nexams, tmax)>0) // if i put exams in extra time slot i need to call tabù
        if(tabu_search(sol, conflicts, nexams, tmax)==1)
            printf("Aborting tabu search...\n");

    // scheduling 'exam2' in time slot 'ets1'
    if(schedule_exam(sol, conflicts, exam2, ets1, nexams, tmax)>0) // if i put exams in extra time slot i need to call tabù
        if(tabu_search(sol, conflicts, nexams, tmax)==1)
            printf("Aborting tabu search...\n");

    if((obj_tmp=check_best_sol(sol, bsol, filename, conflicts, nstudents, tmax))>obj){
        // solution is not better from the previous and i need to do probability calculation
        p = probabilty(obj_tmp, obj, temperature);
        prand = (float)rand()/(float)(RAND_MAX);
        if(prand>p) // i need to backtrack the scheduled exam
            return 1;
    }

    return 0;
}

int neighborhood_s3wn(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nstudents, int nexams, int tmax,
                      double temperature, int exam1)
{
    int ets=0, epos=0, exam2=rand()%nexams, ts1=rand()%tmax, ts2=rand()%tmax;
    double obj_tmp=0, p=0, prand=0;

    while(exam2==exam1)
        exam2 = rand() % nexams;

    while(ts1==ts2)
        ts2 = rand() % tmax;

    search_exam_pos(sol, exam1, &ets, &epos, tmax);
    sol[ets].e[epos] = sol[ets].e[sol[ets].currpos-1]; // unplanned 'exam1' from its previous time slot
    sol[ets].currpos--;

    search_exam_pos(sol, exam2, &ets, &epos, tmax);
    sol[ets].e[epos] = sol[ets].e[sol[ets].currpos-1]; // unplanned 'exam2' from its previous time slot
    sol[ets].currpos--;

    // scheduling 'exam1' in time slot 'ts1'
    if(schedule_exam(sol, conflicts, exam1, ts1, nexams, tmax)>0) // if i put exams in extra time slot i need to call tabù
        if(tabu_search(sol, conflicts, nexams, tmax)==1)
            printf("Aborting tabu search...\n");

    // scheduling 'exam2' in time slot 'ts2'
    if(schedule_exam(sol, conflicts, exam2, ts2, nexams, tmax)>0) // if i put exams in extra time slot i need to call tabù
        if(tabu_search(sol, conflicts, nexams, tmax)==1)
            printf("Aborting tabu search...\n");

    if((obj_tmp=check_best_sol(sol, bsol, filename, conflicts, nstudents, tmax))>obj){
        // solution is not better from the previous and i need to do probability calculation
        p = probabilty(obj_tmp, obj, temperature);
        prand = (float)rand()/(float)(RAND_MAX);
        if(prand>p) // i need to backtrack the scheduled exam
            return 1;
    }

    return 0;
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

double check_best_sol(Solution *sol, Solution **bsol, char *filename, int **conflicts, int nstudents, int tmax)
{
    int i=0, j=0, k=0, t=0;
    double obj_tmp=0;

    Solution *tmpSol = malloc(sizeof(Solution) * tmax);

    for (i=0; i<tmax; ++i) {
        for (j=0; j<tmax; ++j) {
            if (sol[j].timeslot == i) {
                tmpSol[i] = sol[j];
                break;
            }
        }
    }

    for (i=0; i<tmax; ++i) {
        sol[i] = tmpSol[i];
        sol[i].timeslot = i;
    }

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

        for(i=0; i<tmax; i++)
            bsol[i] = &sol[i]; // updating new best sol
        print_bestsol(bsol, filename, tmax);
    }

    free(tmpSol);

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
