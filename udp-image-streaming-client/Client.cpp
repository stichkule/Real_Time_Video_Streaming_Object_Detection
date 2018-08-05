/***********************************************************************************************
 *   C++ UDP client for live image upstreaming
 *   Modified from https://github.com/chenxiaoqino/udp-image-streaming
 *                 http://mercury.pr.erau.edu/~siewerts/cec450/code/sequencer_generic/
 *
 *   Copyright (C) 2018
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   @Description: This is a multithreaded code
 *                 Scheduler - SCHED_FIFIO
 *                 Thread1 - Sequencer (schedules other threads at required frquencies)
 *                 Thread2(@15Hz)  - Frame Capture and UDP socket transfer
 *                 Thread3(@0.3Hz) - Run python script for object detection using AWS Rekognition
 *                 Thread4(@0.3Hz) - Connect to another UDP socket port and send the AWS result -json
 *   @Authors:     Srivishnu Alvakonda
 *	               Mounika Reddy Edula
 *                 Shiril Tichkule
 *   @Date:        04/30/2018
 *
 ***************************************************************************************************/

#include <unistd.h> //For exec() to run python script
#include <stdio.h> //For printf
#include <stdlib.h> //For malloc
#include <iostream> //For Cout,Cerror
#include <time.h> //For calculation of WCET and timing analysis of deadlines
#include <math.h>
#include "config.h" // Cofiguration parameters like frame width and height
#include <errno.h> //for perror
#include <sys/time.h> //Timing analsyis
#include <fstream> //For file operations like write and read
#include <syslog.h> //For logging the debug statements

#include <pthread.h> //For thread creation,mutex
#include <sched.h> //To set scheduler to SCHED_FIFO
#include <semaphore.h> //Semaphores

#include <Python.h> //To run python script from C++

#include "PracticalSocket.h" //Open source library for UDP sockets
#include <opencv2/core/core.hpp> //Opensource OpenCV library
#include <opencv2/highgui/highgui.hpp> //Opensoure OpenCV library
#include <opencv2/imgproc/imgproc.hpp> //Opensource OpenCV library

using namespace cv;
using namespace std;

#define NUM_THREADS 4
#define TRUE 1
#define FALSE 0
#define USEC_PER_MSEC (1000)
#define NANOSEC_PER_SEC (1000000000)

/******multithreading**************/
sem_t Streaming_sem, Detection_sem, Sequencer_sem,sendlabels_sem; //Semaphores for IPC - ordering the threads
int abortS1=FALSE, abortS2=FALSE ,abortTest= FALSE,abortS3= FALSE; //Graceful exit
pthread_mutex_t processing; //Disable premption critical section
pthread_mutex_t writing;//Disable preemption critical section


/*********UDP sockets************/
string servAddress;//Address of server to connect
unsigned short servPort; //Port to transfer frames
unsigned short servPort1;//Port to transfer the AWS object labels


/*********Timing and deadline Analysis**********/
struct timeval start_sequencer, stop_sequencer, start_streaming, stop_streaming, start_detection, stop_detection, start_labels, stop_labels;
double WCET_sequencer=0.0, WCET_streaming=0.0, WCET_detection=0.0, WCET_labels=0.0;

double sequencer_deadline = 6.6;
double streaming_deadline = 66.0;
double detection_deadline = 3000.0;
double labels_deadline = 3000.0;

double acc_streaming = 0.0;
double acc_detection = 0.0;
double acc_labels = 0.0;

typedef struct
{
    int threadIdx;
    unsigned long long sequencePeriods;
} threadParams_t;

