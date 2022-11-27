#ifndef __BASE_HH__H_
#define __BASE_HH__H_

#ifdef __cplusplus
#define BASE_EXTERN extern "C" {
#define BASE_EXTERNEND }
#else
#define BASE_EXTERN
#define BASE_EXTERNEND
#endif
#ifdef API_EXPORTS
#define BASE_API __declspec(dllexport)
#else
#define BASE_API __declspec(dllimport)
#endif //!API_EXPORTS
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif //!WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <Windows.h>
#include <stdlib.h>
#include <stdio.h>
#else
#ifndef closesocket
#define closesocket(sock) close(sock)
#endif //!closesocket
#ifndef SOCKET
#define SOCKET size_t
#endif //!SOCKET
#ifndef SOCKADDR
#define SOCKADDR struct sockaddr
#endif //!SOCKADDR7
#endif //!_WIN32
#ifndef IInterface
#define IInterface struct
#endif
#ifndef CRASH
#define CRASH \
do\
{\
	int *ptr = nullptr;\
	*ptr = 0;\
} while(0)
#endif //!CRASH
#ifndef JAYNAMESPACE
#define JAYNAMESPACE namespace Jay {
#define JAYNAMESPACEEND }
#define USEJAYNAMESPACE using namespace Jay;
#endif //!JAYNAMESPACE

#endif //!__BASE_HH__H_
