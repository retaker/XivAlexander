#include "pch.h"
#include "Utils_Win32_ThreadPool.h"

Utils::Win32::TpEnvironment::TpEnvironment(int maxCores, int threadPriority)
	: m_threadPriority(threadPriority)
	, m_maxCores(maxCores)
	, m_pool(CreateThreadpool(nullptr))
	, m_group(CreateThreadpoolCleanupGroup()) {
	if (!m_pool || !m_group) {
		if (m_pool)
			CloseThreadpool(m_pool);
		if (m_group)
			CloseThreadpoolCleanupGroup(m_group);
		throw Error("CreateThreadpool or CreateThreadpoolCleanupGroup");
	}

	InitializeThreadpoolEnvironment(&m_environ);
	SetThreadpoolCallbackPool(&m_environ, m_pool);
	SetThreadpoolCallbackCleanupGroup(&m_environ, m_group, [](void* objectCtx, void* cleanupCtx) {
		delete static_cast<std::function<void()>*>(objectCtx);
	});

	SYSTEM_INFO sysInfo;
	GetNativeSystemInfo(&sysInfo);
	if (m_maxCores <= 0)
		m_maxCores = std::max(static_cast<int>(sysInfo.dwNumberOfProcessors) + m_maxCores, 1);
	SetThreadpoolThreadMaximum(m_pool, m_maxCores);
}

Utils::Win32::TpEnvironment::~TpEnvironment() {
	Cancel();
	CloseThreadpoolCleanupGroup(m_group);
	CloseThreadpool(m_pool);
	DestroyThreadpoolEnvironment(&m_environ);
}

size_t Utils::Win32::TpEnvironment::ThreadCount() const {
	return m_maxCores;
}

void Utils::Win32::TpEnvironment::SubmitWork(std::function<void()> cb) {
	if (m_cancelling)
		return;

	const auto lock = std::lock_guard(m_workMtx);
	const auto obj = new std::function([this, cb = std::move(cb)]() {
		SetThreadPriority(GetCurrentThread(), m_threadPriority);
		cb();
	});
	const auto work = CreateThreadpoolWork([](PTP_CALLBACK_INSTANCE, void* objectCtx, PTP_WORK) {
		(*static_cast<std::function<void()>*>(objectCtx))();
	}, obj, &m_environ);
	if (!work) {
		const auto error = GetLastError();
		delete obj;
		throw Error(error, "CreateThreadpoolWork");
	}
	SubmitThreadpoolWork(work);
	m_lastWork = work;
}

void Utils::Win32::TpEnvironment::WaitOutstanding() {
	if (m_cancelling)
		return;

	const auto lastWork = m_lastWork;
	if (!lastWork)
		return;
	WaitForThreadpoolWorkCallbacks(lastWork, FALSE);

	m_lastWork = nullptr;
	const auto lock = std::lock_guard(m_workMtx);
	CloseThreadpoolCleanupGroupMembers(m_group, TRUE, this);
}

void Utils::Win32::TpEnvironment::Cancel() {
	if (m_cancelling)
		return;

	const auto lock = std::lock_guard(m_workMtx);
	m_cancelling = true;

	const auto lastWork = m_lastWork;
	if (lastWork)
		WaitForThreadpoolWorkCallbacks(lastWork, TRUE);

	m_lastWork = nullptr;
	CloseThreadpoolCleanupGroupMembers(m_group, TRUE, this);

	m_cancelling = false;
}
