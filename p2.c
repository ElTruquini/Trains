#include "station.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h> 
#include <unistd.h>
#include <assert.h>

#define BILLION 1000000000L
#define TS_LEN 28
#define TENTH_SEC 100000

typedef struct Train{ 
    pid_t pid; 
    int id; 
    int pr; //1 high, 0 low
    char b; // bound
    char bound[5];
    int ld;
    double ld_time; // Timestamp stop loading time
    int x;	// crossing time
    int s; // 0-loading, 1-ready, 2-granted, 3-xing, 4-gone 
    struct Train *next; 
}Train;


pthread_mutex_t queue_mutex;
pthread_mutex_t main_track_mutex;
pthread_cond_t green_light_cv;
struct Train *root, *temp;
int train_ctr = 0; // number of trains
int rdy_ctr = 0;
int awaiting_ld = 1; //global variable used to signal runner start loading
int train_on_track = 0; //global variable used to signal dispatcher,
	

Train *addTrain (char b, int pr, int ld, int x, char *bound){
	Train *new_train = (Train*)malloc(sizeof(Train));
	memset(new_train, 0, sizeof(Train));

	new_train->id = train_ctr;
	new_train->pr = pr;
	new_train->ld = ld;
	new_train->x = x;
	new_train->b = b;
	strncpy(new_train->bound,bound,5);	
	new_train->s = -1; 
	new_train->next = NULL;


	if (train_ctr == 0){
		root = new_train;
		printf("Id=%d - s:%d, b:%c-%s, p:%d, ld-x:%d-%d, ldts:%f\n", 
			new_train->id, new_train->s, new_train->b, new_train->bound, new_train->pr, new_train->ld, new_train->x, new_train->ld_time);

	}else{
		for (temp = root ; temp->next != NULL ; temp = temp->next);
		temp->next = new_train;
		printf("Id=%d - s:%d, b:%c-%s, p:%d, ld-x:%d-%d, ldts:%f\n", 
			new_train->id, new_train->s, new_train->b, new_train->bound, new_train->pr, new_train->ld, new_train->x, new_train->ld_time);
	}
	train_ctr ++;

	return new_train;
}

/*
	// ssize_t format_timeval(struct timeval *tv, char *buf, size_t sz){
	// 	ssize_t written = -1;
	// 	struct tm *gm = gmtime(&tv->tv_sec);
	// 	if (gm){
	// 		written = (ssize_t)strftime(buf, sz, "%T", gm);
		
	// 		// Getting first digit from usec
	// 		int first = tv->tv_usec; 
	// 		while (first >= 10) { first = first/10;}
	// 		// printf("\nformat_timeval First digit:%d \n", first);
		
	// 		if ((written > 0) && ((size_t)written < sz)){
	// 			int w = snprintf(buf+written, sz-(size_t)written, ".%d", first);
	// 			// int w = snprintf(buf+written, sz-(size_t)written, ".%01i", tv->tv_usec);
	// 			written = (w > 0) ? written + w : -1;
	// 		}
	// 	}
	// 	return written;
	// }
*/

int getTime(struct timeval start, struct timeval stop, char *buf){
	double hours, t, minutes, seconds, accum;
	memset( buf, '\0', sizeof(char)*TS_LEN );


	// double waka1 = (double)(start.tv_usec) / 1000000 + (double)(start.tv_sec); 
	// double waka2 = (double)(stop.tv_usec) / 1000000 + (double)(stop.tv_sec); 
	// // printf("getTime - start:%f | stop:%f\n", waka1, waka2);
	
	accum = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);

	hours = accum/3600;
	t = fmod(accum,3600);
	minutes = t/60;
	seconds = fmod(t,60);
	int w = sprintf(buf, "%02d:%02d:%04.1f", (int)hours, (int)minutes, seconds);
	// printf("getTime - Accum:%f | PRE STRING :%lf, %02d:%02d:%04.1f | BUF:%s\n", accum, (int)hours, (int)minutes, seconds, buf);

	int x = (w > 0) ? w : -1;
	return x;
}

