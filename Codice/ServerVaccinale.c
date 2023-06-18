#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>      // libreria C per la gestione delle situazioni di errore.
#include <string.h>
#include <fcntl.h> 
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/socket.h> // libreria C per i socket
#include <arpa/inet.h>  // contiene le definizioni per le operazioni Internet.
#include <time.h>
#include <signal.h>     // libreria C che consente l'uso delle funzioni per la gestione dei segnali fra processi.
#define MAX_SIZE 2048   // dimensione max del buf
#define ID_SIZE 11        //dimensione del codice della tessera (10 byte + 1 byte per il terminatore)

//Pacchetto dell'ASL contenente il numero di tessera sanitaria di un green pass ed il suo report di validità
typedef struct  {
    char ID[ID_SIZE];
    char report;
} REPORT;

//Permette di salvare una data
typedef struct {
    int day;
    int month;
    int year;
} DATE;

//Pacchetto inviato dal centro vaccinale al server vaccinale contentente il numero di tessera sanitaria dell'utente, la data di inizio e fine del GP
typedef struct {
    char ID[ID_SIZE];
    char report; //0 GP non valido, 1 GP valido
    DATE start_date;
    DATE expire_date;
} GP_REQUEST;

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

//Handler che cattura il segnale CTRL-C e stampa un messaggio di arrivederci.
void handler (int sign){
    if (sign == SIGINT) {
        printf("\n Uscita.. \n");
        sleep(2); //aspetta 2 secondi prima della prossima operazione
        printf("*Grazie per aver utilizzato il nostro servizio*\n");
        exit(0);
    }
}

//Invia un GP richiesto dal ServerVerifica
void send_gp(int connect_fd) {
    char report, ID[ID_SIZE];
    int fd;
    GP_REQUEST gp;

    //Riceve il numero di tessera dal ServerVerifica
    if (full_read(connect_fd, ID, ID_SIZE) < 0) {
        perror("full_read() error");
        exit(1);
    }
    //Apre il file rinominato "ID", cioè il codice ricevuto dal ServerVerifica
    fd = open(ID, O_RDONLY, 0777);
    /* Se il numero di tessera sanitaria inviato dall'AppVerifca non esiste la variabile globale errno cattura l'evento ed in quel caso
       invia un report uguale ad 1 al ServerVerifica, che a sua volta aggiornerà l'AppVerifica dell'inesistenza del codice. In caso
       contrario invierà un report uguale a 0 per indicare che l'operazione è avvenuta correttamente.
    */
    
    if (errno == 2) {
        printf("Numero tessera inesistente, riprovare.\n");
        report = '2';
        
        if (full_write(connect_fd, &report, sizeof(char)) < 0) {
            perror("full_write() error");
            exit(1);
        }
    } else {

        //Accede in modo esclusivo al file in lettura
        if (flock(fd, LOCK_EX) < 0) {
            perror("flock() error");
            exit(1);
        }
        
        //Lettura del GP dal file aperto
        if (read(fd, &gp, sizeof(GP_REQUEST)) < 0) {
            perror("read() error");
            exit(1);
        }

            if(flock(fd, LOCK_UN) < 0) {
            perror("flock() error");
            exit(1);
        }

        close(fd);
        report = '1';

        //Invia il report al ServerVerifica
        if (full_write(connect_fd, &report, sizeof(char)) < 0) {
            perror("full_write() error");
            exit(1);
        }

        //Mandiamo il GP richiesto al ServerVerifica che controllerà la validità
        if(full_write(connect_fd, &gp, sizeof(GP_REQUEST)) < 0) {
            perror("full_write() error");
            exit(1);
        }
    }
}

//Modifica il report di un GP, sotto richiesta dell'ASL
void modify_report(int connect_fd) {
    REPORT package;
    GP_REQUEST gp;
    int fd;
    char report;

    //Riceve il pacchetto dal ServerVerifica proveniente dall'ASL contenente numero di tessera ed il risultato del tampone
    if (full_read(connect_fd, &package, sizeof(REPORT)) < 0) {
        perror("full_read() error");
        exit(1);
    }

    //Apre il file contenente il GP relativo al numero di tessera ricevuto dall'ASL
    fd = open(package.ID, O_RDWR , 0777);

    if (errno == 2) {
        printf("Numero tessera inesistente, riprova.\n");
        report = '1';
    } else {
        //Accediamo modo esclusivo al file in lettura
        if(flock(fd, LOCK_EX | LOCK_NB) < 0) {
            perror("flock() error");
            exit(1);
        }
        //Legge il file aperto contenente il GP relativo al numero di tessera ricevuto dall'ASL
        if (read(fd, &gp, sizeof(GP_REQUEST)) < 0) {
            perror("read() error");
            exit(1);
        }

        //Assegna il report ricevuto dall'ASL al green pass
        gp.report = package.report;

        lseek(fd, 0, SEEK_SET);

        //Andiamo a sovrascrivere i campi di GP nel file binario con nome il numero di tessera sanitaria del green pass
        if (write(fd, &gp, sizeof(GP_REQUEST)) < 0) {
            perror("write() error");
            exit(1);
        }
        
        if(flock(fd, LOCK_UN) < 0) {
            perror("flock() error");
            exit(1);
        }
        report = '0';
    }

    //Invia il report al ServerVerifica
    if (full_write(connect_fd, &report, sizeof(char)) < 0) {
        perror("full_write() error");
        exit(1);
    }
}

