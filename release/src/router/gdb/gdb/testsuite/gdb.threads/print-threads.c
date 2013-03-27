#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

void *thread_function(void *arg); /* Pointer to function executed by each thread */

int slow = 0;

#define NUM 5

int main() {
    int res;
    pthread_t threads[NUM];
    void *thread_result;
    int args[NUM];
    int i;

    for (i = 0; i < NUM; i++)
      {
	args[i] = i;
	res = pthread_create(&threads[i], NULL, thread_function, (void *)&args[i]);
      }

    for (i = 0; i < NUM; i++)
      res = pthread_join(threads[i], &thread_result);

    printf ("Done\n");

    if (slow)
      sleep (4);

    exit(EXIT_SUCCESS);
}

void *thread_function(void *arg) {
    int my_number = *(int *)arg;
    int rand_num;

    printf ("Print 1, thread %d\n", my_number);
    sleep (1);

    if (slow)
      {
	printf ("Print 2, thread %d\n", my_number);
	sleep (1);
	printf ("Print 3, thread %d\n", my_number);
	sleep (1);
	printf ("Print 4, thread %d\n", my_number);
	sleep (1);
	printf ("Print 5, thread %d\n", my_number);
	sleep (1);
      }

    printf("Bye from %d\n", my_number);
    pthread_exit(NULL);
}

