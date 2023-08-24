#ifndef __IntrusiveList_h
#define __IntrusiveList_h

/*
#include "../shared/IntrusiveList.h"
*/

namespace se_sdk
{

// Linked list optimised for FIFO (push_back and pop_front only).
// next and prev pointers are held by contained class in 'intrusiveListNext".
// In the case of object belonging to multiple lists, use INDEX to diferentiate. 
template<class T, int INDEX = 0>
struct IntrusiveListSingleLinked
{
	T* head;
	T* tail;

	IntrusiveListSingleLinked() :
		head(nullptr),
		tail(nullptr)
	{}

	inline void push_back(T * block)
	{
		assert(INDEX < sizeof(block->intrusiveListNext) / sizeof(block->intrusiveListNext[0]));

		block->intrusiveListNext[INDEX] = nullptr;

		if (tail != nullptr)
		{
			tail->intrusiveListNext[INDEX] = block;
			tail = block;
		}
		else
		{
			tail = head = block;
		}
	}
/*
	inline T* pop_front()
	{
		assert(INDEX < sizeof(block->intrusiveListNext) / sizeof(block->intrusiveListNext[0]));

		auto block = head;
		if (block != nullptr)
		{
			head = head->intrusiveListNext[INDEX];
			if (head == nullptr)
			{
				tail = nullptr;
			}
		}
		return block;
	}
*/
	inline bool empty()
	{
		return head == nullptr;
	}
};

template<class T, int COUNT>
struct IntrusiveListClientDoubleLinked
{
	T* intrusiveListPrev[COUNT];
	T* intrusiveListNext[COUNT];

	IntrusiveListClientDoubleLinked()
	{
		memset(intrusiveListPrev, 0, sizeof(intrusiveListPrev));
		memset(intrusiveListNext, 0, sizeof(intrusiveListNext));
	}
};

// Linked list optimised for insert and removal anywhere.
template<class T, int INDEX>
struct IntrusiveListDoubleLinked
{
	struct iterator
	{
		T* current;
		iterator(T* pcurrent) :
			current(pcurrent)
		{
		}

		T* operator*() const
		{
			return current;
		}
/*
		inline T* begin()
		{
			return head;
		}
*/
		iterator& operator++() // ++x
		{
			current = current->intrusiveListNext[INDEX];
			return *this;
		}

		iterator& operator--() // --x
		{
			current = current->intrusiveListPrev[INDEX];
			return *this;
		}

		bool operator==(iterator other)
		{
			return current == other.current;
		}
		bool operator!=(iterator other)
		{
			return current != other.current;
		}
	};

	inline iterator begin()
	{
		return iterator(head);
	}

	T* head;
	T* tail;

	IntrusiveListDoubleLinked() :
		head(nullptr),
		tail(nullptr)
	{}


	inline void push_back(T* block)
	{
		assert(INDEX < sizeof(block->intrusiveListPrev) / sizeof(block->intrusiveListPrev[0]));
		assert(INDEX < sizeof(block->intrusiveListNext) / sizeof(block->intrusiveListNext[0]));
#if 0 //def _DEBUG
		if (INDEX == cacheBlock::sampleBlockList)
		{
			_RPT2(_CRT_WARN, "sampleBlockList.push_back [%x]->[%x]\n", (int)tail, (int)block);
		}
#endif

		block->intrusiveListNext[INDEX] = nullptr;
		if (tail != nullptr)
		{
			block->intrusiveListPrev[INDEX] = tail;
			tail->intrusiveListNext[INDEX] = block;
			tail = block;
		}
		else
		{
			block->intrusiveListPrev[INDEX] = nullptr;
			tail = head = block;
		}
	}

	inline T* pop_front()
	{
		assert(INDEX < sizeof(head->intrusiveListPrev) / sizeof(head->intrusiveListPrev[0]));
		assert(INDEX < sizeof(head->intrusiveListNext) / sizeof(head->intrusiveListNext[0]));
		auto block = head;
		if (block != nullptr)
		{
			head = block->intrusiveListNext[INDEX];
			if (head == nullptr)
			{
				tail = nullptr;
			}
			else
			{
				head->intrusiveListPrev[INDEX] = nullptr;
			}

			assert(block->intrusiveListPrev[INDEX] == nullptr);
			block->intrusiveListNext[INDEX] = nullptr;
		}
		return block;
	}

	inline T* pop_back()
	{
		assert(INDEX < sizeof(tail->intrusiveListPrev) / sizeof(tail->intrusiveListPrev[0]));
		assert(INDEX < sizeof(tail->intrusiveListNext) / sizeof(tail->intrusiveListNext[0]));
		auto block = tail;
		if (block != nullptr)
		{
			tail = block->intrusiveListPrev[INDEX];
			if (tail == nullptr)
			{
				head = nullptr;
			}
			else
			{
				tail->intrusiveListNext[INDEX] = nullptr;
			}
			
			block->intrusiveListPrev[INDEX] = nullptr;
			assert(block->intrusiveListNext[INDEX] == nullptr);
		}
		return block;
	}

	inline T* erase(T* block)
	{
		assert(INDEX < sizeof(block->intrusiveListPrev) / sizeof(block->intrusiveListPrev[0]));
		assert(INDEX < sizeof(block->intrusiveListNext) / sizeof(block->intrusiveListNext[0]));
		auto prev = block->intrusiveListPrev[INDEX];
		auto next = block->intrusiveListNext[INDEX];

		assert(prev || next || head == block); // item is not in list.

		if (prev)
		{
			prev->intrusiveListNext[INDEX] = next;
#if 0 //def _DEBUG
			if (INDEX == cacheBlock::sampleBlockList && next == nullptr)
			{
				_RPT1(_CRT_WARN, "sampleBlockList.erase [%x]->NULL\n", (int)prev);
			}
#endif
		}
		else
		{
			head = next;
		}

		if(next)
		{
			next->intrusiveListPrev[INDEX] = prev;
		}
		else
		{
			tail = prev;
		}

		block->intrusiveListPrev[INDEX] = block->intrusiveListNext[INDEX] = nullptr;
		
		return block;
	}

	inline iterator erase(iterator& it)
	{
		iterator r = it;
		++r;
		erase(*it);
		return r;
	}

	inline bool empty()
	{
		return head == nullptr;
	}

	iterator end()
	{
		return iterator(nullptr);
	}

	iterator find(T* block)
	{
		assert(INDEX < sizeof(block->intrusiveListPrev) / sizeof(block->intrusiveListPrev[0]));
		assert(INDEX < sizeof(block->intrusiveListNext) / sizeof(block->intrusiveListNext[0]));
		auto prev = block->intrusiveListPrev[INDEX];
		auto next = block->intrusiveListNext[INDEX];

		if (prev || next || head == block) // item is in list?
		{
			return iterator(block);
		}
		else
		{
			return iterator(nullptr);
		}
	}
};

}

#endif
