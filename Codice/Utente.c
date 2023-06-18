#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>      // libreria C per la gestione delle situazioni di errore.
#include <string.h>
#include <netdb.h>      
#include <sys/types.h>
#include <sys/socket.h> //libreria C per i socket.
#include <arpa/inet.h>  // contiene le definizioni per le operazioni Internet.

#define MAX_SIZE 1024   //dimensione max del buf
#define ID_SIZE 11      //dimensione del codice della tessera (10 byte + 1 byte per il terminatore)
#define ACK_SIZE 61     

//Definiamo il pacchetto applicazione per l'user da inviare al centro vaccinale
typedef struct {
    char name[MAX_SIZE];
    char surname[MAX_SIZE];
    char ID[ID_SIZE];
} VAX_REQUEST;

//Legge esattamente count byte s iterando opportunamente le letture. Legge anche se viene interrotta da una System Call.
ssize_t full_read(int fd, void *buf, size_t count) {
    size_t nleft;
    ssize_t nread;
    nleft = count;
    while (nleft > 0) {  
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

//Funzione per la creazione del pacchetto da inviare al centro vaccinale
VAX_REQUEST create_package() {
    char buf[MAX_SIZE];
    VAX_REQUEST create_pack;

    //Inserimento nome
    printf("Inserisci nome: ");
    if (fgets(create_pack.name, MAX_SIZE, stdin) == NULL) {
        perror("fgets() error");
    }
    //Andiamo a inserire il terminatore al posto dell'invio inserito dalla fgets, poichè questo veniva contato ed inserito come carattere nella stringa
    create_pack.name[strlen(create_pack.name) - 1] = 0;

    //Inserimento cognome
    printf("Inserisci cognome: ");
    if (fgets(create_pack.surname, MAX_SIZE, stdin) == NULL) {
        perror("fgets() error");
    }
    //Andiamo a inserire il terminatore al posto dell'invio inserito dalla fgets, poichè questo veniva contato ed inserito come carattere nella stringa
    create_pack.surname[strlen(create_pack.surname) - 1] = 0;

    //Inserimento codice tessera sanitaria
    while (1) {
        printf("Inserisci codice tessera sanitaria: ");
        if (fgets(create_pack.ID, MAX_SIZE, stdin) == NULL) {
            perror("fgets() error");
            exit(1);
        }
        //Controllo sull'input dell'utente: il numero di tessera deve essere esattamente di 10 caratteri
        if (strlen(create_pack.ID) != ID_SIZE) printf("Numero caratteri tessera sanitaria non corretto! Riprova\n\n");
        else {
            create_pack.ID[ID_SIZE - 1] = 0;
           break;
        }
    }
    return create_pack;
}

int main(int argc, char **argv) {
    int socket_fd, welcome_size, package_size;
    struct sockaddr_in server_addr;
    VAX_REQUEST package;
    char buf[MAX_SIZE];
    char **alias;
    char *addr;
	struct hostent *data; //struttura per utilizzare la gethostbyname

    if (argc != 2) {
        perror("usage: <host name>"); //perror: Produce un messaggio sullo standard error che descrive l’ultimo errore avvenuto durante una System call o una funzione di libreria.
        exit(1);
    }

    //Creazione del descrittore del socket
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket() error");
        exit(1);
    }

    //Valorizzazione struttura
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1024);

    //Conversione dal nome al dominio a indirizzo IP
    if ((data = gethostbyname(argv[1])) == NULL) {
        herror("gethostbyname() error");
		exit(1);
    }
	alias = data -> h_addr_list;

    //inet_ntop converte un indirizzo in una stringa:
    if ((addr = (char *)inet_ntop(data -> h_addrtype, *alias, buf, sizeof(buf))) < 0) {
        perror("inet_ntop() error");
        exit(1);
    }

    //Conversione dell’indirizzo IP, preso in input come stringa in formato dotted, in un indirizzo di rete in network order.
    if (inet_pton(AF_INET, addr, &server_addr.sin_addr) <= 0) {
        perror("inet_pton() error");
        exit(1);
    }

    //Effettua connessione con il server
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() error");
        exit(1);
    }
    //FullRead per leggere quanti byte invia il Centro Vaccinale
    if (full_read(socket_fd, &welcome_size, sizeof(int)) < 0) {
        perror("full_read() error");
        exit(1);
    }
    //Riceve il benevenuto dal centro vaccinale
    if (full_read(socket_fd, buf, welcome_size) < 0) {
        perror("full_read() error");
        exit(1);
    }
    printf("%s\n", buf);

    //Creazione del pacchetto da inviare al centro vaccinale
    package = create_package();

    //Invio del pacchetto richiesto al centro vaccinale
    if (full_write(socket_fd, &package, sizeof(package)) < 0) {
        perror("full_write() error");
        exit(1);
    }

    //Ricezione dell'ack
    if (full_read(socket_fd, buf, ACK_SIZE) < 0) {
        perror("full_read() error");
        exit(1);
    }
    printf("%s\n\n", buf);

    exit(0);
}
