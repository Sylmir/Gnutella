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


/* Server replies with it's direct neighbours. */
#define SMSG_NEIGHBOURS SMSG(0)
/* Server replies with "yes" or "no". */
#define SMSG_JOIN SMSG(1)
/* Server replies with a neighbour. */
#define SMSG_NEIGHBOUR_RESCUE SMSG(2)


/* Client contact server to notify it's ready. */
#define CMSG_INT_HANDSHAKE CMSGI(0)
/* Client contact server to announce it is closing. */
#define CMSG_INT_EXIT CMSGI(1)


/* Server contact client to notify it's ready. */
#define SMSG_INT_HANDSHAKE SMSGI(0)


/* Size of the IP protocol version identifier in bytes. */
typedef uint8_t ip_version_id_t;
#define IP_VERSION_ID_SIZE sizeof(ip_version_id_t)


typedef enum smsg_join_reply_e {
    JOIN_NO     = 0,
    JOIN_YES    = 1,
} smsg_join_reply_t;


#endif /* PACKET_DEFINES_H */
