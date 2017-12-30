#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#define N (3+1) // number of parameters have to receive from command line
#define BUF 10 // buffer size for string
#define MS 100 // multi-start metaheuristics
#define TS 1000000 // max iteration for tabu search

/* It's receives from the command line 3 files, respectively: instanceXX.stu instanceXX.exm instanceXX.slo */

typedef struct solution{
    int *e; // vectors says me if exam 'eX' is scheduled in the time slot 'i'. EX: sol[i].e[j]=1 mens exam 'e1' is scheduled in the time slot 'i'
    int dim; // allocation
    int currpos; // current position (real dim) of 'e'
} Solution;

void quickSort(int *arr, int *arr2, int low, int high);
int partition(int *arr, int *arr2, int low, int high);
void swap(int *a, int *b);

void exam_local_search(Solution *sol, Solution **psol, int **conflicts, int nexams, int nstudents, int tmax);
void feasible_search(Solution *sol, Solution **psol, int **conflicts, int *timeslot, int nexams, int nstudents, int tmax);
void graph_coloring_greedy(Solution *sol, int **conflicts, int *stexams, int nexams, int tmax);
int tabu_search(Solution *sol, int **conflicts, int nexams, int tmax, int nstudents);
int is_forbidden(Solution *sol, int **conflicts, int exam, int timeslot, int *tabulist, int tls);
float confl_exams(Solution *sol, int exam, int timeslot, int **conflicts, int tmax, int nstudents);
int schedule_exam(Solution *sol, int exam, int ts, int **conflicts, int *tabulist, int *next, int dimtabu, int nexams, int tmax);

void dens(Solution *sol, Solution **psol, int **conflicts, int nexams, int nstudents, int tmax);
void density_gen(int *density, int **conflicts, Solution *sol, int tmax);
void gauss_sorting(int *density, int *newdens, int *idcorr, int dim, int *mark);
int search_tmp_max(int *density, int *mark, int *id, int dim, int max_tmp);

void swap_freetimeslots(Solution *sol, Solution **psol, int **conflicts, int *timeslots, int nexams, int nstudents, int tmax, int freets);
void disp(int pos, int *valfreets, int **conflicts, Solution *sol, Solution **psol, int *mark, int *timeslots, int *freetimeslots, int nexams, int nstudents, int tmax, int freets);

//void perm(int pos, int **conflicts, Solution *sol, Solution **psol, int *timeslots, int *mark, int nexams, int nstudents, int tmax);

void check_best_sol(Solution *sol, Solution **psol, int **conflicts, int *timeslots, int nstudents, int tmax);
int power(int base, int exp);

void free2d(int **matr, int n);
void checksol(int **conflicts, Solution *sol, int tmax);

float obj=INT_MAX;

