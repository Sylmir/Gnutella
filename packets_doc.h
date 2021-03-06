#error "This file should not be included. It serves as documentation purpose."

/*
 * Store the documentation for each packet : how it is composed, what is the
 * size of each data written and so on.
 */


/*
 * In the present documentation, we will use the following terms :
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
 * that is PKT_ID_SIZE long. Usually, PKT_ID_SIZE is sizeof(uint8_t) or
 * sizeof(uint16_t) depending on how many different packets we have. We therefore
 * require PKT_ID_SIZE to be the size of an unsigned integral type large
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
 * This strategy results in us using PKT_ID_SIZE * 8 - 2 bits to actually
 * identify a packet
 */


/**********************************/
/* SPECIFIC PACKETS DOCUMENTATION */
/**********************************/


/*******************************************************************************
 * Remote C -> S
 */


/*
 * #define CMSG_NEIGHBOURS CMSG(0)
 *
 * Description: a client asks for the neighbours of a server.
 *
 * Content :
 *  - PKT_ID_SIZE bytes to represent the opcode, in which we write the opcode
 * itself.
 *
 * Expected answer : SMSG_NEIGHBOURS.
 */


/*
 * #define CMSG_JOIN CMSG(1)
 *
 * Description: a client asks a server to become it's neighbour.
 *
 * Content:
 *  - PKT_ID_SIZE bytes to represent the opcode, in which we write the opcode
 * itself.
 *  - 1 byte indicating the "rescue" flag. When this flag is set to 1, the server
 * cannot refuse the connection (unless saturated).
 *  - 1 byte indicating the length of the port (string format) used to contact
 * this servent, thereafter referred to as "port_length".
 *  - port_length bytes to store the port.
 *
 * Exepcted answer : SMSG_JOIN.
 */


/*
 * #define CMSG_NEIGHBOUR_RESCUE CMSG(2)
 *
 * Description: when a client realizes one of its neighbours has closed
 * connection, it asks for a neighbour of one of its neighbours, creating a
 * loop inside a network, preventing it from splitting in two if more server
 * close connection.
 *
 * Content:
 *  - PKT_ID_SIZE bytes to represent the opcode.
 *
 * Expected answer : SMSG_NEIGHBOUR_RESCUE.
 */


/*
 * #define CMSG_SEARCH_REQUEST CMSG(3)
 *
 * Description: client searches a file on the network and will receive a list of
 * machines that have the file.
 *
 * Content:
 *  - PKT_ID_SIZE bytes to store the opcode.
 *  - 1 byte to store the length of the IP in dotted-string representation,
 * thereafter referred to as ip_length.
 *  - ip_length bytes to store the dotted-string IP of the source machine.
 *  - 1 byte to store the length the port in string format to contact the source
 * machine, thereafter reffered to as src_port_length
 *  - src_port_length bytes to store the port in string format
 *  - 1 byte to store the length of the name of the file, thereafter referred to
 * as name_length
 *  - name_length bytes to store the name of the file.
 *  - 1 byte to store the TTL (reasearch depth)
 *  - 1 byte to indicate the number of machines that have the file
 *  For each machine we have the following informations:
 *      - 1 byte to store the length of the distant ip (in dotted-string format),
 * thereafter referred to as remote_ip_length
 *      - remote_ip_length bytes to store the IP (in dotted-string format)
 *      - 1 byte to store the length of the remote contact port (string format),
 * thereafter referred to as remote_port_length
 *      - remote_port_length bytes to store the length of the port.
 */


/*
 * #define CMSG_LEAVE CMSG(4)
 *
 * Description: a machine is leaving the network and notifies all neighbours.
 *
 * Content:
 *  - PKT_ID_SIZE bytes to store the opcode.
 */


/*******************************************************************************
 * Remote S -> C
 */


/*
 * #define SMSG_NEIGHBOURS SMSG(0)
 *
 * Description: a server answers the request of a client for it's neighbours.
 *
 * Content:
 *  - PKT_ID_SIZE bytes to represent the opcode, in which we write the opcode
 * itself.
 *  - 1 byte to store the number of neighbours we are transmitting.
 *  - Each neighbours is subsequently transmitted by sending both the IP adress
 * and the port used to contact it. For each neighbour transmitted, we format
 * like that:
 *      - 1 byte to store the length of the IP adress (doted-string format),
 * thereafter referred to as "ip_length".
 *      - ip_length bytes to store the doted-string representation of the IP.
 *      - 1 byte to store the length of the port (string format), thereafter
 * referred to as "port_length".
 *      - port_length bytes to store the port (string format).
 */


