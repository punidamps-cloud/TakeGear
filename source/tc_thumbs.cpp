// Take Control — background frame thumbnails implementation.
#include "tc_thumbs.h"

#include <atomic>
#include <deque>
#include <mutex>
#include <vector>

#include "c4d_thread.h"
#include "drendersettings.h"

namespace tc
{

namespace
{

struct Job
{
	BaseDocument* iso = nullptr; // isolated take document, owned by the job
	String				key;
	Int32					frame = 0;
};

struct Entry
{
	String key;
	BaseBitmap* bmp = nullptr; // nullptr = still rendering
};

std::mutex g_lock;
std::deque<Job> g_jobs;					 // guarded by g_lock
std::vector<Entry> g_cache;			 // guarded by g_lock
std::atomic<UInt32> g_version{ 0 };

Entry* FindEntry(const String& key)
{
	for (auto& e : g_cache)
		if (e.key == key)
			return &e;
	return nullptr;
}

// ------------------------------------------------------------------ worker

class ThumbThread : public C4DThread
{
public:
	virtual const Char* GetThreadName() { return "TC Thumbnails"; }

	virtual void Main()
	{
		while (!TestBreak())
		{
			Job job;
			{
				std::lock_guard<std::mutex> guard(g_lock);
				if (g_jobs.empty())
					return; // queue drained; restarted on next request
				job = g_jobs.front();
				g_jobs.pop_front();
			}

			BaseBitmap* bmp = Render(job);

			{
				std::lock_guard<std::mutex> guard(g_lock);
				Entry* e = FindEntry(job.key);
				if (e)
					e->bmp = bmp; // may stay nullptr on failure -> "no thumb"
				else if (bmp)
					BaseBitmap::Free(bmp);
			}
			BaseDocument::Free(job.iso);
			g_version.fetch_add(1);
			SpecialEventAdd(ID_TC_COMMAND); // nudge the dialog to redraw
		}
	}

private:
	BaseBitmap* Render(Job& job)
	{
		if (!job.iso)
			return nullptr;

		job.iso->SetTime(BaseTime(job.frame, job.iso->GetFps()));
		job.iso->ExecutePasses(this->Get(), true, true, true, BUILDFLAGS::NONE);

		RenderData* rd = job.iso->GetActiveRenderData();
		BaseContainer rdata = rd ? rd->GetDataInstanceRef() : BaseContainer();
		// cheap, thread-safe thumbnail settings: Standard renderer, tiny res
		rdata.SetInt32(RDATA_RENDERENGINE, RDATA_RENDERENGINE_STANDARD);
		rdata.SetInt32(RDATA_XRES, TC_THUMB_W);
		rdata.SetInt32(RDATA_YRES, TC_THUMB_H);
		rdata.SetFloat(RDATA_FILMASPECT, (Float)TC_THUMB_W / (Float)TC_THUMB_H);
		rdata.SetBool(RDATA_GLOBALSAVE, false);
		rdata.SetBool(RDATA_SAVEIMAGE, false);
		rdata.SetBool(RDATA_MULTIPASS_ENABLE, false);
		rdata.SetBool(RDATA_POSTEFFECTS_ENABLE, false);

		BaseBitmap* bmp = BaseBitmap::Alloc();
		if (!bmp)
			return nullptr;
		if (bmp->Init(TC_THUMB_W, TC_THUMB_H, 24) != IMAGERESULT::OK)
		{
			BaseBitmap::Free(bmp);
			return nullptr;
		}

		const RENDERRESULT res = RenderDocument(job.iso, rdata, nullptr, nullptr, bmp,
			RENDERFLAGS::EXTERNAL | RENDERFLAGS::NODOCUMENTCLONE | RENDERFLAGS::PREVIEWRENDER, this->Get());
		if (res != RENDERRESULT::OK)
		{
			BaseBitmap::Free(bmp);
			return nullptr;
		}
		return bmp;
	}
};

// IMPORTANT: must NOT be a static instance — the C4DThread constructor calls
// into the C4D core (C4DOS_Bt->Alloc), which is not initialized yet while the
// DLL is being loaded. A static instance kills the whole module with Windows
// error 1114. Created lazily on first use instead.
ThumbThread* g_thread = nullptr;

} // anonymous namespace

// ------------------------------------------------------------------ api

BaseBitmap* TCThumbGet(BaseDocument* doc, const String& takeName, Int32 frame)
{
	if (!doc || !takeName.IsPopulated())
		return nullptr;

	const String key = takeName + String("@") + IStr(frame);
	{
		std::lock_guard<std::mutex> guard(g_lock);
		Entry* e = FindEntry(key);
		if (e)
			return e->bmp; // done or still pending (nullptr)
	}

	// main thread: isolate the take into its own document and enqueue
	TakeData* td = doc->GetTakeData();
	BaseTake* take = TCFindTakeByName(doc, takeName);
	if (!td || !take)
		return nullptr;
	BaseDocument* iso = td->TakeToDocument(take);
	if (!iso)
		return nullptr;

	{
		std::lock_guard<std::mutex> guard(g_lock);
		Entry e;
		e.key = key;
		g_cache.push_back(e);
		Job job;
		job.iso = iso;
		job.key = key;
		job.frame = frame;
		g_jobs.push_back(job);
	}

	if (!g_thread)
		g_thread = NewObjClear(ThumbThread);
	if (g_thread && !g_thread->IsRunning())
		g_thread->Start();
	return nullptr;
}

UInt32 TCThumbVersion()
{
	return g_version.load();
}

void TCThumbClear()
{
	if (g_thread)
		g_thread->End(true);
	std::lock_guard<std::mutex> guard(g_lock);
	for (auto& j : g_jobs)
		BaseDocument::Free(j.iso);
	g_jobs.clear();
	for (auto& e : g_cache)
		if (e.bmp)
			BaseBitmap::Free(e.bmp);
	g_cache.clear();
	g_version.fetch_add(1);
}

void TCThumbShutdown()
{
	TCThumbClear();
	if (g_thread)
	{
		DeleteObj(g_thread);
		g_thread = nullptr;
	}
}

} // namespace tc
