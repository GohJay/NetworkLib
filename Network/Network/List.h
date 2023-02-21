#ifndef __LIST__H_
#define __LIST__H_
#include <stdlib.h>

namespace Jay
{
	/**
	* @file		List.h
	* @brief	Doubly linked List Template Class
	* @details	Iterator 패턴으로 구현한 이중연결 리스트 클래스
	* @author	고재현
	* @date		2023-01-08
	* @version	1.0.1
	**/
	template<typename T>
	class List
	{
	public:
		struct NODE
		{
			T data;
			NODE* prev;
			NODE* next;
		};
		class iterator
		{
		public:
			iterator(NODE* ptr = nullptr) : cur(ptr)
			{
			}
		public:
			iterator& operator++()
			{
				cur = cur->next;
				return *this;
			}
			iterator operator ++(int)
			{
				iterator temp(cur);
				cur = cur->next;
				return temp;
			}
			iterator& operator--()
			{
				cur = cur->prev;
				return *this;
			}
			iterator operator --(int)
			{
				iterator temp(cur);
				cur = cur->prev;
				return temp;
			}
			T& operator *()
			{
				return cur->data;
			}
			bool operator ==(const iterator& ref)
			{
				return cur == ref.cur;
			}
			bool operator !=(const iterator& ref)
			{
				return cur != ref.cur;
			}
		private:
			NODE* cur;
			friend class List;
		};
	public:
		List() : count(0)
		{
			head.prev = nullptr;
			head.next = &tail;
			tail.prev = &head;
			tail.next = nullptr;
		}
		~List()
		{
			clear();
		}
	public:
		iterator begin()
		{
			return iterator(head.next);
		}
		iterator end()
		{
			return iterator(&tail);
		}
		void push_front(T data)
		{
			NODE* pNode = (NODE*)malloc(sizeof(NODE));
			pNode->data = data;
			pNode->prev = &head;
			pNode->next = head.next;
			head.next->prev = pNode;
			head.next = pNode;
			count++;
		}
		void push_back(T data)
		{
			NODE* pNode = (NODE*)malloc(sizeof(NODE));
			pNode->data = data;
			pNode->prev = tail.prev;
			pNode->next = &tail;
			tail.prev->next = pNode;
			tail.prev = pNode;
			count++;
		}
		void pop_front()
		{
			NODE* pNode = head.next;
			head.next->next->prev = &head;
			head.next = head.next->next;
			free(pNode);
			count--;
		}
		void pop_back()
		{
			NODE* pNode = tail.prev;
			tail.prev->prev->next = &tail;
			tail.prev = tail.prev->prev;
			free(pNode);
			count--;
		}
		iterator erase(iterator iter)
		{
			NODE* pNode = iter.cur;
			iterator temp(pNode->next);
			pNode->prev->next = pNode->next;
			pNode->next->prev = pNode->prev;
			free(pNode);
			count--;
			return temp;
		}
		void remove(T data)
		{
			iterator iter;
			for (iter = begin(); iter != end();)
			{
				if (*iter == data)
					iter = erase(iter);
				else
					++iter;
			}
		}
		void clear()
		{
			iterator iter;
			for (iter = begin(); iter != end();)
			{
				iter = erase(iter);
			}
		}
		int size()
		{
			return count;
		}
		bool empty()
		{
			return count == 0;
		}
	private:
		NODE head;
		NODE tail;
		long count;
	};
}

#endif !__LIST__H_
