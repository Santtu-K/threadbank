#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>

#include "client_command.c"
#include "my_log.c"

#define QUEUESIZE 10 // deskien määrä

FILE *f; // Lokitiedosto

int n_cust = 0; // Serveriin yhdistäneiden asiakkaiden määrä
int queue_len[QUEUESIZE]; // deskien jonojen tilanne
pthread_t *all_desk; // deskit
void *thread_results[QUEUESIZE]; 
pthread_mutex_t lukko; // Mutex, jonka haltija voi muokata tietokantaa
int sukat[QUEUESIZE + 1];

// sig handler SIGINT/SIGTERM varten
struct sigaction sig;
int pipefd[2];

void ready_up(int to) { // Lähettää "ready" kuittauksen
    char *msg = "ready\n";
    assert(write(to, msg, sizeof(msg)) == sizeof(msg));
}

void copydata(int from,int to) {
  char buf[1024];
  int amount;
  while ((amount = read(from, buf, sizeof(buf))) > 0) {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL); // Hallittu alasajo

    char *reset = calloc(1024, sizeof(char));
    char *cmd = calloc(1024, sizeof(char)); // Saatu komento

    memcpy(cmd, reset, 1024); // Edellisen komennon nollaus
    memcpy(cmd, buf, amount);
    
    pthread_mutex_lock(&lukko); // Mutex:in otto
    int ret = give_cmd(cmd, from, amount);
    if (ret != 0) {printf("Error with taking command\n");log_up("Error with taking command\n");}
    pthread_mutex_unlock(&lukko); // Mutex:in luovutus
    
    free(cmd);
    free(reset);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  }
  assert(amount >= 0);
}

void *desk_thread(void *arg) {
    // Yksittäisen deski
    int desk_num = *(int*) arg; // Deskin numero
    free(arg);
    
    char desk_name[20];
    sprintf(desk_name, "./desk%d", desk_num); // Socketin nimi
    
    struct sockaddr_un address;
    int sock, conn;
    socklen_t addrLength;

    sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sock < 0){
        printf("Error with creating socket for desk: %d\n", desk_num);
        log_up("Error with creating socket for desk: %d\n", desk_num);
        pthread_exit(arg);
    }

    /* Remove any preexisting socket (or other file) */
    unlink(desk_name);
    
    address.sun_family = AF_UNIX;       /* Unix domain socket */
    strcpy(address.sun_path, desk_name);

    /* The total length of the address includes the sun_family 
        element */
    addrLength = sizeof(address.sun_family) + strlen(address.sun_path);

    int temp = bind(sock, (struct sockaddr *) &address, addrLength);
    if (temp != 0) {
        printf("Error with binding socket for desk: %d\n", desk_num);
        log_up("Error with binding socket for desk: %d\n", desk_num);
        pthread_exit(arg);
    }
    assert(listen(sock, 10) == 0);
    log_up("Desk: %d is ready\n", desk_num);
    sukat[desk_num] = sock;

    while ((conn = accept(sock, (struct sockaddr *) &address, 
                            &addrLength)) >= 0) {
        log_up("Customer connected to desk: %d\n", desk_num);
        printf("Customer connected to desk: %d\n", desk_num);

        ready_up(conn);
        copydata(conn,STDOUT_FILENO); // Asiakas käyttää pankkia

        log_up("Customer done with desk: %d\n", desk_num);
        printf("Customer done with desk: %d\n", desk_num);
        
        queue_len[desk_num - 1]--;
        close(conn);
    }
    assert(conn >= 0);
    
    close(sock);
    pthread_exit(arg);
}

void connect_client(int desknum, int to) { // Lähettää asiakkaalle deskin johon yhdistää
    char desk_name[20];
    int maara = sprintf(desk_name, "./desk%d", desknum);
    int ret = write(to, desk_name, maara);
    if (ret != maara) {
        printf("Error with connecting client to desk: %d\n", desknum);
        log_up("Error with connecting client to desk: %d\n", desknum);
    }
}

int get_desk(){ // Löytää deskin, jossa lyhyin jono
    int shortest = queue_len[0];
    int res_desk = 1;
    for (int i = 1; i < QUEUESIZE; i++) {
        int curr_queue = queue_len[i];
        if (curr_queue < shortest){
            shortest = curr_queue;
            res_desk = i + 1;
        }
    }
    queue_len[res_desk - 1]++;
    return res_desk;
}

