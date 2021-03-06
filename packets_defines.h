#ifndef PACKET_DEFINES_H
#define PACKET_DEFINES_H

#include <stdint.h>


/* Type used to represent a packet number. */
typedef uint8_t opcode_t;
/* Length of a packet number (bytes) in the header. */
#define PKT_ID_SIZE sizeof(opcode_t)


/* Mask for a client message. */
#define CMSG_MASK 0 << 6
/* Mask for a server message. */
#define SMSG_MASK 1 << 6
/* Mask for an internal client message. */
#define CMSG_INT_MASK (1 << 7 | CMSG_MASK)
/* Mask for an internal server message. */
#define SMSG_INT_MASK (1 << 7 | SMSG_MASK)


#define CMSG(X) (CMSG_MASK | X)
#define SMSG(X) (SMSG_MASK | X)
#define CMSGI(X) (CMSG_INT_MASK | X)
#define SMSGI(X) (SMSG_INT_MASK | X)


/* Client asks for a list of neighbours. */
#define CMSG_NEIGHBOURS CMSG(0)
/* Client asks to become neighbour. */
#define CMSG_JOIN CMSG(1)
/* Client asks for a single neighbour from the target. */
#define CMSG_NEIGHBOUR_RESCUE CMSG(2)
/* Client asks for a list of machines that have a file. */
#define CMSG_SEARCH_REQUEST CMSG(3)
/* Client leaves the network. */
#define CMSG_LEAVE CMSG(4)
/* Client wants to download a file. */
#define CMSG_DOWNLOAD CMSG(5)


/* Server replies with it's direct neighbours. */
#define SMSG_NEIGHBOURS SMSG(0)
/* Server replies with "yes" or "no". */
#define SMSG_JOIN SMSG(1)
/* Server replies with a neighbour. */
#define SMSG_NEIGHBOUR_RESCUE SMSG(2)
/* Server replies with a list of machines that have a file. */
#define SMSG_SEARCH_REQUEST SMSG(3)
/* Server transfers the file. */
#define SMSG_DOWNLOAD SMSG(4)


/* Client contact server to notify it's ready. */
#define CMSG_INT_HANDSHAKE CMSGI(0)
/* Client contact server to announce it is closing. */
#define CMSG_INT_EXIT CMSGI(1)
/* Client contact server to ask for a file. */
#define CMSG_INT_SEARCH CMSGI(2)
/* Client contact server to download a file. */
#define CMSG_INT_DOWNLOAD CMSGI(3)


/* Server contact client to notify it's ready. */
#define SMSG_INT_HANDSHAKE SMSGI(0)
/* Server contact client to send list of machines that have a file. */
#define SMSG_INT_SEARCH SMSGI(1)
/* Server contact client to notify file has been downloaded. */
#define SMSG_INT_DOWNLOAD SMSGI(2)


/*
 * Codes that can be found inside the SMSG_INT_DOWNLOAD packet, field 'answer'.
 */
typedef enum smsg_int_download_answer_codes_s {
    /* File is already present locally. */
    ANSWER_CODE_LOCAL               = 0,
    /* Remote offline. */
    ANSWER_CODE_REMOTE_OFFLINE      = 1,
    /* File not found on remote. */
    ANSWER_CODE_REMOTE_NOT_FOUND    = 2,
    /* File is present on remote. */
    ANSWER_CODE_REMOTE_FOUND        = 3
} smsg_int_download_answer_codes_t;


#endif /* PACKET_DEFINES_H */