/************************************************************
* Sequencer Thread - Acts like a scheduler of streaming threads
*                    streaming thread at frquency 15FPS
*		     Thread communication using semaphores
* priority	   - RT_MAX
*************************************************************/
void* Sequencer_thread(void*)
{
    struct timeval current_time_val;
    struct timespec delay_time = {0,6666666}; // delay for 6.66 msec, 150 Hz sequencer thread
    struct timespec remaining_time;
    double current_time;
    double residual;
    int rc, delay_cnt=0;
    unsigned long long seqCnt=0;

    do
    {
        delay_cnt=0; residual=0.0;
        do
        {
            rc=nanosleep(&delay_time, &remaining_time);
            if(rc == EINTR)
            {
                residual = remaining_time.tv_sec + ((double)remaining_time.tv_nsec / (double)NANOSEC_PER_SEC);
                delay_cnt++;
            }
            else if(rc < 0)
            {
                perror("Sequencer nanosleep");
                exit(-1);
            }
        } while((residual > 0.0) && (delay_cnt < 100));

        gettimeofday(&start_sequencer, (struct timezone *)0);
        seqCnt++;
        if(delay_cnt > 1) printf("Sequencer looping delay %d\n", delay_cnt);

        // Streaming service = RT_MAX-1	@ 15 Hz
        if((seqCnt % 10) == 0)
        {
        sem_post(&Streaming_sem); //Give semphore for streaming thread
        }

        gettimeofday(&stop_sequencer, (struct timezone *)0);
        double diff = ((int)(stop_sequencer.tv_sec-start_sequencer.tv_sec))*1000000 + ((int)(stop_sequencer.tv_usec-start_sequencer.tv_usec));
        if(diff > WCET_sequencer)
        {
          WCET_sequencer = diff;
        }
    } while(!abortTest);

    //On error release semaphore and exit all threads
    sem_post(&Streaming_sem);
    abortS1=TRUE; abortS2=TRUE;abortS3=TRUE;
    pthread_exit((void *)0);
}

/************************************************************
* Streaming Thread - Waits for semaphore released by sequencer
*		     Capture a frame and resize as per configuration
*                    Display the frames captured
*                    encode the image into raw bytes
*                    send the number of packets required for a frame
*                    Pack the raw bytes into each UDP packet 4096 bytes
*                    send packet using UDP socket
*                    Give semaphore for object detection every 0.3Hz
*priority         -   RT_MAX-1
*************************************************************/
void* Streaming_thread(void*)
{
    //Timing and deadline analysis
    double C_avg_streaming = 0.0;
    double C_sum_streaming = 0.0;
    double WCET_avg = 0.0;
    double WCET_sum = 0.0;
    int WCET_count=0;
    int missed_streaming = 0;
    int streaming_count =-1;

    try {
        UDPSocket sock; //UDP socket
        int jpegqual =  ENCODE_QUALITY; // Compression Parameters
        Mat frame, send;
        vector < uchar > encoded;
        VideoCapture cap(1); // Grab the frame
        if (!cap.isOpened()) {
            cerr << "OpenCV Failed to open camera";
            exit(1);
        }

        while (!abortS1) {
	          sem_wait(&Streaming_sem); //Blocked for semaphore released by sequencer
            gettimeofday(&start_streaming, (struct timezone *)0); //Time at start of streaming
            cap >> frame;
            streaming_count++;
            if(frame.size().width==0)continue;//error if the frame is width is 0

            resize(frame, send, Size(FRAME_WIDTH, FRAME_HEIGHT), 0, 0, INTER_LINEAR);//Resize the frame as per configuration
            vector < int > compression_params;
            compression_params.push_back(CV_IMWRITE_JPEG_QUALITY);
            compression_params.push_back(jpegqual);

            imencode(".jpg", send, encoded, compression_params);//encode frame to raw bytes
            imshow("send", send); //Display image
            waitKey(1);//Mandatory for imshow

            int total_pack = 1 + (encoded.size() - 1) / PACK_SIZE; //Number of packets for frame transfer, each packet - 4096 bytes
            int ibuf[1];
            ibuf[0] = total_pack;
            sock.sendTo(ibuf, sizeof(int), servAddress, servPort);//Send the number of packets for a frame

            for (int i = 0; i < total_pack; i++)
                sock.sendTo( & encoded[i * PACK_SIZE], PACK_SIZE, servAddress, servPort); //Send all packets

	         if(streaming_count % 45 == 0) //Every 0.3Hz give semaphore for object detection
           {
            pthread_mutex_lock(&processing); //Disable preemption
            imwrite("images.jpg",send);//Save the frame for further analysis
	          pthread_mutex_unlock(&processing);
            sem_post(&Detection_sem);//Semaphore for sending the labels
           }
           //WCET and timing analysis
           if(streaming_count>90) //Skip the setup WCET
           {
           gettimeofday(&stop_streaming, (struct timezone *)0);
           double diff = ((int)(stop_streaming.tv_sec-start_streaming.tv_sec))*1000 + ((int)(stop_streaming.tv_usec-start_streaming.tv_usec))/1000;
           C_sum_streaming += diff;
           C_avg_streaming = C_sum_streaming/((streaming_count*1.0)-90.0);
           if(diff > WCET_streaming)
           {
            WCET_count++;
            WCET_streaming = diff;
            WCET_sum += WCET_streaming;
            WCET_avg = WCET_sum/(WCET_count*1.0);
           }
           if (diff>streaming_deadline)
           {
             missed_streaming++;
           }
           acc_streaming = 100.0*(1.0 - (double)(missed_streaming/((streaming_count*1.0)-90.0)));

           syslog(LOG_CRIT, "Streaming: C1 = %.2f ms | Avg. C1 = %.2f ms | WCET1 = %.2f ms | Avg. WCET1 = %.2f ms | D1 = %.2f ms | Deadline_Met_Pct = %.2f\n",diff,C_avg_streaming, WCET_streaming, WCET_avg,streaming_deadline,acc_streaming);
          }
        }

    } catch (SocketException & e) {
        cerr << e.what() << endl;
        exit(1);
  }
}


