#ifndef PACKET_DEFINES_H
#define PACKET_DEFINES_H


typedef enum smsg_neighbours_reply_e {
    /* Node has too many neighbours and cannot afford to take more. */
    NEIGHBOURS_SATURATED    = 0,
    /* Node still has the possibility to accept new neighbours. */
    NEIGHBOURS_OK           = 1
} smsg_neighbours_reply_t;


typedef enum smsg_join_reply_e {
    JOIN_NO     = 0,
    JOIN_YES    = 1,
} smsg_join_reply_t;


/* Possible answers the server might send when someone connects. */
typedef enum connect_reply_e {
    /* Server has not performed handshake yet. */
    REPLY_NOT_READY = 0,
    /* Server is ready. */
    REPLY_READY     = 1,
} connect_reply_t;

#endif /* PACKET_DEFINES_H */
