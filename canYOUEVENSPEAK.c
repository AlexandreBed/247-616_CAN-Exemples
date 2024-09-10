#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <signal.h>

void sender(int can_id, const char *interface);
void receiver(const char *interface);

int main(int argc, char **argv) {
    int can_id = 0x009; // Remplacez par les 3 derniers chiffres de votre numéro de DA en hexadécimal
    const char *interface = (argc >= 2) ? argv[1] : "can0";

    printf("Programme de communication CAN\n");

    pid_t pid = fork();

    if (pid < 0) {
        perror("Erreur lors du fork");
        return -1;
    } else if (pid == 0) {
        // Processus fils - réception des messages CAN
        receiver(interface);
    } else {
        // Processus père - envoi des messages CAN
        int choix;
        while (1) {
            printf("Menu:\n1. Envoyer un message CAN\n2. Quitter\n");
            printf("Votre choix : ");
            scanf("%d", &choix);

            if (choix == 1) {
                sender(can_id, interface);
            } else if (choix == 2) {
                kill(pid, SIGKILL); // Arrête le processus fils
                wait(NULL); // Attendre la fin du processus fils
                break;
            } else {
                printf("Choix invalide. Réessayez.\n");
            }
        }
    }
    return 0;
}

void sender(int can_id, const char *interface) {
    int fdSocketCAN;
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;

    // Création du socket
    if ((fdSocketCAN = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Erreur de création du socket");
        return;
    }

    // Configuration de l'interface CAN
    strcpy(ifr.ifr_name, interface);
    ioctl(fdSocketCAN, SIOCGIFINDEX, &ifr);

    // Liaison du socket à l'interface CAN
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(fdSocketCAN, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Erreur de liaison");
        close(fdSocketCAN);
        return;
    }

    // Préparation et envoi de la trame CAN avec "009"
    frame.can_id = 0x009;
    frame.can_dlc = 8;  // Longueur des données
    memcpy(frame.data, "Wetsocks", frame.can_dlc);

    printf("Envoi du message CAN ID 0x%03X : %s\n", frame.can_id, frame.data);
    if (write(fdSocketCAN, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        perror("Erreur d'écriture");
    }

    close(fdSocketCAN);
}

void receiver(const char *interface) {
    int fdSocketCAN, nbytes, i;
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;

    printf("Processus fils - en attente de messages CAN\n");

    // Création du socket
    if ((fdSocketCAN = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Erreur de création du socket");
        exit(-1);
    }

    // Configuration de l'interface CAN
    strcpy(ifr.ifr_name, interface);
    ioctl(fdSocketCAN, SIOCGIFINDEX, &ifr);

    // Liaison du socket à l'interface CAN
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(fdSocketCAN, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Erreur de liaison");
        close(fdSocketCAN);
        exit(-1);
    }

    // Configuration des filtres CAN pour les messages à recevoir
    struct can_filter rfilter[2];
    rfilter[0].can_id = 0x115;
    rfilter[0].can_mask = 0xFFF;
    rfilter[1].can_id = 0x565;
    rfilter[1].can_mask = 0xFFF;

    setsockopt(fdSocketCAN, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));

    // Boucle pour recevoir et afficher les messages CAN
    while (1) {
        nbytes = read(fdSocketCAN, &frame, sizeof(struct can_frame));
        if (nbytes < 0) {
            perror("Erreur de lecture");
            close(fdSocketCAN);
            exit(-1);
        }

        printf("Message CAN reçu : ID 0x%03X ", frame.can_id, frame.can_dlc);
        for (i = 0; i < frame.can_dlc; i++)
            printf("%02X ", frame.data[i]);
        printf("\n");
    }

    close(fdSocketCAN);
}