void* runner(void *train_arg){
	char buf[TS_LEN];
	struct Train *train = (Train *)train_arg;
	struct timeval start_ld, stop_ld, start_x, stop_x;
	// printf("Runner - NEW THREAD Train | id:%d | s:%d | b:%c | pr:%d \n", train->id, train->s, train->b, train->pr );


	int loading = train->ld * TENTH_SEC;
	int xing = train->x * TENTH_SEC;
	// Waiting for dispatcher global var signal for all trains to start loading
	while (awaiting_ld);
	
	// LOADING
	train->s = 0; // loading
	gettimeofday(&start_ld, NULL);
	usleep(loading);
	gettimeofday(&stop_ld, NULL);
	if (getTime(start_ld, stop_ld, buf) <= 0){
		fprintf(stderr,"ERROR - Converting clock format (1) \n");
	}
	printf("%s Train %d is READY to go %s\n", buf, train->id, train->bound);
	train->ld_time = (double)(stop_ld.tv_usec) / 1000000 + (double)(stop_ld.tv_sec); 
	train->s = 1; //ready
	
	pthread_mutex_lock(&queue_mutex);
	addNode(train->id, train->pr, train->b, train->ld_time);
	rdy_ctr++;
	pthread_mutex_unlock(&queue_mutex);


	//CROSSING main track
	pthread_mutex_lock(&main_track_mutex);
	while(train->s != 2){
		// printf("Train:%d is going to sleep ZzZzZZZZZzzzzZz\n", train->id);
		pthread_cond_wait(&green_light_cv, &main_track_mutex);
	}
	// printf("Train:%d is AWAKE\n", train->id);
	gettimeofday(&start_x, NULL);
	if (getTime(start_ld, start_x, buf) <= 0){
		fprintf(stderr,"ERROR - Converting clock format (2)\n");
	}
	train->s = 3; //xing
	train_on_track = 1; 
	printf("%s Train:%d is ON the main track going %s\n", buf, train->id, train->bound);
	usleep(xing);
	gettimeofday(&stop_x, NULL);
	if (getTime(start_ld, stop_x, buf) <= 0){
		fprintf(stderr,"ERROR - Converting clock format (3)\n");
	}
	printf("%s Train:%d is OFF the main track going %s\n", buf, train->id, train->bound);
 	// TODO: Timestamp finish xing
	pthread_mutex_unlock(&main_track_mutex);
	train->s = 4; //gone
	train_on_track = 0; 
	printf("RUNN - Exit Train:%d thread with status:%d \n", train->id, train->s);





	// return EXIT_SUCCESS;






	return NULL;

}

