#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

char desk[20]; // Saatu deski

void copydata(int from,int to) { // Tiedon kaksisuuntainen välitys client ja desk socketien välillä
  char buf[1024];
  int amount;

  while ((amount = read(from, buf, sizeof(buf))) > 0) {
    assert((write(to, buf, amount) == amount));
    
    char response[1024];
    int resp_len;
    assert((resp_len = read(to, response, sizeof(response))) > 0);
    if (strstr(response, "quittin\n") != NULL) {
      printf("quittin\n");
      break;
    }
    assert(write(STDOUT_FILENO, response, resp_len) == resp_len);
  }
  assert(amount >= 0);
}

void get_ready(int from){ // "ready" kuittauksen saaminen
  char rcv[10];
  int amount = read(from, rcv, sizeof(rcv));
  assert(amount >= 0);
  printf("%s", rcv);
}

void get_desk(int from){ // Deskin saaminen 
  int amount;

  amount = read(from, desk, sizeof(desk));
  assert(amount >= 0);
}

int main(void) {
  setvbuf(stdin,NULL,_IOLBF,0);
  setvbuf(stdout,NULL,_IOLBF,0);
  printf("Connecting to the bank, please wait.\n");
  
  // Socket muuttujat
  struct sockaddr_un address, desk_address;
  int sock, desk_sock;
  size_t addrLength, desk_addrLength;
  
  // Yhdistys ensin main socketiin:
  assert((sock = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);

  address.sun_family = AF_UNIX;    /* Unix domain socket */
  strcpy(address.sun_path, "./main_desk");

  /* The total length of the address includes the sun_family 
     element */
  addrLength = sizeof(address.sun_family) + 
               strlen(address.sun_path);

  assert((connect(sock, (struct sockaddr *) &address, addrLength)) == 0);
  
  // Service desk socketiin yhdistäminen
  get_desk(sock); // Desk socketin saanti
  close(sock);

  assert((desk_sock = socket(PF_UNIX, SOCK_STREAM, 0)) >= 0);
  desk_address.sun_family = AF_UNIX;
  strcpy(desk_address.sun_path, desk);
  desk_addrLength = sizeof(desk_address.sun_family) + strlen(desk_address.sun_path);

  assert((connect(desk_sock, (struct sockaddr *) &desk_address, desk_addrLength)) == 0);
  
  get_ready(desk_sock); // "ready" kuittauksen vastaanotto
  copydata(STDIN_FILENO,desk_sock); // Serverin kanssa keskusteleminen

  close(desk_sock);
    
  return 0;
}