//Funzione che tratta la comunicazione con il ServerVerifica, ricava il GP dal file system relativo al numero di tessera ricevuto e lo invia al ServerVerifica.
void SV_comunication(int connect_fd) {
    char start_bit;

    /*
        Il ServerVaccinale riceve un bit dal ServerVerifica, che può essere 0 o 1, siccome sono due funzioni differenti.
        Quando riceve 0  il ServerVaccinale gestirà la funzione per modificare il report di un GP.
        Quando riceve 1  il ServerVaccinale gestirà la funzione per inviare un GP al ServerVerifica.
    */
    if (full_read(connect_fd, &start_bit, sizeof(char)) < 0) {
        perror("full_read() error");
        exit(1);
    }
    if (start_bit == '0') modify_report(connect_fd);
    else if (start_bit == '1') send_gp(connect_fd);
    else printf("Dato non valido\n\n");
}

//Funzione che tratta la comunicazione con il CentroVaccinale e salva i dati ricevuti da questo in un filesystem.
void CV_comunication(int connect_fd) {
    int fd;
    GP_REQUEST gp;

    //Riceve il GP dal CentroVaccinale
    if (full_read(connect_fd, &gp, sizeof(GP_REQUEST)) < 0) {
        perror("full_write() error");
        exit(1);
    }

    //Quando viene generato un nuovo green pass è valido di defualt
    gp.report = '1';

    //Per ogni Tessera Sanitaria crea un file contenente i dati ricevuti.
    if ((fd = open(gp.ID, O_WRONLY| O_CREAT | O_TRUNC, 0777)) < 0) {
        perror("open() error");
        exit(1);
    }
    //Andiamo a scrivere i campi di GP nel file binario con nome il numero di tessera sanitaria del green pass
    if (write(fd, &gp, sizeof(GP_REQUEST)) < 0) {
        perror("write() error");
        exit(1);
    }

    close(fd);
}

int main() {
    int listen_fd, connect_fd, package_size;
    struct sockaddr_in serv_addr;
    pid_t pid;
    char start_bit;
    signal(SIGINT,handler); //Cattura il segnale
    //Creazione del socket
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket() error");
        exit(1);
    }

    //Valorizzazione strutture
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(1025);

    //Assegnazione della porta al server
    if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind() error");
        exit(1);
    }

    //Mette il socket in ascolto in attesa di nuove connessioni
    if (listen(listen_fd, 1024) < 0) {
        perror("listen() error");
        exit(1);
    }

    for (;;) {

    printf("In attesa di nuovi dati\n\n");

        //Accetta una nuova connessione
        if ((connect_fd = accept(listen_fd, (struct sockaddr *)NULL, NULL)) < 0) {
            perror("accept() error");
            exit(1);
        }

        //Creazione del figlio;
        if ((pid = fork()) < 0) {
            perror("fork() error");
            exit(1);
        }

        //Porzione di codice eseguita dal figlio
        if (pid == 0) {
            close(listen_fd);

            /*
                Il ServerVaccinale riceve un bit come primo messaggio, che può essere 0 o 1, siccome ci sono due connessioni differenti.
                Quando riceve 1 il figlio gestirà la connessione con il CentroVaccinale.
                Quando riceve 0 il figlio gestirà la connessione con il ServerVerifica.
            */

            if (full_read(connect_fd, &start_bit, sizeof(char)) < 0) {
                perror("full_read() error");
                exit(1);
            }
            if (start_bit == '1') CV_comunication(connect_fd);
            else if (start_bit == '0') SV_comunication(connect_fd);
            else printf("Client non riconosciuto\n\n");

            close(connect_fd);
            exit(0);
        } else close(connect_fd);
    }
    exit(0);
}