/*
 * #define SMSG_JOIN SMSG(1)
 *
 * Description: a server answers a client with "yes" or "no" when it comes to
 * becoming a neighbour.
 *
 * Content:
 *  - PKT_ID_SIZE bytes to represent the opcode, in which we write the opcode
 * itself.
 *  - 1 byte to store the answer. Note that this byte cannot be set to 0 if the
 * corresponding CMSG_JOIN has the rescue flag enabled (unless saturated).
 *
 *  From this point, we continue writing in the packet only if we answer is
 * POSITIVE.
 *      - 1 byte to store the length of the port to contact us (string), thereafter
 * referred to as "port_length".
 *      - port_length bytes to store the port.
 */


/*
 * #define SMSG_NEIGHBOUR_RESCUE SMSG(2)
 *
 * Description: a server answers the rescue request of a client with one of its
 * own neighbours.
 *
 * Content:
 *  - PKT_ID_SIZE bytes to represent the opcode.
 *  - 1 byte to indicate the version of the IP protocol.
 *  - 1 byte to indicate the length of the IP adress in string representation,
 * therefore refered to as ip_len.
 *  - ip_len bytes to store the IP adress in string representation.
 *  - 2 bytes to store the port used to contact the server.
 */


/*
 * #define SMSG_SEARCH_REQUEST SMSG(3)
 *
 * Description: server answers with a list of machines that have a given file.
 *
 * Content:
 *  - PKT_ID_SIZE bytes to store the opcode.
 *  - 1 byte to store the length of the name of the file, thereafter referred to
 * as name_length
 *  - name_length bytes to store the name of the file.
 *  - 1 byte to indicate the number of machines that have the file
 *  For each machine we have the following informations:
 *      - 1 byte to store the length of the distant ip (in dotted-string format),
 * thereafter referred to as remote_ip_length
 *      - remote_ip_length bytes to store the IP (in dotted-string format)
 *      - 1 byte to store the length of the remote contact port (string format),
 * thereafter referred to as remote_port_length
 *      - remote_port_length bytes to store the length of the port.
 */


/*******************************************************************************
 * Internal C -> S
 */


/*
 * #define CMSG_INT_HANDSHAKE CMSGI(0)
 *
 * Description: local client handshakes with local server to ensure it is up.
 *
 * Content:
 *  - PKT_ID_SIZE bytes to store the opcode.
 *
 * Expected answer: SMSG_INT_HANDSHAKE.
 */


/*
 * #define CMSG_INT_EXIT CMSGI(1)
 *
 * Description: local client notifies local server it is exiting.
 *
 * Content:
 *  - PKT_ID_SIZE bytes to store the opcode.
 */


/*
 * #define CMSG_INT_SEARCH CMSGI(2)
 *
 * Description: local client notifies local server is is searching for a file.
 *
 * Content:
 *  - PKT_ID_SIZE bytes to store the opcode.
 *  - 1 byte to indicate the length of the filename, thereafter referred to as
 * "file_name_length".
 *  - file_name_length bytes to store the name of the file.
 *
 * Once the local server is done searching, it sends SMSG_INT_SEARCH to the
 * local client.
 */


/*
 * #define CMSG_INT_DOWNLOAD CMSGI(3)
 */


/*******************************************************************************
 * Internal S -> C
 */


/*
 * #define SMSG_INT_HANDSHAKE SMSGI(0)
 *
 * Description: local server answers local client handshake to confirm it is up.
 *
 * Content:
 *  - PKT_ID_SIZE bytes to store the opcode.
 */


/*
 * #define SMSG_INT_SEARCH SMSGI(1)
 *
 * Description: local server sends a list of machines that have a desired file.
 *
 * Content:
 *  - PKT_ID_SIZE bytes to store the opcode.
 *  - 1 byte to indicate the length of the filename, thereafter reerred to as
 * "file_name_length".
 *  - file_name_length bytes to store the name of the file.
 *  - 1 byte to indicate the number of machines that have the file, thereafter
 * referred to as "nb_machines"
 *  - Now, we read the following elements for each machine :
 *      - 1 byte to indicate the length of the IP in dotted-string format (ip_length)
 *      - ip_length bytes to represent the IP in dotted-string format
 *      - 1 byte to indicate the length of the port in string format (port_length)
 *      - port_length bytes to represent the port in string format
 */
