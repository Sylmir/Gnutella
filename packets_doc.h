#error "This file should not be included. It serves as documentation purpose."

/*
 * Store the documentation for each packet : how it is composed, what is the
 * size of each data written and so on.
 */


/*
 * In the following documentation, we will use the following terms :
 *  - "Local Client" and "Local Server" refers to the two applications running
 * on the same computer. "Local Client" is the interactive application which let
 * user formulate requests, "Local Server" is the application that will handle
 * users requests and other things such as joining the P2P network.
 *  - "Client" represents the application that is sending a request accross the
 * P2P network. By application we mean local client or local server.
 *  - "Server" represents the application that receives such a request. Upon
 * answer, we still call it "Server" and the target is still referred to as
 * "Client".
 */


/*
 * Each packet begins with an identifier therefore refered to as "opcode",
 * that is PKT_ID_LENGTH long. Usually, PKT_ID_LENGTH is sizeof(uint8_t) or
 * sizeof(uint16_t) depending on how many different packets we have. We therefore
 * require PKT_ID_LENGTH to be the size of an unsigned integral type large
 * enough to hold all the possible opcodes a user might want to have.
 *
 * Each packet opcode specifies it's own type. In bit form, the highest bit
 * indicate if the packet is internal, i.e between a local client and a local
 * server. The seconds highest bit indicate if the packet is client-to-server
 * or server-to-client.
 *
 * Each opcode is declared using one of the macros CMSG(), SMSG(), CMSGI()
 * or SMSGI() to which we pass the low-bits of the opcode, which is
 * subsequently binary-OR'ed with the appropriate mask to completely identify
 * the packet.
 *
 * This strategy results in us using PKT_ID_LENGTH * 8 - 2 bits to actually
 * identify a packet
 */


/**********************************/
/* SPECIFIC PACKETS DOCUMENTATION */
/**********************************/


/*
 * #define CMSG_NEIGHBOURS_REQUEST CMSG(0)
 *
 * Description: a client asks for the neighbours of a server.
 *
 * Content :
 *  - PKT_ID_LENGTH bytes to represent the opcode, in which we write the opcode
 * itself.
 *
 * Expected answer : SMSG_NEIGHBOURS_REPLY.
 */


/*
 * #define SMSG_NEIGHBOURS_REPLY SMSG(0)
 *
 * Description: a server answers the request of a client for it's neighbours.
 *
 * Content:
 *  - PKT_ID_LENGTH bytes to represent the opcode, in which we write the opcode
 * itself.
 *  - 1 byte to store the answer code. This answer might be one of the constants
 * defined in the smsg_neighbours_reply_t enumeration.
 *  - If the answer code is NEIGHBOURS_OK, we continue. Otherwise we stop right
 * here.
 *  - 1 byte to store the number of neighbours we are transmitting.
 *  - Each neighbours is subsequently transmitted by sending both the IP adress
 * and the port used to contact it. For each neighbour transmitted, we format
 * like that:
 *      - 1 byte to indicate which version of the IP protocol the IP refers to.
 *      - The numeric representation of the IP adress as in struct in_addr or
 * struct in_addr6. The size of this field is indicated by the version of the IP
 * protocol we are using.
 *      - 2 bytes to represent the port to contact.
 */


/*
 * #define CMSG_JOIN_REQUEST CMSG(1)
 *
 * Description: a client asks a server to become it's neighbour.
 *
 * Content:
 *  - PKT_ID_LENGTH bytes to represent the opcode, in which we write the opcode
 * itself.
 *
 * Exepcted answer : SMSG_JOIN_REPLY.
 */


/*
 * #define SMSG_JOIN_REPLY SMSG(1)
 *
 * Description: a server answers a client with "yes" or "no" when it comes to
 * becoming a neighbour.
 *
 * Content:
 *  - PKT_ID_LENGTH bytes to represent the opcode, in which we write the opcode
 * itself.
 *  - 1 byte to store the answer. The answer is expected to be one of the
 * constants defines in the
 */

/*
 * #define SMSG_CONNECT_REPLY SMSG(2)
 *
 * Description: automatically send by a server whenever a client attempts a
 * connection. A server is considered ready when it has performed it's handshake
 * with its local client. While the handshake has not been performed, the server
 * is not expected to answer to any requests.
 *
 * Content:
 *  - PKT_ID_LENGTH bytes to represent the opcode, in which we write the opcode
 * itself.
 *  - 1 byte to indicate if we are ready or not. The value is expected to be one
 * of the constants defined in