/************************************************************
* Detection Thread - Waits for semaphore released by streaming thread
*		     Run python script
*                    Give semaphore for sending labels every 0.3Hz
* priority         - RT_MAX-3
*************************************************************/
void* Detection_thread(void*)
{
    double C_avg_detection = 0.0;
    double C_sum_detection = 0.0;
    double WCET_avg = 0.0;
    double WCET_sum = 0.0;
    int WCET_count=0;
    int missed_detection = 0;
    int detection_count = 0;
    Py_Initialize();
    while(!abortS2){
    sem_wait(&Detection_sem); //Wait for Semaphore from streaming thread
    detection_count++;
    gettimeofday(&start_detection, (struct timezone *)0);
    PyRun_SimpleString("exec(open('object_detection.py').read())"); //Run the python script
    //WCET and timing analysis
	  if(detection_count>2) //Skip the setup time
    {
        gettimeofday(&stop_detection, (struct timezone *)0);
        double diff = ((int)(stop_detection.tv_sec-start_detection.tv_sec))*1000 + ((int)(stop_detection.tv_usec-start_detection.tv_usec))/1000;
        C_sum_detection += diff;
        C_avg_detection = C_sum_detection/((detection_count*1.0)-2.0);
        if(diff > WCET_detection)
        {
        WCET_count++;
        WCET_detection = diff;
        WCET_sum += WCET_detection;
        WCET_avg = WCET_sum/(WCET_count*1.0);
        }
        if (diff>detection_deadline)
        {
         missed_detection++;
        }
        acc_detection = 100*(1.0 - (double)(missed_detection/((detection_count*1.0)-2.0)));
        syslog(LOG_CRIT,"----------------------------------------Object Detection Started using AWS Rekognition----------------------------------------\n");
        syslog(LOG_CRIT, "Detection: C2 = %.2f ms | Avg. C2 = %.2f ms | WCET2 = %.2f ms | Avg. WCET2 = %.2f ms | D2 = %.2f ms | Deadline_Met_Pct = %.2f\n",diff,C_avg_detection, WCET_detection, WCET_avg,detection_deadline,acc_detection);
    }
    sem_post(&sendlabels_sem); //Give semaphore for sending the labels over UDP sockets
    }
    Py_Finalize();
}


