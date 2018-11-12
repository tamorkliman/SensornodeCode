/*
 *Written by
 *Isaak Cherdak
 *
 *Credit for general lessons to:
 *http://www.csd.uoc.gr/~hy556/material/tutorials/cs556-3rd-tutorial.pdf
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <mysql/mysql.h>
#include <time.h>

#define IPPROTO_DEFAULT 0
#define MAX_CONNECTIONS 1
#define NUM_SENSORS 6
#define TIMEOUT_NOPACKET 6

#define REQUEST_ID_STRING "s\r"
#define REQUEST_NUM_DEVICES_STRING "c\r"
#define REQUEST_DEVICE_DATA "r" //note need to append device number
#define CARRIGE_RETURN "\r"

typedef struct sockaddr_in sockaddr_in_s;

void stderror(char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char **argv) {
  FILE *fp = NULL;
  sockaddr_in_s server_addr;
  sockaddr_in_s client_addr;
  socklen_t client_addr_len;
  char **sendstr = calloc(NUM_SENSORS, sizeof(char *));
  sendstr[0] = "s1\r";
  sendstr[1] = "c1\r";
  sendstr[2] = "r3\r";
  sendstr[3] = "r4\r";
  sendstr[4] = "r5\r";
  sendstr[5] = "r6\r";

  if (argc != 2) {
    fprintf(stderr, "Usage: %s port\n", argv[0]);
  }

  int port = atoi(argv[1]);
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_DEFAULT);
  if (sockfd == -1) {
    stderror("Couldn't create socket");
  }

  // NOTE: htons converts port to a network format
  int bindstatus = bind(sockfd, (struct sockaddr *) &server_addr, sizeof(sockaddr_in_s));
  if (bindstatus == -1) {
    stderror("Bind unsuccessful");
  }

  int listenstatus = listen(sockfd, MAX_CONNECTIONS);
  if (listenstatus == -1) {
    stderror("Couldn't listen on socket");
  }

  // MYSQL *conn;
  //
  // char *server = "db-01.soe.ucsc.edu";
  // char *user = "arboretum_data";
  // char *password = "icherdak_backend";
  // char *database = "arboretum_data";

  for (;;) {

    int8_t no_receive_count = 0;
    client_addr_len = sizeof(client_addr);
    int clientfd = -1;
    while (clientfd == -1) {
      clientfd = accept(sockfd, (struct sockaddr *) &client_addr, &client_addr_len);
      if (clientfd == -1) {
        printf("Couldn't accept connection, retrying...\n");
      }
    }
    printf("Started new connection with client\n");
    int sendcount = 0;
    int attemptsend = 1;
    while (1) {
      if (attemptsend && send(clientfd, sendstr[sendcount], strlen(sendstr[sendcount]), 0) != strlen(sendstr[sendcount])) {
        printf("Connection error\n");
        break;
      }
      printf("Request Packet: %s\n", sendstr[sendcount]);
      attemptsend = 0;
      char recvstr[101];

      sleep(2);

      int recvstatus = recv(clientfd, recvstr, 100, MSG_DONTWAIT);
      if (recvstatus == -1 && errno != EWOULDBLOCK) { //non-blocking
        stderror("Couldn't receive packet");
      } else if (recvstatus != -1 && recvstatus != 0) {
        recvstr[recvstatus] = '\0'; // theoretically shouldn't do anything
        printf("Received Packet: %s\n", recvstr);
        attemptsend = 1; // send next packet
        // FIXME: change back to NUMSENSORS
        //sendcount = (sendcount + 1) % NUM_SENSORS; // select next packet to send
        sendcount = (sendcount + 1) % 2; // select next packet to send
        no_receive_count = -1; // will become 0 at the end of the loop







        // if (!strchr(recvstr, '=')) {
        //   printf("Invalid data syntax: skipping DB insert\n");
        //   continue;
        // }
        //
        // conn = mysql_init(NULL);
        //
        // // Connect to database
        // if (!mysql_real_connect(conn, server,
        //       user, password, database, 0, NULL, 0)) {
        //   fprintf(stderr, "Failed to insert to DB: %s\n", mysql_error(conn));
        //   continue;
        // }

        // // insert data into database
        // char query_str[256];
        // query_str[255] = '\0';
        // char *device_name = strtok(recvstr, "=\r\n");
        // time_t t = time(NULL);
        // struct tm tm = *localtime(&t);
        // char timestr[32];
        // sprintf(timestr, "%04d%02d%02d::%02d%02d%02d", tm.tm_year + 1900,
        //     tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        // if (device_name) {} // stop compiler from complaining
        // snprintf(query_str, 255, "%s ('%s', '%s', '%s')",
        //     "INSERT INTO device_data VALUES",
        //     "1",
        //     strtok(NULL, "\r\n"),
        //     timestr);
        // if (mysql_query(conn, query_str)) {
        //   //stderror((char *) mysql_error(conn));
        //   fprintf(stderr, "Failed to insert to DB: %s\n", mysql_error(conn));
        //   //exit(1);
        // } else {
        //   printf("DB insertion successful\n");
        // }

        //disconnect from DB
        // mysql_close(conn);

      } else if (recvstatus != 0) {
        printf("No packet received\n");
      } else {
        break; // disconnected
      }
      fp = fopen("stop-arboretum", "r");
      if (fp != NULL) {
        fclose(fp);
        break;
      }
      if (no_receive_count++ > TIMEOUT_NOPACKET) {
        printf("Didn't receive packet\n");
        break;
      }
    }
    if (clientfd != -1) close(clientfd); //we don't do that because it's already closed
    printf("Disconnected from client\n");
    fp = fopen("stop-arboretum", "r");
    if (fp != NULL) {
      fclose(fp);
      break;
    }
  }

  int closestatus = close(sockfd);
  if (closestatus == -1) {
    stderror("Couldn't close socket");
  }
  free(sendstr);
  printf("Server Closed\n");
  return 0;
}