static int sigpipe = 0; // Signal handler putki

static void sig_usr(int signo) { // Hallittu alasjo
	printf("\nReceived SIGINT/SIGTERM\n");
    for(int desknum = 0; desknum < QUEUESIZE; desknum++) { // Deskien terminointi
        printf("Shutting down desk: %d\n", desknum + 1);
        int ret = pthread_cancel(all_desk[desknum]);
        if (ret != 0) {
            printf("Error shutting down desk: %d\n", desknum +1);
            log_up("Error shutting down desk: %d\n", desknum + 1);
        }
    }

    for (int desknum = 0; desknum < QUEUESIZE; desknum++){ 
        assert(pthread_join(all_desk[desknum],&thread_results[desknum])==0);
        printf("Desk: %d has finished execution\n", desknum + 1);
        log_up("Desk: %d has finished execution\n", desknum + 1);
    }
    for (int i = 0; i <=QUEUESIZE; i++) { // Socketien sulkeminen
        close(sukat[i]);
    }
    for (int i = 1; i <= QUEUESIZE; i++) { // Socketien poisto
        char name[10];
        sprintf(name, "./desk%d", i);
        unlink(name);
    }
    unlink("./main_desk");
    log_up("In total: %d customers connected to the server\n", n_cust);
    log_up("Shutting down server\n");
    printf("Shutting down server\n");
    pthread_mutex_destroy(&lukko);
    free(all_desk);
    exit(0); // Ohjelman lopetus
}

int main(void) {
    f = fopen("logfile.log", "w"); // Lokitiedoston avaus
    if (f == NULL) {printf("Error opening log file"); return -1;}
    log_up("Opened the log file succesfully\n");

    pthread_mutex_init(&lukko, NULL); // Mutex:in alustus

    // Sig handling
    assert(pipe(pipefd) == 0);
    sigpipe = pipefd[1];
    fcntl(sigpipe,F_SETFL,O_NONBLOCK);

    sigemptyset(&sig.sa_mask);
    sig.sa_flags= SA_RESTART;  // restart interrupted system calls
    sig.sa_handler = sig_usr;

    assert((sigaction(SIGINT,&sig,NULL)) == 0); 
    assert((sigaction(SIGTERM,&sig,NULL)) == 0); 
    // init desk threads
    all_desk = calloc(QUEUESIZE, sizeof(pthread_t));

    for (int desk_num = 0; desk_num < QUEUESIZE; desk_num++) { // Deskien luonti
        int* a= malloc(sizeof(int));
        *a = desk_num + 1;
        int ret = pthread_create(&all_desk[desk_num], NULL, &desk_thread, a);
        if (ret != 0) {
            printf("Error creating desk: %d\n", *a);
            log_up("Error creating desk: %d\n", *a);
            return 1;
        }
    }

    // Main socketin luonti
    struct sockaddr_un address;
    int sock, conn;
    socklen_t addrLength;

    sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("Error creating main socket\n");
        log_up("Error creating main socket\n");
        return 1;
    } else {
        printf("Main socket created succesfully\n");
        log_up("Main socket created succesfully\n");
    }


    /* Remove any preexisting socket (or other file) */
    unlink("./main_desk");

    address.sun_family = AF_UNIX;       /* Unix domain socket */
    strcpy(address.sun_path, "./main_desk");

    /* The total length of the address includes the sun_family 
        element */
    addrLength = sizeof(address.sun_family) + strlen(address.sun_path);

    int temp = bind(sock, (struct sockaddr *) &address, addrLength);
    if (temp != 0) {
        printf("Error binding main socket\n");
        log_up("Error binding main socket\n");
        perror("Error:");
        return 1;
    }
    assert(listen(sock, 10) == 0);
    sukat[0] = sock;

    while ((conn = accept(sock, (struct sockaddr *) &address, 
                            &addrLength)) >= 0) {
        n_cust++;
        log_up("%dth customer has connected to the main socket\n", n_cust);
        printf("---- customer connected to main socket\n");
        int bestdesk = get_desk();
        connect_client(bestdesk, conn);
        close(conn);
    }
    
    assert(conn >= 0);
    free(all_desk);
    pthread_mutex_destroy(&lukko);
    close(sock);
    return 0;
}