int main(int argc, char **argv)
{
    srand((unsigned) time(NULL));

    FILE *fp=NULL;
    Solution *sol, **psol;
    int **table_schedule=NULL, **conflicts=NULL; //from instanceXX.stu and instanceXX.exm
    int tmax=0; //from instanceXX.slo
    int i=0, j=0, k=0, nexams=0, nstudents=-1, tmp_dim=1000, examid=0, nstudconf=0, *timeslots;
    char buffer[BUF], studentid[BUF], student_tmp[BUF]=" ";

    if(argc!=N){
        fprintf(stderr, "Error. Usage: <%s> <instanceXX.stu> <instanceXX.exm> <instanceXX.slo>\n", argv[0]);
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
    sol = malloc((tmax+1)*sizeof(Solution)); // i generate an extra time slot for exams i can't schedule
    for(i=0; i<tmax+1; i++){
        sol[i].e = malloc(nexams*sizeof(int)); // i allocate for nexams to avoid realloc
        sol[i].dim = nexams;
        sol[i].currpos=0;
    }

    timeslots = malloc(tmax*sizeof(int)); // the real order of time slots
    for(i=0; i<tmax; i++)
        timeslots[i]=i; // at the beginning is from 0 to 'tmax', it will modified after

    fprintf(stdout, "Searching several feasible solution...\n");
    feasible_search(sol, psol, conflicts, timeslots, nexams, nstudents, tmax);

    for(i=0; i<tmax; i++)
        timeslots[i]=i; // at the beginning is from 0 to 'tmax', it will modified after

    fprintf(stdout, "Local searching...\n");
    for (i = 0; i < 100; ++i) {
        exam_local_search(sol, psol, conflicts, nexams, nstudents, tmax);
        check_best_sol(sol, psol, conflicts,timeslots, nstudents, tmax);
    }

    check_best_sol(sol, psol, conflicts,timeslots, nstudents, tmax);
    fprintf(stdout, "Best obj is: %.3f\n", obj);


//    fprintf(stdout, "Swapping time slot and searching for a better solution...\n");
//    perm(0, conflicts, sol, psol, timeslots, mark, nexams, nstudents, tmax);

    checksol(conflicts,sol,tmax);

    for (i=0;i<tmax;++i) {
        printf("%d:\t",i);
        for (j=0;j<sol[i].currpos;++j) {
            printf("%d ",sol[i].e[j]);
        }
        printf("\n");
    }

    ///DEALLOCATION AND END OF PROGRAM
    for(i=0; i<tmax; i++)
        free(sol[i].e);
    free(sol);
    free(psol);
    free2d(table_schedule, tmp_dim);
    free2d(conflicts, nexams);

    return 0;
}

void exam_local_search(Solution *sol, Solution **psol, int **conflicts, int nexams, int nstudents, int tmax) {
    int examId, j, k;
    int *slotsToAvoid = calloc((size_t) tmax, sizeof(int));

    examId = rand() % nexams;   //id of random exam

    int examSlot;
    int examIndex;

    for (j = 0; j < tmax; ++j) {
        for (k = 0; k < sol[j].currpos; ++k) {
            if (sol[j].e[k] == examId) {    //salvo slot e index dell'esame per dopo
                examSlot = j;
                examIndex = k;
                slotsToAvoid[j] = 1;
            }

            if (conflicts[examId][sol[j].e[k]] != 0) {
                slotsToAvoid[j] = 1;
            }
        }
    }

    for (j = 0; j < 10; ++j) {
        int randSlot = rand() % tmax;
        if (slotsToAvoid[randSlot] == 0) {
            if (sol[randSlot].currpos >= sol[randSlot].dim) { // i need to realloc sol[randSlot].e, because of it was allocate for 'nexams'
                sol[randSlot].dim = sol[randSlot].dim * 2;
                sol[randSlot].e = realloc(sol[randSlot].e, sol[randSlot].dim * sizeof(int));
            }

            sol[randSlot].e[(sol[randSlot].currpos++)] = examId;
            break;
        }
    }
    if (j == 10)
        return;

    sol[examSlot].e[examIndex] = sol[examSlot].e[(sol[examSlot].currpos--)-1];
}

void checksol(int **conflicts, Solution *sol, int tmax)
{
    int i, j, k,w=0;

    for(i=0; i<tmax; i++){
        for(j=0; j<sol[i].currpos; j++){
            w++;
            int e1 = sol[i].e[j];
            for(k=0; k<sol[i].currpos; k++){
                if(k!=j){
                    int e2 = sol[i].e[k];
                    if(conflicts[e1][e2]!=0){
                        printf("HAI SBAGLIATO GAY l'esame %d fa conflitto con %d sono nel timeslot %d\n", sol[i].e[j], sol[i].e[k], i);

                    }
                }
            }
        }
    }
    printf("CHECK = %d, timeslots = %d\n", w, tmax);
}

void feasible_search(Solution *sol, Solution **psol, int **conflicts, int *timeslots, int nexams, int nstudents, int tmax)
{
    int i=0, j=0, k=0, f=0, swapn=nexams/2, swi=0, swj=0, tmp=0;
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

    fprintf(stdout, "Graph coloring on sorted priority array of exams...\n");
    graph_coloring_greedy(sol, conflicts, stexams, nexams, tmax);
    if(sol[tmax].currpos>0){ // if i have exams in my extra time slot i have to schedule those exams
        fprintf(stdout, "Can't found it. Apllying Tabù Search with %d exams in extra time slot...\n", sol[tmax].currpos);
        if(tabu_search(sol, conflicts, nexams, tmax, nstudents)==0) {
            check_best_sol(sol, psol, conflicts, timeslots, nstudents, tmax);
            fprintf(stdout, "Sorting like Gauss function...\n");
            dens(sol, psol, conflicts, nexams, nstudents, tmax);
        }
        else
            fprintf(stdout, "Aborting Tabù Search...\n\n");
    }
    else{
        check_best_sol(sol, psol, conflicts, timeslots, nstudents, tmax);
        dens(sol, psol, conflicts, nexams, nstudents, tmax);
    }

    fprintf(stdout, "Multi-start metaheuristics...\n");
    for(i=0; i<MS; i++){
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
            if(tabu_search(sol, conflicts, nexams, tmax, nstudents)==0) {
                check_best_sol(sol, psol, conflicts, timeslots, nstudents, tmax);
                dens(sol, psol, conflicts, nexams, nstudents, tmax);
            }
        }
        else{
            check_best_sol(sol, psol, conflicts, timeslots, nstudents, tmax);
            dens(sol, psol, conflicts, nexams, nstudents, tmax);
        }
    }

    free(timeslots);
    free(priority);
    free(stexams);
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

int tabu_search(Solution *sol, int **conflicts, int nexams, int tmax, int nstudents)
{
    int i=0, j=0, k=0, ts=-1, *tabulist, dimtabu=tmax/2, next=0;
    float obj_min=INT_MAX, obj_tmp=0;

    tabulist = malloc(dimtabu*sizeof(int));
    for(i=0; i<dimtabu; i++)
        tabulist[i]=-1; // '-1' means no value

    for(i=0, k=0; i<sol[tmax].currpos; i++, k++){ // i use this cycle to take exams i put in the extra time slot
        obj_min=INT_MAX;
        ts=-1;
        for(j=0; j<tmax; j++){ // 'j' is the hypothetical time slot in which i want to put my exam sol[tmax].e[i]
            if(is_forbidden(sol, conflicts, sol[tmax].e[i], j, tabulist, dimtabu)==0){
                obj_tmp = confl_exams(sol, sol[tmax].e[i], j, conflicts, tmax, nstudents); // i calculate for that time slot the temporary 'obj'
                if(obj_tmp<obj_min){ // if this obj is better than the previous i remember the time slot in which my 'obj' it the best
                    obj_min = obj_tmp;
                    ts = j;
                }
            }
        }
        if(ts==-1) // all moves are forbidden
            return 1;
        if(k==TS) // i've reached the max allowed iteration
            return 1;
        schedule_exam(sol, sol[tmax].e[i], ts, conflicts, tabulist, &next, dimtabu, nexams, tmax); // i schedule my exam in time slot 'ts' which has the best temporary 'obj'
    }

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
                    // if it is presents and it has conflict with exam i want to schedule
                    return 1;
                }
            }
        }
    }

    return 0;
}

