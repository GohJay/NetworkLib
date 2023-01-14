#ifndef __ERROR__H
#define __ERROR__H

#define NET_ERROR_LISTEN_FAILED			10000
#define NET_ERROR_INITIAL_FAILED		NET_ERROR_LISTEN_FAILED + 1
#define NET_ERROR_RELEASE_FAILED		NET_ERROR_LISTEN_FAILED + 2
#define NET_ERROR_SOCKET_FAILED			NET_ERROR_LISTEN_FAILED + 3
#define NET_ERROR_NETBUFFER_OVER		NET_ERROR_LISTEN_FAILED + 4
#define NET_ERROR_SESSION_TIMEOUT		NET_ERROR_LISTEN_FAILED + 5
#define NET_ERROR_CONNECT_ERROR			NET_ERROR_LISTEN_FAILED + 6
#define NET_ERROR_ALREADY_CONNECTED		NET_ERROR_LISTEN_FAILED + 7
#define NET_ERROR_NOT_CONNECTED			NET_ERROR_LISTEN_FAILED + 8
#define NET_ERROR_NOT_ENOUGH_SPACE		NET_ERROR_LISTEN_FAILED + 9
#define NET_ERROR_DECRYPTION_FAILED		NET_ERROR_LISTEN_FAILED + 10

#define NET_FATAL_INVALID_SIZE			20000

#endif
