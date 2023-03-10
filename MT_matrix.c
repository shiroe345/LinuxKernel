#include<stdio.h>
#include<stdlib.h>
#include <time.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include<sys/syscall.h>
#include <pthread.h>

#define MAX_READ_LENGTH 4096

// 加入 Mutex
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;

int fd; //file descriptor
int number_of_threads;
int m1[MAX_READ_LENGTH][MAX_READ_LENGTH], m2[MAX_READ_LENGTH][MAX_READ_LENGTH], result[MAX_READ_LENGTH][MAX_READ_LENGTH];
int row1, row2, col1, col2;

void *child(void* arg){
    /* matrix calculation */
    int* number = (int*)arg;
    if(row1 > 16){ //cut by row in matrix1
    	int number_per_thread = row1 / number_of_threads;
	    if(*number < number_of_threads - 1){
	        for(int i = *number * number_per_thread;i<(*number+1) * number_per_thread;i++){
	            for(int j = 0;j<col2;j++){
	                int sum = 0;
	                for(int k = 0;k<row2;k++){
	                    sum += m1[i][k] * m2[k][j];
	                }
	                result[i][j] = sum;
	            }
	        }
	    }
	    else{
	        for(int i = *number * number_per_thread;i<row1;i++){
	            for(int j = 0;j<col2;j++){
	                int sum = 0;
	                for(int k = 0;k<row2;k++){
	                    sum += m1[i][k] * m2[k][j];
	                }
	                result[i][j] = sum;
	            }
	        }
	    }
	}
	else{ //cut by col in matrix2
		int number_per_thread = col2 / number_of_threads;
		if(*number < number_of_threads - 1){
	        for(int i = 0;i<row1;i++){
	            for(int j = *number * number_per_thread;j<(*number+1) * number_per_thread;j++){
	                int sum = 0;
	                for(int k = 0;k<row2;k++){
	                    sum += m1[j][k] * m2[k][i];
	                }
	                result[i][j] = sum;
	            }
	        }
	    }
	    else{
	        for(int i = 0;i<row1;i++){
	            for(int j = *number * number_per_thread;j<col2;j++){
	                int sum = 0;
	                for(int k = 0;k<row2;k++){
	                    sum += m1[j][k] * m2[k][i];
	                }
	                result[j][j] = sum;
	            }
	        }
	    }
	}
    /* every thread calculate m1(row1 / number_of_threads) * m2 */

    int tid = syscall(__NR_gettid); //thread_id, same as getpid

    /* call write to /proc/thread_info */
    char stringToSend[100];
    snprintf(stringToSend,100,"%d",tid);
    pthread_mutex_lock( &mutex1 ); // 上鎖
    int ret = write(fd, stringToSend, strlen(stringToSend)); // Send the string to the LKM
    if (ret < 0)
    {
        perror("Failed to write the message to the device.");
        //return errno;
    }
    pthread_mutex_unlock( &mutex1 ); // 解鎖

    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    

    /* get arguments */
    number_of_threads = atoi(argv[1]);
    char *matrix1 = argv[2] ,*matrix2 = argv[3];
 

    /* open and read file of matrixes */
    FILE *f1, *f2;
    f1 = fopen(matrix1,"r");
    f2 = fopen(matrix2,"r");
    fscanf(f1,"%d %d",&row1 ,&col1);
    fscanf(f2,"%d %d",&row2, &col2);
    for(int i = 0;i<row1;i++)
        for(int j = 0;j<col1;j++)
            fscanf(f1,"%d",&m1[i][j]);
    for(int i = 0;i<row2;i++)
        for(int j = 0;j<col2;j++)
            fscanf(f2,"%d",&m2[i][j]);
    fclose(f1);
    fclose(f2);

    /* open proc */ 
    fd = open("/proc/thread_info",O_RDWR); //don't know how to open writable in proc without sudo ./this.file
    if (fd < 0)
    {
        perror("Failed to open the device...");
        return errno;
    }

    time_t start =  time(NULL);

    /* create children */
    pthread_t children[number_of_threads];
    for(int i = 0;i<number_of_threads;i++){
        int* num = (int*)malloc(sizeof(int));
        *num = i;
        pthread_create(&children[i],NULL,child,num);
    }
    
    /* terminate children */
    for(int i = 0;i<number_of_threads;i++){
        pthread_join(children[i],NULL);
    }

    time_t end = time(NULL);
    int elapse_time = end - start;
    printf("ALL time is : %d\n",elapse_time);
    /* call read to /proc/thread_info */
    char thread_info[MAX_READ_LENGTH];
    int ret = read(fd,thread_info,MAX_READ_LENGTH);
    if (ret < 0)
    {
        perror("Failed to write the message to the device.");
        return errno;
    }

    /* write result file */
    FILE *res = fopen("result.txt","w");
    fprintf(res,"%d %d\n",row1,col2);
    for(int i = 0;i<row1;i++){
        for(int j = 0;j<col2;j++){
            fprintf(res,"%d ",result[i][j]);
        }
        fprintf(res,"\n");
    }
    fclose(res);

    /* print the thread_info */
    thread_info[strlen(thread_info)] = '\0';
    while(thread_info[strlen(thread_info)-1] != '\n') thread_info[strlen(thread_info)-1] = '\0';
    printf("PID:%d\n%s",getpid(),thread_info);

    //printf("Due Date: 2022/11/25 23:59:59\r\n");
    return 0;
}