/************************************************************
* Labels Thread    - Waits for semaphore released by detection thread
*		     Open UDP scoket on other port
*                    Send the data from AWS rekognition over UDP
* priority         - RT_MAX-2
*************************************************************/
void* labels_thread(void*)
{
    double C_avg_labels = 0.0;
    double C_sum_labels = 0.0;
    double WCET_avg = 0.0;
    double WCET_sum = 0.0;
    int WCET_count=0;
    int missed_labels = 0;
    int labels_count = 0;

    ifstream data_file; //file which stored AWS results

    UDPSocket sock;
    char buffer[10000];
    while(!abortS3)
    {
    sem_wait(&sendlabels_sem);//Wait for semaphore from detection thread
    labels_count++;
    gettimeofday(&start_labels, (struct timezone *)0);
    data_file.open("data.txt"); //Open file containing AWS data
    data_file.read(buffer,10000);
    sock.sendTo(buffer,10000, servAddress, servPort1);//send UDP data
    data_file.close();
    //WCET and deadline analysis
    if(labels_count>2)
    {
    gettimeofday(&stop_labels, (struct timezone *)0);
    double diff = ((int)(stop_labels.tv_sec-start_labels.tv_sec))*1000000 + ((int)(stop_labels.tv_usec-start_labels.tv_usec));
    C_sum_labels += diff;
    C_avg_labels = C_sum_labels/((labels_count*1.0)-2.0);
    if(diff > WCET_labels)
    {
    WCET_count++;
    WCET_labels = diff;
    WCET_sum += WCET_labels;
    WCET_avg = WCET_sum/(WCET_count*1.0);
    }
    if (diff/1000.0>labels_deadline)
    {
     missed_labels++;
    }
    acc_labels = 100*(1.0 - (double)(missed_labels/((labels_count*1.0)-2.0)));
    syslog(LOG_CRIT,"----------------------------------------Sending object detection results to Server----------------------------------------\n");
    syslog(LOG_CRIT, "Labels: C3 = %.2f us | Avg. C3 = %.2f us | WCET3 = %.2f us | Avg. WCET3 = %.2f us | D3 = %.2f ms | Deadline_Met_Pct = %.2f\n",diff,C_avg_labels, WCET_labels, WCET_avg,labels_deadline,acc_labels);
    syslog(LOG_CRIT,"----------------------------------------Starting video streaming to Server----------------------------------------\n");
    }
   }
}

/************************************************************
* main - Initialise semaphores and mutex and create all threads
*        set the scheduling policy to SCHED_FIFO
*************************************************************/