void *dispatcher(void* args){
	pthread_t tid[train_ctr];
	pthread_attr_t attr;
	int total_dispatched = 0;
	struct Train *temp;

	// Initialize mutex and condition variable objects
	pthread_mutex_init(&queue_mutex, NULL);
	pthread_mutex_init(&main_track_mutex, NULL);
	pthread_cond_init (&green_light_cv, NULL);
	// For portability, explicitly create threads in joinable state
	pthread_attr_init(&attr);
  	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);


	// Initialize train threads
	// printf("DISP - address of root:%p\n", (void *) &root);

  	int x;
	for (x = 0, temp = root ; x < train_ctr ; x++, temp = temp->next){
		// printf("DISP - Creating thread #%d | id:%d | s:%d | b:%c | pr:%d |temp_add:%p |root_add:%p\n",
					 // x, temp->id, temp->s, temp->b, temp->pr, temp, root);
		if (pthread_create(&tid[x], &attr, runner, (void *) temp) != 0){
			fprintf(stderr,"ERROR - Cannot create TRAIN thread tid:%d\n", x);
		}
	}

	usleep(500000);
	awaiting_ld = 0; // Signal ready to load

	usleep(3000000); //***************************************!!!!!!!!!! remove this sleep, used for testing

	int last_b_disp = 0; // 0 West, 1 East 
	while (total_dispatched < train_ctr){
		Train *opt_first, *opt_second, *to_disp_train;
		int to_disp_id;
		while (train_on_track); //waiting for train to leave main track befores dispatches next train
		// printf ("DISP - Train off main track, Anything ready?\n");
		if (rdy_ctr > 0){
			printf ("DISP - Train is waiting |rdy_ctr:%d | EA_ctr:%d | WE_ctr:%d | e_ctr:%d | w_ctr:%d\n",rdy_ctr, stEA_ctr, stWE_ctr,ste_ctr, stw_ctr);
			if (stEA_ctr > 0 || stWE_ctr > 0){ // High train waiting 
				printf ("DISP - Atleast one high waiting | rdy_ctr:%d | ***EA_ctr:%d | ***WE_ctr:%d | e_ctr:%d | w_ctr:%d\n",rdy_ctr, stEA_ctr, stWE_ctr,ste_ctr, stw_ctr);
				// More than 1 High waiting?
				if ((stEA_ctr >= 1 && stWE_ctr >= 1) || stEA_ctr >= 2 || stWE_ctr >= 2 ){ 
					printf ("DISP - More than one high waiting |stEA_ctr:%d | stWE_ctr:%d\n", stEA_ctr, stWE_ctr);

					// 2+ High, OPPOSITE direction - dispatch opposite direction from last dispatched
					if (stEA_ctr >= 1 && stWE_ctr >= 1) {
						printf ("DISP - >=2 high - Opposite direction |stEA_ctr:%d | stWE_ctr:%d\n", stEA_ctr, stWE_ctr);
						if (last_b_disp){ // 0 West, 1 East 
							to_disp_id = getHeadID('W'); 
							// printf("DISP - to_disp_train | id:%d | b:%c | pr:%d\n", to_disp_train->id, to_disp_train->b, to_disp_train->pr);
						} else{
							to_disp_id = getHeadID('E'); 
						}

					// 2+ High, SAME direction
					}else{	//this should be the code for traveling same direction same priority
						printf ("DISP - >=2 high - Same direction |stEA_ctr:%d | stWE_ctr:%d\n", stEA_ctr, stWE_ctr);
						int x, y;
						if (stWE_ctr >0){
							// assert(stEA_ctr == 0);
							x = getHeadID('W');
							y = getSecondID('W');

						}else{
							// assert(stWE_ctr == 0);
							x = getHeadID('E');
							y = getSecondID('W');

						}

						//Insert code for getmin time and remove by id




						for(opt_first = root ; opt_first->id != x ; opt_first = opt_first->next);
						for(opt_second = root ; opt_second->id != y ; opt_second = opt_second->next);
						printf("opt_first id:%d | opt_second id:%d \n", opt_first->id, opt_second->id);
						printStation('W');


						// Dispatch by loading times
						if (opt_first->ld_time < opt_second->ld_time){
							printf ("DISP - Dispatching by Ld_time  | ***id:%d->ld_time:%f | id:%d->ld_time:%f\n", opt_first->id, opt_first->ld_time, opt_second->id, opt_second->ld_time);
							to_disp_id = opt_first->id;
						} else if (opt_first->ld_time > opt_second->ld_time){
							printf ("DISP - Dispatching by Ld_time  | id:%d->ld_time:%f | ***id:%d->ld_time:%f\n", opt_first->id, opt_first->ld_time, opt_second->id, opt_second->ld_time);
							to_disp_id = opt_second->id;
						//Dispatch by (min)id
						}else{ 	
							printf ("DISP - Dispatching by lower id |opt_first->id:%d | opt_second->id:%d\n", opt_first->id, opt_second->id);
							to_disp_train = (opt_first->id < opt_second->id) ? opt_first : opt_second;
						}
					}
				// Single High train waiting
				}else{
					printf("DISP - Single high train | rdy_ctr:%d |stEA_ctr:%d | stWE_ctr:%d\n", rdy_ctr, stEA_ctr, stWE_ctr);
					if (stEA_ctr == 1){
						to_disp_id = getHeadID('E'); 
					} else{
						to_disp_id = getHeadID('W'); 
					}
				}

			// Low pr waiting (no high) waiting
			}else { // Low train waiting
				printf ("DISP - Atleast one low waiting | (Must be 0)EA_ctr:%d | (Must be 0)WE_ctr:%d | *e_ctr:%d | *w_ctr:%d\n", stEA_ctr, stWE_ctr,ste_ctr, stw_ctr);
				// More than 1 Low waiting
				if ((ste_ctr >= 1 && stw_ctr >= 1) || ste_ctr >= 2 || stw_ctr >= 2 ){ 
					printf ("DISP - More than one low waiting |ste_ctr:%d | stw_ctr:%d\n", ste_ctr, stw_ctr);
				// Single Low train waiting
				}else{
					printf("DISP - Single high train | rdy_ctr:%d |ste_ctr:%d | stw_ctr:%d\n", rdy_ctr, ste_ctr, stw_ctr);
					if (ste_ctr == 1){
						to_disp_id = getHeadID('e'); 
					} else{
						to_disp_id = getHeadID('w'); 
					}
				}				
			}
			for(to_disp_train = root ; to_disp_train->id != to_disp_id ; to_disp_train = to_disp_train->next);
			// Simulation rule: if same priority, opposite directions, 
			// pick train which will travel in the opposite dir from last dispatched
			switch (to_disp_train->b){
				case 'W':
				case 'w':
					last_b_disp = 0;
					break;
				case 'E':
				case 'e':
					last_b_disp = 1;
					break;

			}

			printf("DISP - new last_b_disp:%d | b:%c | \n", last_b_disp, (last_b_disp == 0) ? 'W' : 'E' );
			printf("DISP - DISPATCHING | id:%d | b:%c | pr:%d\n\n", to_disp_train->id, to_disp_train->b, to_disp_train->pr);

			//Signal Granted train to dispatch
			pthread_mutex_lock(&main_track_mutex);
			to_disp_train->s = 2; // Granted
			pthread_cond_broadcast(&green_light_cv);
			pthread_mutex_unlock(&main_track_mutex);

			//Removing train from station
			pthread_mutex_lock(&queue_mutex);
			rdy_ctr--;
			removeHead(to_disp_train->b);
			pthread_mutex_unlock(&queue_mutex);

			while (to_disp_train->s != 4);



		}
		total_dispatched++;
		// total_dispatched = train_ctr; //**************************change this to total_dispatched++
	}	


	// // Get candidate trains  op1 and opt_second
	// int diff_direction = 0;
	// if (stEA_ctr >= 1){ 
	// 	int opt_E_id = getHeadID('E'); 
	// 	for(opt_first = root ; opt_first->id != opt_E_id ; opt_first = opt_first->next);
	// 	printf("DISP - Option 1 to disp | id:%d | b:%c | pr:%d\n", opt_first->id, opt_first->b, opt_first->pr);
	// 	diff_direction++;
	// }
	// if (stEA_ctr >= 1){ 
	// 	int opt_W_id = getHeadID('W'); 
	// 	for(opt_second = root ; opt_second->id != opt_W_id ; opt_second = opt_second->next);
	// 	printf("DISP - Option 2 to disp | id:%d | b:%c | pr:%d\n", opt_second->id, opt_second->b, opt_second->pr);
	// 	diff_direction++;














	// usleep(100000);
	// awaiting_ld = 0; // Signal all trains to start loading
	// usleep(3000000);
	// temp = root;
	// temp->s = 2;
	// pthread_mutex_lock(&main_track_mutex);
	// pthread_cond_broadcast(&green_light_cv);
	// pthread_mutex_unlock(&main_track_mutex);
	// usleep(3000000);
	// temp = root->next;
	// temp->s = 2;
	// pthread_mutex_lock(&main_track_mutex);
	// pthread_cond_broadcast(&green_light_cv);
	// pthread_mutex_unlock(&main_track_mutex);
	// // usleep(3000000);
	// // root->next->s =2;
	// // return EXIT_SUCCESS;


