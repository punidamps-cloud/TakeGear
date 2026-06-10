// Take Control — module entry point and plugin registration.
#include "c4d_plugin.h"
#include "c4d_resource.h"

#include "tc_dialog.h"
#include "tc_thumbs.h"

using namespace cinema;

// ------------------------------------------------------------------ command

class TakeControlCommand : public CommandData
{
public:
	tc::TCDialog dlg;

	virtual Bool Execute(BaseDocument* doc, GeDialog* parentManager)
	{
		return dlg.Open(DLG_TYPE::ASYNC, ID_TC_COMMAND, -1, -1, 1100, 560);
	}

	virtual Bool RestoreLayout(void* secret)
	{
		return dlg.RestoreLayout(ID_TC_COMMAND, 0, secret);
	}
};

// ------------------------------------------------------------------ icon

// Draws a small NLE-style icon (colored clip bars + orange playhead) so the
// plugin does not need any resource files on disk.
static BaseBitmap* CreateTakeControlIcon()
{
	BaseBitmap* bmp = BaseBitmap::Alloc();
	if (!bmp)
		return nullptr;
	if (bmp->Init(32, 32, 24) != IMAGERESULT::OK)
	{
		BaseBitmap::Free(bmp);
		return nullptr;
	}

	auto fillRect = [&](Int32 x1, Int32 y1, Int32 x2, Int32 y2, Int32 r, Int32 g, Int32 b) {
		for (Int32 y = y1; y <= y2; ++y)
			for (Int32 x = x1; x <= x2; ++x)
				bmp->SetPixel(x, y, r, g, b);
	};

	fillRect(0, 0, 31, 31, 30, 30, 33);				// dark background
	fillRect(2, 5, 17, 10, 70, 160, 80);			// green clip
	fillRect(10, 13, 26, 18, 165, 165, 55);		// olive clip
	fillRect(18, 21, 30, 26, 230, 130, 50);		// orange clip
	fillRect(22, 1, 23, 30, 255, 160, 30);		// playhead
	return bmp;
}

// ------------------------------------------------------------------ registration

static Bool RegisterTakeControl()
{
	BaseBitmap* icon = CreateTakeControlIcon(); // kept alive for the app lifetime

	if (!RegisterCommandPlugin(
				ID_TC_COMMAND, "TakeGear"_s, 0, icon,
				"Timeline-driven Take switcher"_s, NewObjClear(TakeControlCommand)))
		return false;

	if (!RegisterSceneHookPlugin(
				ID_TC_SCENEHOOK, "TakeGear Data"_s, 0,
				tc::TakeControlHook::Alloc, EXECUTIONPRIORITY_GENERATOR, TC_DISKLEVEL))
		return false;

	return true;
}

// ------------------------------------------------------------------ module hooks

static Bool TGPluginStart()
{
	return RegisterTakeControl();
}

static void TGPluginEnd()
{
	tc::TCThumbShutdown();
}

static Bool TGPluginMessage(Int32 id, void* data)
{
	switch (id)
	{
		case C4DPL_INIT_SYS:
		{
			// TakeGear ships no string resources: a missing res folder must
			// not prevent the module from loading.
			g_resource.Init();
			return true;
		}
	}
	return false;
}

#if TC_HAS_CINEMA_NS // 2025+: hooks live in the cinema namespace

Bool cinema::PluginStart()
{
	return TGPluginStart();
}
void cinema::PluginEnd()
{
	TGPluginEnd();
}
Bool cinema::PluginMessage(Int32 id, void* data)
{
	return TGPluginMessage(id, data);
}

#else // 2024: classic global hooks

Bool PluginStart()
{
	return TGPluginStart();
}
void PluginEnd()
{
	TGPluginEnd();
}
Bool PluginMessage(Int32 id, void* data)
{
	return TGPluginMessage(id, data);
}

#endif
