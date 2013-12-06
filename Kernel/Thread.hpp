/**
 * @file Thread.hpp
 * @author created by: Peter Hlavaty
 */

#ifndef __THREAD_H__
#define __THREAD_H__

#include "../base/Common.h"
#include "../base/ComparableId.hpp"
#include "IRQL.hpp"
#include "../utils/VADWalker.h"
#include "../utils/Undoc.hpp"

EXTERN_C NTKERNELAPI PVOID NTAPI PsGetThreadTeb( PETHREAD Thread );
EXTERN_C NTKERNELAPI PVOID NTAPI PsGetProcessWow64Process( PEPROCESS Process );

class CEthread :
	public COMPARABLE_ID<PETHREAD>
{
public:
	explicit CEthread(
		__in HANDLE threadId
		) : COMPARABLE_ID(NULL),
			m_threadId(threadId),
			m_stackPtr(NULL),
			m_commitedStackBottom(NULL)
	{
		m_stack.Set(NULL, NULL);
	}

	~CEthread()
	{
		if (Id)
			ObDereferenceObject(Id);
	}

	__checkReturn
	bool Resolve()
	{
		if (!m_stack.Begin() && !Id)
		{
			CApcLvl irql;
			if (irql.SufficienIrql())
			{
				if (NT_SUCCESS(PsLookupThreadByThreadId(m_threadId, &Id)))
				{
					void* teb;
					if (teb = GetWow64Teb(Id))
						ResolveThreadLimits<NT_TIB32>(reinterpret_cast<NT_TIB32*>(teb));
					else if (teb = PsGetThreadTeb(Id))
						ResolveThreadLimits<NT_TIB>(reinterpret_cast<NT_TIB*>(teb));
					return true;
				}
			}
		}
		return false;
	}

	HANDLE ThreadId()
	{
		return m_threadId;
	}

	PEPROCESS GetEProcess()
	{
		return PsGetThreadProcess(Id);
	}

	CRange<ULONG_PTR>& Stack()
	{
		return m_stack;
	}

	ULONG_PTR* StackPtr()
	{
		return m_stackPtr;
	}

	ULONG_PTR* CommitedStackBottom()
	{
		return m_commitedStackBottom;
	}

	__checkReturn
	bool IsResolved()
	{
		return (NULL != m_stackPtr);
	}

private:
	template<class TYPE>
	__forceinline
	void ResolveThreadLimits(
		__in const TYPE* teb
		)
	{
		KTRAP_FRAME* trap_frame;
		if (teb && (trap_frame = GetTrapFrame(Id)))
		{
			m_stackPtr = reinterpret_cast<ULONG_PTR*>(trap_frame->Rsp);
			m_commitedStackBottom = reinterpret_cast<ULONG_PTR*>(teb->StackLimit);
			m_stack.Set(reinterpret_cast<ULONG_PTR*>(*CUndoc::DeallocationStack<TYPE>(teb)), reinterpret_cast<ULONG_PTR*>(teb->StackBase));
		}
		else
		{
			m_stackPtr = NULL;
			m_commitedStackBottom = NULL;
			m_stack.Set(NULL, NULL);
		}
	}

	__checkReturn
	NT_TIB32* GetWow64Teb( 
		__in PETHREAD thread
		)
	{
		if(PsGetProcessWow64Process(IoThreadToProcess(thread)))
		{
			NT_TIB* teb = reinterpret_cast<NT_TIB*>(PsGetThreadTeb(thread));
			if (teb)
			{
				NT_TIB32* teb32 = reinterpret_cast<NT_TIB32*>(teb->ExceptionList);
				if (teb32 && (static_cast<ULONG_PTR>(teb32->Self) == reinterpret_cast<ULONG_PTR>(teb32)))
					return teb32;
			}
		}
		return NULL;
	}
	
	__forceinline
	__checkReturn
	static KTRAP_FRAME* GetTrapFrame(
		__in const PETHREAD ethread
		)
	{
		ULONG_PTR kbegin;
		ULONG_PTR kend;
		IoGetStackLimits(&kbegin, &kend);
		CRange<void> kernel_stack(reinterpret_cast<void*>(kbegin), reinterpret_cast<void*>(kend));

		KTRAP_FRAME* trap_frame = CUndoc::EthreadTrapFrame(ethread);
		if (!kernel_stack.IsInRange(trap_frame))
			return NULL;

		while (!IsUserModeAddress(trap_frame->Rsp))
		{
			if (!kernel_stack.IsInRange(trap_frame))
				return NULL;

			trap_frame = reinterpret_cast<PKTRAP_FRAME>(trap_frame->TrapFrame);
		}

		return trap_frame;
	}

protected:
	HANDLE m_threadId;
	ULONG_PTR* m_stackPtr;
	ULONG_PTR* m_commitedStackBottom;
	CRange<ULONG_PTR> m_stack;
};

#endif //__THREAD_H__
