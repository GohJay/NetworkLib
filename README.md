# Network Library

## Overview
고성능 Stateful 네트워크 라이브러리

<br>

## Class diagram

![image](https://user-images.githubusercontent.com/51254582/232109881-e2447b94-42e9-47df-a9c7-4087a13c10a3.png)

<br>

|`클래스 명`|`설명 요약`|
|:---|:---|
|NetServerEx|외부망 게임 클라이언트와 통신을 목적으로 한 IOCP 서버|
|NetContent|게임 컨텐츠 스레드의 이벤트를 핸들링 하기 위한 인터페이스 클래스|
|NetServer|외부망 클라이언트와 통신을 목적으로 한 IOCP 서버|
|LanServer|내부망 클라이언트와 통신을 목적으로 한 IOCP 서버|
|NetClient|외부망 서버와 통신을 목적으로 한 IOCP 클라이언트|
|LanClient|내부망 서버와 통신을 목적으로 한 IOCP 클라이언트|
|RingBuffer|메시지 수신을 위한 원형 버퍼|
|NetPacket|메시지 송수신을 위한 직렬화 버퍼|
|ObjectPool<T>|오브젝트의 메모리 할당과 해제 비용을 줄이기 위한 풀|
|ObjectPool_TLS<T>|오브젝트 풀 TLS(Thread Local Storage) 활용 버전|
|LockFreeStack<T>|락 프리 알고리즘으로 구현한 스택 자료구조|
|LockFreeQueue<T>|락 프리 알고리즘으로 구현한 큐 자료구조|

<br>

## Tech
* Socket Programming
* Overlapped IO
* IOCP
* LockFree Algorithm