/*	
	// printf("DISP - 1. Temp | s:%d | ID:%d | b:%c \n", temp->s, temp->id, temp->b);
	
	while (total_dispatched < train_ctr){
		// printf("DISP - LOOP, total_dispatched:%d\n", total_dispatched);
		while (rdy_ctr == 0);
		if (stEA_ctr > 0 || stWE_ctr > 0){ // High PR 
			double E1, W1;
			int total_waiting=0 ,dispatch_id;
			printf("\nDISP - High priority train is ready to depart - EASTq:%d, WESTq:%d\n", stEA_ctr, stWE_ctr);
			if (stEA_ctr > 0 ){
				E1 = getLoadTime('E');
				// printf("EAST waiting Ld_time:%f\n", E1);
				total_waiting++;
			}
			if (stWE_ctr > 0 ){
				W1 = getLoadTime('W');
				// printf("WEST waiting Ld_time:%f\n", W1);
				total_waiting++;
			}
			if (total_waiting > 1){
				if  (E1 < W1) {
					printf("DISP - >2 Waiting - EAST is smaller, must go first - %f | total_waiting:%d\n",E1, total_waiting);
					dispatch_id = getHeadID('E');
					// dispatch_b = 'E';
				}else{
					printf("DISP - >2 Waiting - WEST is smaller, must go first - %f | total_waiting:%d\n",W1, total_waiting);
					
					dispatch_id = getHeadID('W');
					// dispatch_b = 'W';
				}
			}else {
				if (E1 != 0){
					printf("DISP - Single - EAST is smaller, must go first - %f | total_waiting:%d\n",E1, total_waiting);
					printf("DISP - EAST counter:%d\n", stEA_ctr);
					dispatch_id = getHeadID('E');
					// dispatch_b = 'E';
				}else{
					printf("DISP - Single - WEST is smaller, must go first - %f | total_waiting:%d\n",W1, total_waiting);
					dispatch_id = getHeadID('W');
					// dispatch_b = 'W';
				}
			}
			for(temp = root; temp->id != dispatch_id ; temp = temp->next );
			temp->s = 2; // Granted
			printf("DISP - TO DISPATCH | ID=%d | S:%d | b:%c-%s | p:%d | ld-x:%d-%d | ld_time:%f\n", temp->id, temp->s, temp->b, temp->bound, temp->pr, temp->ld, temp->x, temp->ld_time);
			
			pthread_mutex_lock(&main_track_mutex);
			pthread_cond_broadcast(&green_light_cv);
			pthread_mutex_unlock(&main_track_mutex);
			
			pthread_mutex_lock(&queue_mutex);
			rdy_ctr--;
			removeHead(temp->b);
			pthread_mutex_unlock(&queue_mutex);
			while (temp->s != 4){

				// printf ("DISP - train dispatched status:%d\n", temp->s);
			}
			printf("DISP - Train dispatched GONE | ID:%d | Status(should be 4):%d  \n\n", temp->id, temp->s);
		}
		total_dispatched++;
		// printf("DISP - total_dispatched:%d |train_ctr:%d\n", total_dispatched, train_ctr);
	}

	printf ("DISP - Joining threads\n");


*/



	// Join all running Train threads 
	for (x =0 ; x < train_ctr ; x++){
		if ( pthread_join(tid[x], NULL) != 0){
			fprintf(stderr, "ERROR - Cannot join thread TRAIN tid:%d\n", x);
		}
	}
	// printf("\n");
	// printStation('E');
	// printStation('W');
	// printStation('e');
	// printStation('w');



	return NULL;
}

