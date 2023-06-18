#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>      // libreria C per la gestione delle situazioni di errore.
#include <string.h>
#include <netdb.h>      
#include <sys/types.h>
#include <sys/socket.h> //libreria C per i socket.
#include <arpa/inet.h>  

#define MAX_SIZE 1024   //dimensione max del buffer
#define ACK_SIZE 64     
#define WELCOME_SIZE 108 
#define APP_ACK 39       
#define ID_SIZE 11 //10 byte per la tessera sanitaria più un byte per il terminatore

//Legge esattamente count byte s iterando opportunamente le letture. Legge anche se viene interrotta da una System Call.
ssize_t full_read(int fd, void *buf, size_t count) {
    size_t nleft;
    ssize_t nread;
    nleft = count;
    while (nleft > 0) {  // repeat finchè non ci sono left
        if ((nread = read(fd, buf, nleft)) < 0) {
            if (errno == EINTR) 
            continue; // Se si verifica una System Call che interrompe ripeti il ciclo
            else exit(nread);
        } else if (nread == 0) 
        break; // Se sono finiti, esci
        nleft -= nread;
        buf += nread;
    }
    buf = 0;
    return nleft;
}


//Scrive esattamente count byte s iterando opportunamente le scritture. Scrive anche se viene interrotta da una System Call.
ssize_t full_write(int fd, const void *buf, size_t count) {
    size_t nleft;
    ssize_t nwritten;
    nleft = count;
    while (nleft > 0) {          //repeat finchè non ci sono left
        if ((nwritten = write(fd, buf, nleft)) < 0) {
            if (errno == EINTR) 
            continue; //Se si verifica una System Call che interrompe ripeti il ciclo
            else exit(nwritten); //Se non è una System Call, esci con un errore
        }
        nleft -= nwritten;
        buf += nwritten;
    }
    buf = 0;
    return nleft;
}


int main() {
    int socket_fd;
    struct sockaddr_in server_addr;
    char start_bit, report, buf[MAX_SIZE], ID[ID_SIZE];

    start_bit = '0'; //Inizializziamo il bit a 0 per inviarlo al ServerVerifica

    //Creazione del descrittore del socket
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket() error");
        exit(1);
    }

    //Valorizzazione struttura
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1026);

    //Conversione dell’indirizzo IP, preso in input come stringa in formato dotted, in un indirizzo di rete in network order.
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("inet_pton() error");
        exit(1);
    }

    //Effettua connessione con il server
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() error");
        exit(1);
    }

    //Invia un bit di valore 0 al ServerVerifica per informarlo che la comunicazione deve avvenire con l'AppVerifica
    if (full_write(socket_fd, &start_bit, sizeof(char)) < 0) {
        perror("full_write() error");
        exit(1);
    }

    //Riceve il benvenuto dal ServerVerifica
    if (full_read(socket_fd, buf, WELCOME_SIZE) < 0) {
        perror("full_read() error");
        exit(1);
    }
    printf("%s\n\n", buf);

    //Inserimento codice tessera sanitaria
    while (1) {
        printf("Inserisci codice tessera sanitaria: ");
        if (fgets(ID, MAX_SIZE, stdin) == NULL) {
            perror("fgets() error");
            exit(1);
        }

        //Controllo sull'input dell'utente
        if (strlen(ID) != ID_SIZE) printf("Numero caratteri tessera sanitaria non corretto,Riprova\n\n");
        else {

            //Andiamo a inserire il terminatore al posto dell'invio inserito dalla fgets, poichè questo veniva contato ed inserito come carattere nella stringa
            ID[ID_SIZE - 1] = 0;
            break;
        }
    }

    //Invio del numero di tessera sanitaria da convalidare al server verifica
    if (full_write(socket_fd, ID, ID_SIZE)) {
        perror("full_write() error");
        exit(1);
    }

    //Ricezione dell'ack
    if (full_read(socket_fd, buf, ACK_SIZE) < 0) {
        perror("full_read() error");
        exit(1);
    }
    printf("\n%s\n\n", buf);

    printf("Convalida in corso..\n\n");

    //Facciamo attendere 3 secondi per completare l'operazione di verifica
    sleep(3);
    
    //Riceve esito scansione Green Pass dal ServerVerifica
    if (full_read(socket_fd, buf, APP_ACK) < 0) {
        perror("full_read() error");
        exit(1);
    }
    printf("%s\n", buf);

    close(socket_fd);

    exit(0);
}
