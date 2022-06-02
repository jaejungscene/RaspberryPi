#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "header/spiRW.h"
#include "header/mySocket.h"
#include "header/gpioRW.h"


#define IN 0
#define OUT 1
#define MAX_MSG 30
#define L_CHANNEL 3
#define R_CHANNEL 4
#define FRONT_PIN 17
#define BACK_PIN 22
#define WAIT_TIME 300000


int signal_from_main = 0;
int fd; //adc fd
int L_pressure; // value of left pressure sensor
int R_pressure; // value of right pressur esensor
int front_weight; // value of left pressure sensor
int back_weight; // value of right pressur esensor



void *alert_to_server(void *argv){
   printf("alert thread start\n");

   while(signal_from_main == 1){
      if( (L_pressure > 10 || R_pressure > 10) && (front_weight == 1 || back_weight == 1) )
         break;
   }

   int sock;
   struct sockaddr_in serv_addr;
   int str_len;

   sock = socket(PF_INET, SOCK_STREAM, 0); // PF_INET = IPv4, SOCK_STREAM = TCP // SOCK_DGRAM = UDP
   if(sock == -1) error_handling("socket() error");
   memset(&serv_addr, 0, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET; //IPv4
   serv_addr.sin_addr.s_addr = inet_addr(((char**)argv)[2]); // <-- 변경!!!!!!!!!
   serv_addr.sin_port = htons(atoi(((char**)argv)[3])); // <-- 변경!!!!!!!!

   printf("%s : %s\n", ((char**)argv)[2], ((char**)argv)[3]);
   while(signal_from_main == 1){
      if(L_pressure <= 0 && R_pressure <= 0 && front_weight==0 && back_weight==0){
         connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
         // if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1){
         //    error_handling("connect() error");
         // }
         printf("** send msg to server **\n");
         write(sock, "accident", sizeof("accident"));
         close(sock);
      }
   }

   pthread_exit(NULL);
}



void *pressure_sensor_worker(void *param){
   printf("pressure thread start\n");

   while(signal_from_main == 1){
      if((long)param == L_CHANNEL){
         L_pressure = readadc(fd, (long)L_CHANNEL);
         printf("%ld -> pressure: %3d\n", (long)L_CHANNEL, L_pressure);
      }
      else{
         R_pressure = readadc(fd, (long)R_CHANNEL);
         printf("%ld -> pressure: %3d\n", (long)R_CHANNEL, R_pressure);
      }
      usleep(WAIT_TIME);
   }

   pthread_exit(NULL);
}



void *weight_sensor_worker(void *param){
   printf("weight thread start\n");

   GPIOExport((long)param);
   GPIODirection((long)param, IN);
   int value;

   while(signal_from_main == 1){
      if((long)param == FRONT_PIN){
         front_weight = GPIORead(FRONT_PIN);
         printf("%ld -> weight: %3d\n", (long)FRONT_PIN, front_weight);
      }
      else{
         back_weight = GPIORead(BACK_PIN);
         printf("%ld -> weight: %3d\n", (long)BACK_PIN, back_weight);
      }
      usleep(WAIT_TIME);
   }

   GPIOUnexport((long)param);
   pthread_exit(NULL);
}



void *vibration_sensor_worker(){
   printf("thread01!!!\n");
   pthread_exit(NULL);
}


// ./(file) <port> <Server ip> <Server port>
int main(int argc, char *argv[]){
   printf("== automatic report start ==\n");
   pthread_t alert, L_pressure_sensor, R_pressure_sensor,
               front_weight_sensor, back_weight_sensor;

   int serv_sock, clnt_sock = -1; // socket filedescriptor
   struct sockaddr_in serv_addr, clnt_addr;
   socklen_t clnt_addr_size = sizeof(clnt_addr);
   char msg[MAX_MSG];

   serverPrepare(&serv_sock, &serv_addr, &argv[1]);
   clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_addr_size);
   if (clnt_sock == -1) error_handling("accept() error");
   printf("=== complete connection with %s ===\n", inet_ntoa(clnt_addr.sin_addr));

   int str_len = -1;
   while(1)
   {
      printf("wait read...\n");
      str_len = read(clnt_sock, msg, sizeof(msg));
      if (str_len == -1){
         error_handling("Err : read() error");
      }
      else if (!strcmp(msg, "1")){
         printf("** active **\n");
         signal_from_main = 1;
      }
      else if (!strcmp(msg, "0")){
         printf("** inactive **\n");
         signal_from_main = 0;
         continue;
         // close(clnt_sock);
      }
      

      fd = open(DEVICE, O_RDWR); // opening the DEVICE file as read/write
      if (fd <= 0) {
         printf( "Device %s not found\n", DEVICE);
         break;
      }
      if(prepare(fd) == -1){
         break;
      }

      pthread_create(&alert, NULL, alert_to_server, (void*)argv);
      pthread_create(&L_pressure_sensor, NULL, pressure_sensor_worker, (void*)L_CHANNEL);
      pthread_create(&R_pressure_sensor, NULL, pressure_sensor_worker, (void*)R_CHANNEL);
      pthread_create(&front_weight_sensor, NULL, weight_sensor_worker, (void*)FRONT_PIN);
      pthread_create(&back_weight_sensor, NULL, weight_sensor_worker, (void*)BACK_PIN);
      printf("============= check03 ===============\n");
      // pthread_create(&front_weight_sensor, NULL, vibration_sensor_worker, NULL);
      // pthread_create(&back_weight_sensor, NULL, vibration_sensor_worker, NULL);
      // pthread_join(alert, NULL);
      // pthread_join(L_pressure_sensor, NULL);
      // pthread_join(R_pressure_sensor, NULL);
      // pthread_join(front_weight_sensor, NULL);
      // pthread_join(back_weight_sensor, NULL);
   }

   printf("== automatic report finish ==\n");
   return 0;
}