#ifndef __CRYPTO__H_
#define __CRYPTO__H_
#include "Base.h"

JAYNAMESPACE
#define CRYPTO_KEY 0x96
/**
* @file		Crypto.h
* @brief	Crypto Global Function
* @details	XOR ������ �̿��� ��ȣȭ ��ȣȭ �Լ�
* @author   ������
* @date		2022-05-22
* @version  1.0.0
**/
inline
	void Encrypt(const char* plaintext, char* ciphertext)
{
	char* pt_pos = (char*)plaintext;
	char* ct_pos = ciphertext;
	while (*pt_pos)
	{
		*ct_pos = *pt_pos ^ CRYPTO_KEY;
		ct_pos++;
		pt_pos++;
	}
	*ct_pos = '\0';
}
inline
	void Decrypt(const char* ciphertext, char* plaintext)
{
	Encrypt(ciphertext, plaintext);
}
JAYNAMESPACEEND

#endif !__CRYPTO__H_