int main (int argc, char *argv[]){
	FILE *fp;

	if (argc != 2){
		printf("ERROR - Usage: [file_name]\n");
		return EXIT_FAILURE;
	}

	fp = fopen(argv[1], "r");
	if (fp == NULL){
		printf("ERROR - failed opening file\n");
		return EXIT_FAILURE;
	}else {
		printf("File opened successfully\n");
	}

	//Reading contents from input file
	char b, bound[5] ;
	int l, p, x;
	Train *temp;
	printf("=======INPUT FILE=======\n");
	for ( ; fscanf(fp, "%c %d %d \n", &b, &l, &x) != EOF ; ){
		if (b == 'w' || b == 'e'){
			p = 0;
		}else{
			p = 1;
		}

		if (b == 'w' || b == 'W'){
			strncpy(bound, "West", 5);
		}else{
			strncpy(bound, "East", 5);		
		}
		temp = addTrain(b,p,l,x, bound);
	}
	printf("=========================\n\n");

	// For portability, explicitly create threads in joinable state
	pthread_t tid;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
  	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	
	//Create dispatcher thread
	if (pthread_create(&tid, &attr, dispatcher, NULL) != 0){
		fprintf(stderr,"ERROR - Cannot create DISPATCHER thread TID:%lu\n", tid);
		return EXIT_FAILURE;
	}

	if (pthread_join(tid, NULL) != 0){
		fprintf(stderr, "ERROR - Cannot join thread #%d\n", x);
	}

	printf("\nMAIN: All threads finalized\n");

	printf("==========FINAL DATA==========\n");
	for(temp = root; temp != NULL ; temp = temp->next ){
		printf("Id=%d - s:%d, b:%c-%s, p:%d, ld-x:%d-%d, ld_time:%f\n", temp->id, temp->s, temp->b, temp->bound, temp->pr, temp->ld, temp->x, temp->ld_time);
	}
	printf("==============================\n");



	//TODO: NEED TO FREE ALL MALLOC FROM ADD TRAIN
	//TODO: DESTROY MUTEXES 

	/*
	  pthread_attr_destroy(&attr);
   pthread_mutex_destroy(&count_mutex);
   pthread_cond_destroy(&count_threshold_cv);
   pthread_exit(NULL);
   */


	fclose(fp);
	return EXIT_SUCCESS;
}