float confl_exams(Solution *sol, int exam, int timeslot, int **conflicts, int tmax, int nstudents)
{
    int i=0, k=0;
    float obj_tmp=0;

    /* i calculate all the conflicts between my exam and all the others exams excluding exams in time slot 'timeslot',
        because this time slot is the hypothetic time slot in which i want to schedule my exam and i want to minimize
        the 'obj' with conflicting exams */

    /*//down cycle
    for(i=timeslot+1; i<tmax; i++){ // cycle in time slot sol[timeslot, ..., tmax]
        for(k=0; k<=i+5 && k<sol[i].currpos; k++){ // cycle in sol[i].e[0, ..., k]
            obj_tmp += conflicts[exam][sol[i].e[k]]*power(2, 5-(i-timeslot));
        }
    }

    //up cycle
    for(i=0; i<timeslot; i++){ // cycle in time slot sol[0, ..., timeslot]
        if(timeslot-i>=5){
            for(k=0; k<sol[i].currpos; k++){ // cycle in sol[i].e[0, ..., k]
                obj_tmp += conflicts[exam][sol[i].e[k]]*power(2, 5-(timeslot-i));
            }
        }
    }

    return obj_tmp/nstudents;*/

    // instead of search for the min 'tmp_obj' i search time slot with less number of conflicting exam with the mine
    for(i=0; i<sol[timeslot].currpos; i++){
        if(conflicts[exam][sol[timeslot].e[i]]>0)
            k += conflicts[exam][sol[timeslot].e[i]];
    }

    return k;
}

int schedule_exam(Solution *sol, int exam, int ts, int **conflicts, int *tabulist, int *next, int dimtabu, int nexams, int tmax)
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

    tabulist[(*next)]=exam; // save move as tabù
    (*next)++;
    if((*next)>=dimtabu)
        (*next)=0;

    free(idel);

    return nconfl;
}