int main( int argc, char** argv )
{
    syslog(LOG_CRIT, "Starting Demo------------------------------Starting Demo\n\n");
    //Initilase semaphores
    sem_init(&Streaming_sem,0,0);
    sem_init(&Detection_sem,0,0);
    sem_init(&sendlabels_sem,0,0);
    sem_init(&Sequencer_sem,0,1);

    struct timeval current_time_val;
    int i, rc, scope;
    cpu_set_t threadcpu;
    pthread_t threads[NUM_THREADS];
    threadParams_t threadParams[NUM_THREADS];
    pthread_attr_t rt_sched_attr[NUM_THREADS];
    int rt_max_prio, rt_min_prio;
    struct sched_param rt_param[NUM_THREADS];
    struct sched_param main_param;
    pthread_attr_t main_attr;
    pid_t mainpid;
    cpu_set_t allcpuset;

   if ((argc < 4) || (argc > 4)) { // Test for correct number of arguments
        cerr << "Usage: " << argv[0] << " <Server> <Server Port><server Port 1>\n";
        exit(1);
    }

    servAddress = argv[1]; // server address
    servPort = Socket::resolveService(argv[2], "udp");//One port for video streaming
    servPort1 = Socket::resolveService(argv[3], "udp");//One port for AWS labels

    //Initialise mutex
    if(pthread_mutex_init(&processing, NULL) !=0)
    {
      printf("mutex initialization failed\n");
      return 1;
    }
    if(pthread_mutex_init(&writing, NULL) !=0)
    {
      printf("mutex initialization failed\n");
      return 1;
    }

    printf("Starting Streaming and object-detection demo\n");

    mainpid=getpid();
    //Find the rt_max and min priority
    rt_max_prio = sched_get_priority_max(SCHED_FIFO);
    rt_min_prio = sched_get_priority_min(SCHED_FIFO);

    rc=sched_getparam(mainpid, &main_param);
    main_param.sched_priority=rt_max_prio;

    //set scheduler to SCHED_FIFO
    rc=sched_setscheduler(getpid(), SCHED_FIFO, &main_param);

    if(rc < 0) perror("main_param");
    pthread_attr_getscope(&main_attr, &scope);

    if(scope == PTHREAD_SCOPE_SYSTEM)
      printf("PTHREAD SCOPE SYSTEM\n");
    else if (scope == PTHREAD_SCOPE_PROCESS)
      printf("PTHREAD SCOPE PROCESS\n");
    else
      printf("PTHREAD SCOPE UNKNOWN\n");

    printf("rt_max_prio=%d\n", rt_max_prio);
    printf("rt_min_prio=%d\n", rt_min_prio);

    //Set pthread_atrributes for all threads
    for(i=0; i < NUM_THREADS; i++)
    {

      CPU_ZERO(&threadcpu);
      CPU_SET(3, &threadcpu);

      rc=pthread_attr_init(&rt_sched_attr[i]);
      rc=pthread_attr_setinheritsched(&rt_sched_attr[i], PTHREAD_EXPLICIT_SCHED);
      rc=pthread_attr_setschedpolicy(&rt_sched_attr[i], SCHED_FIFO);
      //rc=pthread_attr_setaffinity_np(&rt_sched_attr[i], sizeof(cpu_set_t), &threadcpu);

      rt_param[i].sched_priority=rt_max_prio-i;
      pthread_attr_setschedparam(&rt_sched_attr[i], &rt_param[i]);

      threadParams[i].threadIdx=i;
    }

    printf("Service threads will run on %d CPU cores\n", CPU_COUNT(&threadcpu));

    // Create Service threads which will block awaiting release for:

    // Streaming_thread = RT_MAX-1	@ 15 Hz

    rt_param[1].sched_priority=rt_max_prio-1;
    pthread_attr_setschedparam(&rt_sched_attr[1], &rt_param[1]);
    rc=pthread_create(&threads[1],               // pointer to thread descriptor
                      &rt_sched_attr[1],         // use specific attributes
                      //(void *)0,               // default attributes
                      Streaming_thread,                 // thread function entry point
                      (void *)&(threadParams[1]) // parameters to pass in
                     );
    if(rc < 0)
        perror("pthread_create for Streaming thread");
    else
        printf("pthread_create successful for Streaming thread\n");

    // Detection_thread = RT_MAX-3	@ 0.3 Hz

    rt_param[2].sched_priority=rt_max_prio-3;
    pthread_attr_setschedparam(&rt_sched_attr[2], &rt_param[2]);
    rc=pthread_create(&threads[2], &rt_sched_attr[2], Detection_thread, (void *)&(threadParams[2]));
    if(rc < 0)
        perror("pthread_create for Detection thread");
    else
        printf("pthread_create successful for Detection thread\n");

    // Labels_thread = RT_MAX-2	@ 0.3 Hz

    rt_param[3].sched_priority=rt_max_prio-2;
    pthread_attr_setschedparam(&rt_sched_attr[3], &rt_param[3]);
    rc=pthread_create(&threads[3], &rt_sched_attr[3], labels_thread, (void *)&(threadParams[3]));
    if(rc < 0)
        perror("pthread_create for Labels thread");
    else
        printf("pthread_create successful for Labels thread\n");

    printf("Start sequencer\n");

    // Sequencer = RT_MAX	@ 30 Hz

    rt_param[0].sched_priority=rt_max_prio;
    pthread_attr_setschedparam(&rt_sched_attr[0], &rt_param[0]);
    rc=pthread_create(&threads[0], &rt_sched_attr[0], Sequencer_thread, (void *)&(threadParams[0]));
    if(rc < 0)
        perror("pthread_create for Sequencer thread");
    else
        printf("pthread_create successful for Sequencer thread\n");

   cvNamedWindow("send", CV_WINDOW_AUTOSIZE);

   //Wait for all threads before exiting
   for(i=0;i<NUM_THREADS;i++)
       pthread_join(threads[i], NULL);

   printf("\nDemo Complete\n");
   //Destroy mutexes
   pthread_mutex_destroy(&processing);
   pthread_mutex_destroy(&writing);
}
