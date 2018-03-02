#ifndef STATION_H_
#define STATION_H_

// extern struct student_data student;
typedef struct Node{ 
    int id; 
    int pr; //1 high, 0 low
    char b; // bound
    double ld_time;
    struct Node *next; 
}Node;

extern int ste_ctr, stEA_ctr, stw_ctr, stWE_ctr;
extern int getSecondID(char b);
extern int getHeadID(char b);
extern void addNode (int train_id, int train_pr, char train_b, double ld_time);
extern int waitingLine(char b);
extern int printStation(char b);
extern int removeHead(char b);
extern double getLoadTime(char b);


#endif 