void dens(Solution *sol, Solution **psol, int **conflicts, int nexams, int nstudents, int tmax)
{
    int i=0, freets=0;
    int *mark, *density, *newdens, *idcorr, *timeslots;

    density = calloc(tmax, sizeof(int));
    density_gen(density, conflicts, sol, tmax);

    for(i=0; i<tmax; i++)
        if(density[i]==0)
            freets++; // count how much free time slots i have

    mark = calloc(tmax, sizeof(int));
    newdens = malloc((tmax-freets)*sizeof(int)); // newdens=gauss_sorting(density)
    idcorr = malloc((tmax-freets)*sizeof(int)); // it will contain the index corresponding to the value of: newdens[i]=value <-> idcorr[i]=idexams
    gauss_sorting(density, newdens, idcorr, tmax-freets, mark);

    timeslots = malloc(tmax*sizeof(int));

    if(freets!=0){
        fprintf(stdout, "Detected free time slot...\n");
        for(i=0; i<tmax-freets; i++) // set in 'timeslots' the new order of exams generated in 'gauss_sorting()'
            timeslots[i]=idcorr[i];
        swap_freetimeslots(sol, psol, conflicts, timeslots, nexams, nstudents, tmax, freets);
    }
    else{
        for(i=0; i<tmax; i++)
            timeslots[i]=idcorr[i];
        check_best_sol(sol, psol, conflicts, timeslots, nstudents, tmax);
    }

    free(density);
    free(newdens);
    free(idcorr);
    free(mark);
    free(timeslots);
}

void density_gen(int *density, int **conflicts, Solution *sol, int tmax)
{
    int i=0, j=0, k=0, t=0;

    ///DENSITY Ne,e'
    for(i=0; i<tmax; i++){ //cycle in time slot sol[0, ..., tmax]
        for(k=0; k<sol[i].currpos; k++){ //cycle in sol[i].e[0, ..., k] - exams i scheduled in time slot 'i'
            for(j=0; j<=i+5 && j<tmax; j++){ //after 5 i don't pay no kind of penalty - cycle to compare time slot 'i' with time slot 'j'
                for(t=0; t<sol[j].currpos; t++){ //i have to compare all exams, for this reason 'j' and 't' start from zero
                    if(sol[i].e[k]!=sol[j].e[t]){
                            density[i] += conflicts[sol[i].e[k]][sol[j].e[t]];
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

void swap_freetimeslots(Solution *sol, Solution **psol, int **conflicts, int *timeslots, int nexams, int nstudents, int tmax, int freets)
{
    int i=0, *mark, *freetimeslots, *valfreets;

    /* Work in progess for thi section */

    mark = calloc(tmax, sizeof(int));
    freetimeslots = malloc((tmax-freets)*sizeof(int));
    valfreets = malloc(tmax*sizeof(int)); // this array'll contain the number of time slots from 0 to tmax-1
    for(i=0; i<tmax; i++) // initialization of 'valfreets', i'll put a disposition of these values in 'freetimeslots' that says me in which slot i have to put the free time slot
        valfreets[i]=i;

    disp(0, valfreets, conflicts, sol, psol, mark, timeslots, freetimeslots, nexams, nstudents, tmax, freets);

    free(freetimeslots);
    free(valfreets);
}

void disp(int pos, int *valfreets, int **conflicts, Solution *sol, Solution **psol, int *mark, int *timeslots, int *freetimeslots, int nexams, int nstudents, int tmax, int freets)
{
    int i=0;

    if(pos>=freets){
        check_best_sol(sol, psol, conflicts, timeslots, nstudents, tmax);
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

void check_best_sol(Solution *sol, Solution **psol, int **conflicts, int *timeslots, int nstudents, int tmax)
{
    int i=0, j=0, k=0, t=0, flag=0, *inserted;
    float obj_tmp=0;

    /*if(freets!=0){
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
        free(inserted);
    }*/

    for(i=0; i<tmax; i++)
        psol[i] = &sol[timeslots[i]];

    ///MIN OBJ 2^(5-i)*Ne,e'/|S|
    for(i=0; i<tmax-1; i++){ //cycle in time slot psol[0, ..., tmax-1]
        for(k=0; k<psol[i]->currpos; k++){ //cycle in psol[i]->e[0, ..., k] - exams i scheduled in time slot 'i'
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

int power(int base, int exp)
{
    int i=0, res=1;

    if(exp==0)
        return 1;

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
