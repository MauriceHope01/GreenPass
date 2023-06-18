#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>      // libreria C per la gestione delle situazioni di errore.
#include <string.h>
#include <netdb.h>      
#include <sys/types.h>
#include <sys/socket.h> //Libreria C per i socket.
#include <arpa/inet.h>  

#define MAX_SIZE 1024   //dimensione max del buf
#define ID_SIZE 11         
#define ACK_SIZE 61        
#define ASL_ACK 39

//Pacchetto dell'ASL contenente il numero di tessera sanitaria di un green pass ed il suo referto di validità
typedef struct  {
    char ID[ID_SIZE];
    char report;
} REPORT;

//Legge esattamente count byte s iterando opportunamente le letture. Legge anche se viene interrotta da una System Call.
ssize_t full_read(int fd, void *buf, size_t count) {
    size_t nleft;
    ssize_t nread;
    nleft = count;
    while (nleft > 0) {  // repeat finchè non ci sono left
        if ((nread = read(fd, buf, nleft)) < 0) {
            if (errno == EINTR) continue; // Se si verifica una System Call che interrompe ripeti il ciclo
            else exit(nread);
        } else if (nread == 0) break; // Se sono finiti, esci
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
            if (errno == EINTR) continue; //Se si verifica una System Call che interrompe ripeti il ciclo
            else exit(nwritten); //Se non è una System Call, esci con un errore
        }
        nleft -= nwritten;
        buf += nwritten;
    }
    buf = 0;
    return nleft;
}


int main(int argc, char **argv) {
    int socket_fd;
    struct sockaddr_in server_addr;
    REPORT package;
    char start_bit, buf[MAX_SIZE];

    start_bit = '1'; //Inizializziamo il bit a 1 da inviare al ServerVerifica

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

    //Invia un bit di valore 1 al ServerVerifica per informarlo che la comunicazione deve avvenire con l'ASL
    if (full_write(socket_fd, &start_bit, sizeof(char)) < 0) {
        perror("full_write() error");
        exit(1);
    }

    printf("*ASL*\n");
    printf("Immettere un numero di tessera sanitaria ed il referto di un tampone per invalidare o ripristinare un GP\n");

    while (1) {
        printf("Inserisci codice tessera sanitaria: ");
        if (fgets(package.ID, MAX_SIZE, stdin) == NULL) {
            perror("fgets() error");
            exit(1);
        }
        //Controllo sull'input dell'utente
        if (strlen(package.ID) != ID_SIZE) printf("Numero caratteri tessera sanitaria non corretto, devono essere esattamente 10! Riprovare\n\n");
        else {
            //Andiamo a inserire il terminatore al posto dell'invio inserito dalla fgets, poichè questo veniva contato ed inserito come carattere nella stringa
            package.ID[ID_SIZE - 1] = 0;
            break;
        }
    }

    while (1) {
        printf("Inserire 0 [GP Non Valido]\nInserire 1 [GP Valido]\n: ");
        scanf("%c", &package.report);

        //Controllo sull'input dell'utente, se !=0 o !=1 dovrà ripetere l'operazione
        if (package.report == '1' || package.report == '0') break;
        printf("Errore: input errato, riprova..\n\n");
    }

    // Controllo sull'input dell'utente
    if (package.report == '1') printf("\n Invio richiesta di ripristino GP\n");
    else printf("\n Invio richiesta di sospensione GP\n");

    //Invia pacchetto report al ServerVerifica
    if (full_write(socket_fd, &package, sizeof(REPORT)) < 0) {
        perror("full_write() error");
        exit(1);
    }
    //Riceve messaggio di report dal ServerVerifica
    if (full_read(socket_fd, buf, ASL_ACK) < 0) {
        perror("full_read() error");
        exit(1);
    }
    //Simuliamo un caricamento con la sleep
    sleep(2);
    //Stampa del messaggio del report ricevuto dal serverVerifica
    printf("%s\n", buf);

    exit(0);
}
