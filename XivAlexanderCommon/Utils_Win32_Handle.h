#pragma once

#include <functional>

#include "Utils_CallOnDestruction.h"
#include "Utils_Win32_Closeable.h"
#include "Utils_Win32_LoadedModule.h"

namespace Utils::Win32 {
	
	class Handle : public Closeable<HANDLE, CloseHandle> {
		static HANDLE DuplicateHandleNullable(HANDLE src);

	public:
		using Closeable<HANDLE, CloseHandle>::Closeable;
		Handle(Handle&& r) noexcept;
		Handle(const Handle& r);
		Handle& operator=(Handle&& r) noexcept;
		Handle& operator=(const Handle& r);
		~Handle() override;
		
		static Handle DuplicateFrom(HANDLE hProcess, HANDLE hSourceHandle, bool bInheritable = false);
		static Handle DuplicateFrom(HANDLE hSourceHandle, bool bInheritable = false);
		
		void Wait() const;
		[[nodiscard]] DWORD Wait(DWORD duration) const;
	};

	class ActivationContext : public Closeable<HANDLE, ReleaseActCtx> {
	public:
		using Closeable<HANDLE, ReleaseActCtx>::Closeable;
		ActivationContext(ActivationContext&& r) noexcept;
		ActivationContext& operator=(ActivationContext&& r) noexcept;
		explicit ActivationContext(const ACTCTXW& actctx);
		~ActivationContext() override;

		[[nodiscard]] CallOnDestruction With() const;
	};

	class Thread : public Handle {
	public:
		using Handle::Handle;
		Thread(std::wstring name, std::function<DWORD()> body, LoadedModule hLibraryToFreeAfterExecution = nullptr);
		Thread(std::wstring name, std::function<void()> body, LoadedModule hLibraryToFreeAfterExecution = nullptr);
		~Thread() override;
		
		[[nodiscard]] DWORD GetId() const;
	};

	class Event : public Handle {
	public:
		using Handle::Handle;
		~Event () override;

		static Event Create(
			_In_opt_ LPSECURITY_ATTRIBUTES lpEventAttributes = nullptr,
			_In_ BOOL bManualReset = TRUE,
			_In_ BOOL bInitialState = FALSE,
			_In_opt_ LPCWSTR lpName = nullptr
		);

		void Set() const;
		void Reset() const;
	};
}