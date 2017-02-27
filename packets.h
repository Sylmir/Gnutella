#ifndef PACKETS_H
#define PACKETS_H

#include <stdint.h>


/* Type used to represent a packet number. */
typedef uint8_t pkt_id_t;
/* Length of a packet number (bytes) in the header. */
#define PKT_ID_LENGTH sizeof(pkt_id_t)


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
#define CMSG_NEIGHBOURS_REQUEST CMSG(0)
/* Client asks to become neighbour. */
#define CMSG_JOIN_REQUEST CMSG(1)


/* Server replies with it's direct neighbours. */
#define SMSG_NEIGHBOURS_REPLY SMSG(0)
/* Server replies with "yes" or "no". */
#define SMSG_JOIN_REPLY SMSG(1)
/* Server reply to the connect() call of a client. */
#define SMSG_CONNECT_REPLY SMSG(2)


/* Client contact server to notify it's ready. */
#define CMSG_INT_HANDSHAKE CMSGI(0)


/* Server contact client to notify it's ready. */
#define SMSG_INT_HANDSHAKE SMSGI(0)

#endif /* PACKETS_H */
