// Take Control — core implementation.
#include "tc_core.h"

#include <algorithm>
#include <cstring>

#include "c4d_basedraw.h"
#include "drendersettings.h"
#include "lib_batchrender.h"

namespace tc
{

// ------------------------------------------------------------------ statuses

static const Char* g_statusNames[TC_STATUS_COUNT] = { "Todo", "WIP", "Review", "Done", "Approved" };

const Char* const* TCStatusNames()
{
	return g_statusNames;
}

Vector TCStatusColor(Int32 status)
{
	switch (status)
	{
		case 0:	 return Vector(0.65, 0.30, 0.30); // Todo
		case 1:	 return Vector(0.85, 0.65, 0.25); // WIP
		case 2:	 return Vector(0.40, 0.55, 0.85); // Review
		case 3:	 return Vector(0.35, 0.75, 0.40); // Done
		case 4:	 return Vector(0.30, 0.85, 0.75); // Approved
		default: return Vector(0.5);
	}
}

Vector TCPaletteColor(Int32 index)
{
	static const Vector pal[TC_PALETTE_COUNT] = {
		Vector(0.35, 0.55, 0.85), Vector(0.85, 0.45, 0.35), Vector(0.40, 0.75, 0.45),
		Vector(0.80, 0.70, 0.30), Vector(0.65, 0.45, 0.80), Vector(0.35, 0.75, 0.75),
		Vector(0.85, 0.55, 0.70), Vector(0.55, 0.60, 0.65), Vector(0.90, 0.60, 0.20),
		Vector(0.45, 0.50, 0.90)
	};
	if (index < 0 || index >= TC_PALETTE_COUNT)
		return Vector(0.5);
	return pal[index];
}

// ------------------------------------------------------------------ model

void TCModel::InitDefaults()
{
	tracks.clear();
	clips.clear();
	markers.clear();
	TCTrack t;
	t.name = String("T1");
	t.color = TCPaletteColor(2);
	tracks.push_back(t);
	inFrame = 0;
	outFrame = 90;
	focusClipId = NOTOK;
	nextId = 1;
	autoSwitch = true;
	fallbackMain = true;
	snap = true;
	drawHud = true;
	audioPath = String();
	audioOffset = 0;
	audioPeaks.clear();
	audioSeconds = 0.0;
}

TCClip* TCModel::FindClip(Int32 id)
{
	for (auto& c : clips)
		if (c.id == id)
			return &c;
	return nullptr;
}

const TCClip* TCModel::FindClip(Int32 id) const
{
	for (const auto& c : clips)
		if (c.id == id)
			return &c;
	return nullptr;
}

Bool TCModel::HasSolo() const
{
	for (const auto& t : tracks)
		if (t.solo)
			return true;
	return false;
}

const TCClip* TCModel::ClipAt(Int32 frame) const
{
	if (focusClipId != NOTOK)
	{
		const TCClip* c = FindClip(focusClipId);
		return (c && c->Contains(frame)) ? c : nullptr;
	}

	const Bool solo = HasSolo();
	const TCClip* best = nullptr;
	for (const auto& c : clips)
	{
		if (c.track < 0 || c.track >= (Int32)tracks.size())
			continue;
		const TCTrack& t = tracks[c.track];
		if (solo ? !t.solo : t.muted)
			continue;
		if (!c.Contains(frame))
			continue;
		// upper track wins; inside one track the later-starting clip wins
		if (!best || c.track < best->track || (c.track == best->track && c.start > best->start))
			best = &c;
	}
	return best;
}

Int32 TCModel::SequenceMin() const
{
	Int32 r = inFrame;
	for (const auto& c : clips)
		r = (c.start < r) ? c.start : r;
	return r;
}

Int32 TCModel::SequenceMax() const
{
	Int32 r = outFrame;
	for (const auto& c : clips)
		r = (c.end > r) ? c.end : r;
	return r;
}

// ------------------------------------------------------------------ scene hook

Bool TakeControlHook::Init(GeListNode* node, Bool isCloneInit)
{
	if (!isCloneInit)
		model.InitDefaults();
	return true;
}

Bool TakeControlHook::Write(const GeListNode* node, HyperFile* hf) const
{
	const TCModel& m = model;
	if (!hf->WriteInt32(3)) // internal model version
		return false;
	hf->WriteBool(m.autoSwitch);
	hf->WriteBool(m.fallbackMain);
	hf->WriteBool(m.snap);
	hf->WriteBool(m.drawHud);
	hf->WriteInt32(m.inFrame);
	hf->WriteInt32(m.outFrame);
	hf->WriteInt32(m.focusClipId);
	hf->WriteInt32(m.nextId);
	hf->WriteString(m.audioPath);
	hf->WriteInt32(m.audioOffset);

	hf->WriteInt32((Int32)m.tracks.size());
	for (const auto& t : m.tracks)
	{
		hf->WriteString(t.name);
		hf->WriteBool(t.locked);
		hf->WriteBool(t.muted);
		hf->WriteBool(t.solo);
		hf->WriteVector64(t.color); // since model version 2
	}

	hf->WriteInt32((Int32)m.clips.size());
	for (const auto& c : m.clips)
	{
		hf->WriteInt32(c.id);
		hf->WriteInt32(c.track);
		hf->WriteInt32(c.start);
		hf->WriteInt32(c.end);
		hf->WriteString(c.takeName);
		hf->WriteString(c.name);
		hf->WriteString(c.note);
		hf->WriteInt32(c.status);
		hf->WriteVector64(c.color);
		hf->WriteBool(c.camFollow);
		hf->WriteInt32(c.renderState);
		hf->WriteString(c.renderOutput); // since model version 3
	}

	hf->WriteInt32((Int32)m.markers.size());
	for (const auto& mk : m.markers)
	{
		hf->WriteInt32(mk.frame);
		hf->WriteString(mk.label);
	}
	return true;
}

Bool TakeControlHook::Read(GeListNode* node, HyperFile* hf, Int32 level)
{
	TCModel& m = model;
	m.InitDefaults();

	Int32 version = 0;
	if (!hf->ReadInt32(&version))
		return false;

	hf->ReadBool(&m.autoSwitch);
	hf->ReadBool(&m.fallbackMain);
	hf->ReadBool(&m.snap);
	hf->ReadBool(&m.drawHud);
	hf->ReadInt32(&m.inFrame);
	hf->ReadInt32(&m.outFrame);
	hf->ReadInt32(&m.focusClipId);
	hf->ReadInt32(&m.nextId);
	hf->ReadString(&m.audioPath);
	hf->ReadInt32(&m.audioOffset);

	Int32 n = 0;
	hf->ReadInt32(&n);
	m.tracks.clear();
	for (Int32 i = 0; i < n; ++i)
	{
		TCTrack t;
		hf->ReadString(&t.name);
		hf->ReadBool(&t.locked);
		hf->ReadBool(&t.muted);
		hf->ReadBool(&t.solo);
		if (version >= 2)
			hf->ReadVector64(&t.color);
		else
			t.color = TCPaletteColor(i % TC_PALETTE_COUNT);
		m.tracks.push_back(t);
	}
	if (m.tracks.empty())
	{
		TCTrack t;
		t.name = String("T1");
		t.color = TCPaletteColor(2);
		m.tracks.push_back(t);
	}

	n = 0;
	hf->ReadInt32(&n);
	m.clips.clear();
	for (Int32 i = 0; i < n; ++i)
	{
		TCClip c;
		hf->ReadInt32(&c.id);
		hf->ReadInt32(&c.track);
		hf->ReadInt32(&c.start);
		hf->ReadInt32(&c.end);
		hf->ReadString(&c.takeName);
		hf->ReadString(&c.name);
		hf->ReadString(&c.note);
		hf->ReadInt32(&c.status);
		hf->ReadVector64(&c.color);
		hf->ReadBool(&c.camFollow);
		hf->ReadInt32(&c.renderState);
		if (version >= 3)
			hf->ReadString(&c.renderOutput);
		m.clips.push_back(c);
	}

	n = 0;
	hf->ReadInt32(&n);
	m.markers.clear();
	for (Int32 i = 0; i < n; ++i)
	{
		TCMarker mk;
		hf->ReadInt32(&mk.frame);
		hf->ReadString(&mk.label);
		m.markers.push_back(mk);
	}
	return true;
}

Bool TakeControlHook::CopyTo(NodeData* dest, const GeListNode* snode, GeListNode* dnode, COPYFLAGS flags, AliasTrans* trn) const
{
	TakeControlHook* d = static_cast<TakeControlHook*>(dest);
	if (d)
		d->model = model;
	return SceneHookData::CopyTo(dest, snode, dnode, flags, trn);
}

Bool TakeControlHook::Draw(BaseSceneHook* node, BaseDocument* doc, BaseDraw* bd, BaseDrawHelp* bh, BaseThread* bt, SCENEHOOKDRAW flags)
{
	if ((flags & SCENEHOOKDRAW::DRAW_PASS) == SCENEHOOKDRAW::NONE)
		return true;
	if (!bd || !doc || !model.drawHud)
		return true;
	// only decorate the active view
	if (bd != doc->GetActiveBaseDraw())
		return true;

	const Int32 frame = TCCurrentFrame(doc);
	const TCClip* clip = model.ClipAt(frame);

	Int32 cl = 0, ct = 0, cr = 0, cb = 0;
	bd->GetSafeFrame(&cl, &ct, &cr, &cb);

	bd->SetMatrix_Screen();

	const Vector col = clip ? clip->color : Vector(0.25, 0.25, 0.25);
	bd->SetPen(col);
	for (Int32 i = 0; i < 3; ++i)
	{
		const Float l = cl + i, t = ct + i, r = cr - i, b = cb - i;
		bd->DrawLine2D(Vector(l, t, 0), Vector(r, t, 0));
		bd->DrawLine2D(Vector(r, t, 0), Vector(r, b, 0));
		bd->DrawLine2D(Vector(r, b, 0), Vector(l, b, 0));
		bd->DrawLine2D(Vector(l, b, 0), Vector(l, t, 0));
	}

	String label(String("TAKEGEAR  |  "));
	TakeData* td = doc->GetTakeData();
	if (td && td->GetCurrentTake())
		label += td->GetCurrentTake()->GetName();
	if (clip)
	{
		label += String("  |  clip: ") + clip->DisplayName();
		label += String("  [") + IStr(clip->start) + String("..") + IStr(clip->end - 1) + String("]  ");
		label += String(g_statusNames[clip->status >= 0 && clip->status < TC_STATUS_COUNT ? clip->status : 0]);
	}
	bd->DrawHUDText(cl + 10, ct + 10, label);
	return true;
}

// ------------------------------------------------------------------ model access

BaseSceneHook* TCGetHook(BaseDocument* doc)
{
	if (!doc)
		return nullptr;
	return doc->FindSceneHook(ID_TC_SCENEHOOK);
}

TCModel* TCGetModel(BaseDocument* doc)
{
	BaseSceneHook* hook = TCGetHook(doc);
	if (!hook)
		return nullptr;
	TakeControlHook* data = hook->GetNodeData<TakeControlHook>();
	return data ? &data->model : nullptr;
}

void TCAddUndo(BaseDocument* doc)
{
	BaseSceneHook* hook = TCGetHook(doc);
	if (!hook)
		return;
	doc->StartUndo();
	doc->AddUndo(UNDOTYPE::CHANGE, hook);
	doc->EndUndo();
}

// ------------------------------------------------------------------ take engine

static void CollectTakesRecursive(BaseTake* take, Int32 depth, std::vector<std::pair<BaseTake*, Int32>>& out)
{
	while (take)
	{
		out.push_back({ take, depth });
		CollectTakesRecursive(take->GetDown(), depth + 1, out);
		take = take->GetNext();
	}
}

void TCCollectTakes(BaseDocument* doc, std::vector<std::pair<BaseTake*, Int32>>& out)
{
	out.clear();
	if (!doc)
		return;
	TakeData* td = doc->GetTakeData();
	if (!td)
		return;
	CollectTakesRecursive(td->GetMainTake(), 0, out);
}

BaseTake* TCFindTakeByName(BaseDocument* doc, const String& name)
{
	if (!name.IsPopulated())
		return nullptr;
	std::vector<std::pair<BaseTake*, Int32>> takes;
	TCCollectTakes(doc, takes);
	for (auto& p : takes)
		if (p.first->GetName() == name)
			return p.first;
	return nullptr;
}

static void CollectCamerasRecursive(BaseObject* op, std::vector<BaseObject*>& out)
{
	while (op)
	{
		if (op->IsInstanceOf(Ocamera))
			out.push_back(op);
		CollectCamerasRecursive(op->GetDown(), out);
		op = op->GetNext();
	}
}

void TCCollectCameras(BaseDocument* doc, std::vector<BaseObject*>& out)
{
	out.clear();
	if (doc)
		CollectCamerasRecursive(doc->GetFirstObject(), out);
}

static void CollectRenderDataRecursive(RenderData* rd, std::vector<RenderData*>& out)
{
	while (rd)
	{
		out.push_back(rd);
		CollectRenderDataRecursive(rd->GetDown(), out);
		rd = rd->GetNext();
	}
}

void TCCollectRenderData(BaseDocument* doc, std::vector<RenderData*>& out)
{
	out.clear();
	if (doc)
		CollectRenderDataRecursive(doc->GetFirstRenderData(), out);
}

Int32 TCCurrentFrame(BaseDocument* doc)
{
	return doc->GetTime().GetFrame(doc->GetFps());
}

Bool TCSwitchTakeForTime(BaseDocument* doc)
{
	if (!doc)
		return false;
	TCModel* m = TCGetModel(doc);
	if (!m || !m->autoSwitch)
		return false;
	TakeData* td = doc->GetTakeData();
	if (!td)
		return false;

	const TCClip* clip = m->ClipAt(TCCurrentFrame(doc));
	BaseTake* cur = td->GetCurrentTake();

	// hot path during playback: if the current take already matches the clip,
	// bail out WITHOUT walking the take tree (TCFindTakeByName is O(takes))
	if (clip && cur && cur->GetName() == clip->takeName)
		return false;
	if (!clip && m->fallbackMain && cur && cur == td->GetMainTake())
		return false;

	BaseTake* target = nullptr;
	if (clip)
		target = TCFindTakeByName(doc, clip->takeName);
	if (!target && m->fallbackMain)
		target = td->GetMainTake();
	if (!target || cur == target)
		return false;

	// automatic switches must not pollute the undo stack
	const Bool undoState = td->GetUndoState();
	td->SetUndoState(false);
	td->SetCurrentTake(target);
	td->SetUndoState(undoState);
	EventAdd();
	return true;
}

void TCSetDocFrame(BaseDocument* doc, Int32 frame)
{
	if (!doc)
		return;
	doc->SetTime(BaseTime(frame, doc->GetFps()));
	TCSwitchTakeForTime(doc);
	DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::STATICBREAK);
	GeSyncMessage(EVMSG_TIMECHANGED);
	EventAdd();
}

// ------------------------------------------------------------------ camera helpers

static BaseObject* GetTakeCamera(BaseDocument* doc, const String& takeName)
{
	TakeData* td = doc->GetTakeData();
	if (!td)
		return nullptr;
	BaseTake* take = TCFindTakeByName(doc, takeName);
	if (!take)
		return nullptr;
	BaseObject* cam = take->GetCamera(td);
	if (!cam)
	{
		BaseTake* rt = nullptr;
		cam = take->GetEffectiveCamera(td, rt);
	}
	return cam;
}

String TCTakeCameraName(BaseDocument* doc, const String& takeName)
{
	BaseObject* cam = GetTakeCamera(doc, takeName);
	return cam ? String(cam->GetName()) : String();
}

void TCRenameTake(BaseDocument* doc, BaseTake* take, const String& newName)
{
	if (!doc || !take || !newName.IsPopulated())
		return;
	const String oldName = take->GetName();
	if (oldName == newName)
		return;
	TCAddUndo(doc);
	take->SetName(newName);
	TCModel* m = TCGetModel(doc);
	if (m)
	{
		for (auto& c : m->clips)
			if (c.takeName == oldName)
				c.takeName = newName;
	}
	EventAdd();
}

void TCCameraKeyFrames(BaseDocument* doc, const String& takeName, std::vector<Int32>& out)
{
	out.clear();
	BaseObject* cam = GetTakeCamera(doc, takeName);
	if (!cam)
		return;
	const Int32 fps = doc->GetFps();
	for (CTrack* tr = cam->GetFirstCTrack(); tr; tr = tr->GetNext())
	{
		CCurve* cu = tr->GetCurve();
		if (!cu)
			continue;
		const Int32 kc = cu->GetKeyCount();
		for (Int32 i = 0; i < kc; ++i)
		{
			const CKey* k = cu->GetKey(i);
			if (k)
				out.push_back(k->GetTime().GetFrame(fps));
		}
	}
	std::sort(out.begin(), out.end());
	out.erase(std::unique(out.begin(), out.end()), out.end());
}

void TCShiftCameraKeys(BaseDocument* doc, const String& takeName, Int32 rangeStart, Int32 rangeEnd, Int32 deltaFrames)
{
	if (deltaFrames == 0)
		return;
	BaseObject* cam = GetTakeCamera(doc, takeName);
	if (!cam)
		return;
	const Int32 fps = doc->GetFps();

	doc->StartUndo();
	for (CTrack* tr = cam->GetFirstCTrack(); tr; tr = tr->GetNext())
	{
		CCurve* cu = tr->GetCurve();
		if (!cu)
			continue;
		doc->AddUndo(UNDOTYPE::CHANGE, tr);

		// collect affected key indices first, then move in a safe order
		std::vector<Int32> idx;
		const Int32 kc = cu->GetKeyCount();
		for (Int32 i = 0; i < kc; ++i)
		{
			CKey* k = cu->GetKey(i);
			if (!k)
				continue;
			const Int32 f = k->GetTime().GetFrame(fps);
			if (f >= rangeStart && f < rangeEnd)
				idx.push_back(i);
		}
		if (deltaFrames > 0)
			std::reverse(idx.begin(), idx.end());
		for (Int32 i : idx)
		{
			CKey* k = cu->GetKey(i);
			if (!k)
				continue;
			const Int32 f = k->GetTime().GetFrame(fps);
			k->SetTime(cu, BaseTime(f + deltaFrames, fps));
		}
	}
	doc->EndUndo();
	cam->Message(MSG_UPDATE);
	EventAdd();
}

// ------------------------------------------------------------------ operations

void TCPreviewEdit(BaseDocument* doc)
{
	TCModel* m = TCGetModel(doc);
	if (!m)
		return;
	const Int32 fps = doc->GetFps();
	Int32 a = m->inFrame;
	Int32 b = m->outFrame;
	if (m->focusClipId != NOTOK)
	{
		const TCClip* c = m->FindClip(m->focusClipId);
		if (c)
		{
			a = c->start;
			b = c->end - 1;
		}
	}
	if (b < a)
		b = a;
	doc->SetLoopMinTime(BaseTime(a, fps));
	doc->SetLoopMaxTime(BaseTime(b, fps));
	TCSetDocFrame(doc, a);
	CallCommand(CID_PLAY_FORWARD);
}

void TCStopPlayback(BaseDocument* doc)
{
	CallCommand(CID_PLAY_STOP);
}

void TCSetRenderRange(BaseDocument* doc)
{
	TCModel* m = TCGetModel(doc);
	if (!m)
		return;
	RenderData* rd = doc->GetActiveRenderData();
	if (!rd)
		return;
	const Int32 fps = doc->GetFps();
	doc->StartUndo();
	doc->AddUndo(UNDOTYPE::CHANGE, rd);
	BaseContainer* bc = rd->GetDataInstance();
	bc->SetInt32(RDATA_FRAMESEQUENCE, RDATA_FRAMESEQUENCE_MANUAL);
	bc->SetData(RDATA_FRAMEFROM, GeData(BaseTime(m->inFrame, fps)));
	bc->SetData(RDATA_FRAMETO, GeData(BaseTime(m->outFrame, fps)));
	doc->EndUndo();
	EventAdd();
}

Int32 TCRenderEdit(BaseDocument* doc)
{
	TCModel* m = TCGetModel(doc);
	if (!m)
		return 0;
	TakeData* td = doc->GetTakeData();
	if (!td)
		return 0;
	RenderData* src = doc->GetActiveRenderData();
	if (!src)
		return 0;

	const Int32 fps = doc->GetFps();
	const Bool solo = m->HasSolo();
	Int32 count = 0;

	// order clips by start frame for a readable render data list
	std::vector<const TCClip*> ordered;
	for (const auto& c : m->clips)
	{
		if (c.track < 0 || c.track >= (Int32)m->tracks.size())
			continue;
		const TCTrack& t = m->tracks[c.track];
		if (solo ? !t.solo : t.muted)
			continue;
		ordered.push_back(&c);
	}
	std::sort(ordered.begin(), ordered.end(), [](const TCClip* a, const TCClip* b) { return a->start < b->start; });

	doc->StartUndo();
	for (const TCClip* c : ordered)
	{
		BaseTake* take = TCFindTakeByName(doc, c->takeName);
		if (!take || take->IsMain())
			continue;

		RenderData* rd = static_cast<RenderData*>(src->GetClone(COPYFLAGS::NONE, nullptr));
		if (!rd)
			continue;
		rd->SetName(String("TC ") + c->DisplayName() + String(" [") + IStr(c->start) + String("-") + IStr(c->end - 1) + String("]"));
		BaseContainer* bc = rd->GetDataInstance();
		bc->SetInt32(RDATA_FRAMESEQUENCE, RDATA_FRAMESEQUENCE_MANUAL);
		bc->SetData(RDATA_FRAMEFROM, GeData(BaseTime(c->start, fps)));
		bc->SetData(RDATA_FRAMETO, GeData(BaseTime(c->end - 1, fps)));
		doc->InsertRenderDataLast(rd);
		doc->AddUndo(UNDOTYPE::NEWOBJ, rd);

		take->SetRenderData(td, rd);
		take->SetChecked(true);

		TCClip* mc = m->FindClip(c->id);
		if (mc)
			mc->renderState = 1;
		++count;
	}
	doc->EndUndo();
	EventAdd();
	return count;
}

// ------------------------------------------------------------------ render now (batch)

static String SafeFileName(const String& s)
{
	String r;
	Char* cstr = s.GetCStringCopy(STRINGENCODING::UTF8);
	if (!cstr)
		return String("clip");
	for (Int32 i = 0; cstr[i]; ++i)
	{
		const Char ch = cstr[i];
		const Bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
		const Char buf[2] = { ok ? ch : '_', 0 };
		r += String(buf);
	}
	DeleteMem(cstr);
	if (!r.IsPopulated())
		r = String("clip");
	return r;
}

Int32 TCRenderNow(BaseDocument* doc, String* outDir)
{
	TCModel* m = TCGetModel(doc);
	if (!m)
		return 0;
	TakeData* td = doc->GetTakeData();
	if (!td)
		return 0;
	BatchRender* br = GetBatchRender();
	if (!br)
		return 0;

	const Int32 fps = doc->GetFps();
	const Bool solo = m->HasSolo();

	// output root: next to the project, or in the prefs folder for unsaved scenes
	Filename root = doc->GetDocumentPath();
	if (!root.IsPopulated())
		root = GeGetStartupWritePath();
	const Filename outRoot = root + Filename("TC_Render");
	GeFCreateDir(outRoot);

	std::vector<const TCClip*> ordered;
	for (const auto& c : m->clips)
	{
		if (c.track < 0 || c.track >= (Int32)m->tracks.size())
			continue;
		const TCTrack& t = m->tracks[c.track];
		if (solo ? !t.solo : t.muted)
			continue;
		ordered.push_back(&c);
	}
	std::sort(ordered.begin(), ordered.end(), [](const TCClip* a, const TCClip* b) { return a->start < b->start; });

	Int32 count = 0;
	for (const TCClip* c : ordered)
	{
		BaseTake* take = TCFindTakeByName(doc, c->takeName);
		if (!take)
			continue;

		// isolated document with this take fully applied (native workflow)
		BaseDocument* iso = td->TakeToDocument(take);
		if (!iso)
			continue;

		const String clipSafe = SafeFileName(c->DisplayName()) + String("_") + IStr(c->id);
		const Filename clipDir = outRoot + Filename(clipSafe);
		GeFCreateDir(clipDir);
		const Filename outBase = clipDir + Filename(clipSafe);

		RenderData* rd = iso->GetActiveRenderData();
		if (rd)
		{
			BaseContainer* bc = rd->GetDataInstance();
			bc->SetInt32(RDATA_FRAMESEQUENCE, RDATA_FRAMESEQUENCE_MANUAL);
			bc->SetData(RDATA_FRAMEFROM, GeData(BaseTime(c->start, fps)));
			bc->SetData(RDATA_FRAMETO, GeData(BaseTime(c->end - 1, fps)));
			bc->SetFilename(RDATA_PATH, outBase);
		}

		const Filename jobFile = outRoot + Filename(String("TC_") + clipSafe + String(".c4d"));
		const Bool ok = SaveDocument(iso, jobFile, SAVEDOCUMENTFLAGS::DONTADDTORECENTLIST, FORMAT_C4DEXPORT);
		BaseDocument::Free(iso);
		if (!ok)
			continue;

		br->AddFile(jobFile, br->GetElementCount());

		TCClip* mc = m->FindClip(c->id);
		if (mc)
		{
			mc->renderState = 1;
			mc->renderOutput = outBase.GetString();
		}
		++count;
	}

	if (outDir)
		*outDir = outRoot.GetString();
	EventAdd();
	return count;
}

void TCStartRenderQueue()
{
	BatchRender* br = GetBatchRender();
	if (br)
		br->SetRendering(BR_START);
}

// counts rendered image files matching the clip's output prefix
static Int32 CountRenderedFrames(const String& outputPrefix)
{
	if (!outputPrefix.IsPopulated())
		return -1;
	const Filename base(outputPrefix);
	const Filename dir = base.GetDirectory();
	const String prefix = base.GetFileString();
	const Int32 plen = (Int32)prefix.GetLength();
	if (plen <= 0)
		return -1;

	AutoAlloc<BrowseFiles> bf;
	if (!bf)
		return -1;
	bf->Init(dir, 0);
	Int32 n = 0;
	while (bf->GetNext())
	{
		if (bf->IsDir())
			continue;
		const Filename f = bf->GetFilename();
		if (f.CheckSuffix(String("c4d")))
			continue;
		const String name = f.GetFileString();
		if ((Int32)name.GetLength() >= plen && name.SubStr(0, plen) == prefix)
			++n;
	}
	return n;
}

String TCRenderStatusText(const TCClip& clip)
{
	if (clip.renderState == 2)
		return String("done");
	if (clip.renderState == 0)
		return String("—");
	const Int32 n = CountRenderedFrames(clip.renderOutput);
	if (n < 0)
		return String("prepared");
	if (n == 0)
		return String("queued");
	if (n >= clip.Length())
		return String("done");
	return String("rendering ") + IStr(n) + String("/") + IStr(clip.Length());
}

// ------------------------------------------------------------------ audio (PCM WAV)

static UInt32 ReadLE32(const UChar* p)
{
	return (UInt32)p[0] | ((UInt32)p[1] << 8) | ((UInt32)p[2] << 16) | ((UInt32)p[3] << 24);
}

static UInt16 ReadLE16(const UChar* p)
{
	return (UInt16)((UInt32)p[0] | ((UInt32)p[1] << 8));
}

Bool TCLoadAudioPeaks(TCModel& m)
{
	m.audioPeaks.clear();
	m.audioSeconds = 0.0;
	if (!m.audioPath.IsPopulated())
		return false;

	AutoAlloc<BaseFile> file;
	if (!file)
		return false;
	if (!file->Open(Filename(m.audioPath), FILEOPEN::READ, FILEDIALOG::NONE, BYTEORDER::V_INTEL))
		return false;

	UChar hdr[12];
	if (file->ReadBytes(hdr, 12) != 12)
		return false;
	if (std::memcmp(hdr, "RIFF", 4) != 0 || std::memcmp(hdr + 8, "WAVE", 4) != 0)
		return false;

	UInt16 format = 0, channels = 0, bits = 0, blockAlign = 0;
	UInt32 sampleRate = 0, dataSize = 0;
	Int64	 dataPos = -1;

	while (true)
	{
		UChar ch[8];
		if (file->ReadBytes(ch, 8) != 8)
			break;
		const UInt32 size = ReadLE32(ch + 4);
		if (std::memcmp(ch, "fmt ", 4) == 0)
		{
			UChar fmt[16];
			if (size < 16 || file->ReadBytes(fmt, 16) != 16)
				return false;
			format = ReadLE16(fmt + 0);
			channels = ReadLE16(fmt + 2);
			sampleRate = ReadLE32(fmt + 4);
			blockAlign = ReadLE16(fmt + 12);
			bits = ReadLE16(fmt + 14);
			if (size > 16)
				file->Seek(size - 16, FILESEEK::RELATIVE_);
		}
		else if (std::memcmp(ch, "data", 4) == 0)
		{
			dataPos = file->GetPosition();
			dataSize = size;
			break;
		}
		else
		{
			file->Seek(size + (size & 1), FILESEEK::RELATIVE_);
		}
	}

	if (dataPos < 0 || !blockAlign || !sampleRate || !channels)
		return false;
	if (format != 1 || (bits != 16 && bits != 8)) // PCM only
		return false;

	const Int64 samples = (Int64)dataSize / blockAlign;
	if (samples <= 0)
		return false;
	const Int32 buckets = (Int32)((samples < 4096) ? samples : 4096);
	m.audioPeaks.assign(buckets, 0.0f);
	m.audioSeconds = (Float64)samples / (Float64)sampleRate;

	const Int32 CHUNK = 65536 - (65536 % blockAlign);
	std::vector<UChar> buf(CHUNK);
	Int64 done = 0;
	Int64 left = (Int64)dataSize;
	while (left > 0)
	{
		const Int64 want = (left < CHUNK) ? left : CHUNK;
		const Int64 got = file->TryReadBytes(buf.data(), want);
		if (got <= 0)
			break;
		const Int64 n = got / blockAlign;
		for (Int64 i = 0; i < n; ++i)
		{
			Float32 v = 0.0f;
			const UChar* p = buf.data() + i * blockAlign;
			if (bits == 16)
			{
				const Int16 s = (Int16)ReadLE16(p);
				v = (Float32)s / 32768.0f;
			}
			else
			{
				v = ((Float32)p[0] - 128.0f) / 128.0f;
			}
			if (v < 0)
				v = -v;
			Int32 b = (Int32)(((done + i) * buckets) / samples);
			if (b >= buckets)
				b = buckets - 1;
			if (v > m.audioPeaks[b])
				m.audioPeaks[b] = v;
		}
		done += n;
		left -= got;
	}
	return true;
}

// ------------------------------------------------------------------ camera wizard

Int32 TCCreateTakesFromCameras(BaseDocument* doc, Int32 startFrame, Int32 track)
{
	TCModel* m = TCGetModel(doc);
	TakeData* td = doc ? doc->GetTakeData() : nullptr;
	if (!m || !td)
		return 0;
	std::vector<BaseObject*> cams;
	TCCollectCameras(doc, cams);
	if (cams.empty())
		return 0;

	TCAddUndo(doc);
	const Int32 fps = doc->GetFps();
	const Int32 len = fps * 2; // two seconds per shot by default
	if (track < 0 || track >= (Int32)m->tracks.size())
		track = 0;

	Int32 n = 0;
	Int32 cursor = startFrame;
	for (BaseObject* cam : cams)
	{
		const String name = cam->GetName();
		BaseTake* take = TCFindTakeByName(doc, name);
		if (!take)
			take = td->AddTake(name, nullptr, nullptr);
		if (!take)
			continue;
		take->SetCamera(td, cam);

		TCClip c;
		c.id = m->nextId++;
		c.track = track;
		c.start = cursor;
		c.end = cursor + len;
		c.takeName = name;
		c.name = name;
		c.color = TCPaletteColor(n % TC_PALETTE_COUNT);
		m->clips.push_back(c);
		cursor += len;
		++n;
	}
	EventAdd();
	return n;
}

// ------------------------------------------------------------------ CSV export

static String CsvQuote(const String& s)
{
	String r("\"");
	const Int32 len = (Int32)s.GetLength();
	for (Int32 i = 0; i < len; ++i)
	{
		const String ch = s.SubStr(i, 1);
		r += ch;
		if (ch == String("\""))
			r += String("\""); // CSV escaping: double the quote
	}
	r += String("\"");
	return r;
}

Bool TCExportCSV(BaseDocument* doc, const Filename& path)
{
	TCModel* m = TCGetModel(doc);
	if (!m)
		return false;
	TakeData* td = doc->GetTakeData();

	std::vector<const TCClip*> ordered;
	for (const auto& c : m->clips)
		ordered.push_back(&c);
	std::sort(ordered.begin(), ordered.end(), [](const TCClip* a, const TCClip* b) { return a->start < b->start; });

	String csv("clip,take,camera,start,end,length,status,render,note\n");
	for (const TCClip* c : ordered)
	{
		String cam;
		BaseTake* take = TCFindTakeByName(doc, c->takeName);
		if (take && td)
		{
			BaseObject* camObj = take->GetCamera(td);
			if (!camObj)
			{
				BaseTake* rt = nullptr;
				camObj = take->GetEffectiveCamera(td, rt);
			}
			if (camObj)
				cam = camObj->GetName();
		}
		csv += CsvQuote(c->DisplayName()) + String(",");
		csv += CsvQuote(c->takeName) + String(",");
		csv += CsvQuote(cam) + String(",");
		csv += IStr(c->start) + String(",");
		csv += IStr(c->end - 1) + String(",");
		csv += IStr(c->Length()) + String(",");
		csv += String(TCStatusNames()[(c->status >= 0 && c->status < TC_STATUS_COUNT) ? c->status : 0]) + String(",");
		csv += CsvQuote(TCRenderStatusText(*c)) + String(",");
		csv += CsvQuote(c->note) + String("\n");
	}

	AutoAlloc<BaseFile> file;
	if (!file || !file->Open(path, FILEOPEN::WRITE, FILEDIALOG::NONE, BYTEORDER::V_INTEL))
		return false;
	Char* cstr = csv.GetCStringCopy(STRINGENCODING::UTF8);
	if (!cstr)
		return false;
	file->WriteBytes(cstr, (Int64)std::strlen(cstr));
	DeleteMem(cstr);
	file->Close();
	return true;
}

// ------------------------------------------------------------------ sound track sync

static const Char* TC_AUDIO_NULL_NAME = "TC Audio";

static BaseObject* FindAudioNull(BaseDocument* doc)
{
	for (BaseObject* op = doc->GetFirstObject(); op; op = op->GetNext())
		if (op->GetName() == String(TC_AUDIO_NULL_NAME))
			return op;
	return nullptr;
}

void TCSyncSoundTrack(BaseDocument* doc)
{
	TCModel* m = TCGetModel(doc);
	if (!doc || !m)
		return;

	BaseObject* holder = FindAudioNull(doc);

	// no audio: remove the managed null (and its sound track) if present
	if (!m->audioPath.IsPopulated())
	{
		if (holder)
		{
			doc->StartUndo();
			doc->AddUndo(UNDOTYPE::DELETEOBJ, holder);
			holder->Remove();
			BaseObject::Free(holder);
			doc->EndUndo();
			EventAdd();
		}
		return;
	}

	doc->StartUndo();
	if (!holder)
	{
		holder = BaseObject::Alloc(Onull);
		if (!holder)
		{
			doc->EndUndo();
			return;
		}
		holder->SetName(String(TC_AUDIO_NULL_NAME));
		doc->InsertObject(holder, nullptr, nullptr);
		doc->AddUndo(UNDOTYPE::NEWOBJ, holder);
	}

	const DescID sid = DescID::Create(DescLevel(CTsound, CTsound, 0));
	CTrack* track = holder->FindCTrack(sid);
	if (!track)
	{
		doc->AddUndo(UNDOTYPE::CHANGE, holder);
		track = CTrack::Alloc(holder, sid);
		if (track)
			holder->InsertTrackSorted(track);
	}
	if (track)
	{
		doc->AddUndo(UNDOTYPE::CHANGE, track);
		// SetParameter (not the raw container) so the track actually loads the file
		track->SetParameter(ConstDescID(DescLevel(CID_SOUND_NAME)), GeData(Filename(m->audioPath)), DESCFLAGS_SET::NONE);
		track->SetParameter(ConstDescID(DescLevel(CID_SOUND_START)), GeData(BaseTime(m->audioOffset, doc->GetFps())), DESCFLAGS_SET::NONE);
		track->SetParameter(ConstDescID(DescLevel(CID_SOUND_ONOFF)), GeData((Int32)1), DESCFLAGS_SET::NONE);
		track->Message(MSG_UPDATE);
	}
	doc->EndUndo();
	EventAdd();
}

// ------------------------------------------------------------------ debug report

String TCWriteDebugReport(BaseDocument* doc)
{
	String r;
	r += String("TakeGear ") + String(TC_VERSION_STRING) + String(" — debug report\n");
	r += String("Cinema 4D version: ") + IStr(GetC4DVersion()) + String("\n");
	if (doc)
	{
		r += String("Document: ") + doc->GetDocumentName().GetString() + String("\n");
		r += String("FPS: ") + IStr(doc->GetFps()) + String("\n");

		std::vector<std::pair<BaseTake*, Int32>> takes;
		TCCollectTakes(doc, takes);
		r += String("\nTakes (") + IStr((Int32)takes.size()) + String("):\n");
		for (auto& p : takes)
		{
			for (Int32 i = 0; i < p.second; ++i)
				r += String("  ");
			r += String("- ") + p.first->GetName() + (p.first->IsChecked() ? String(" [marked]") : String("")) + String("\n");
		}

		TCModel* m = TCGetModel(doc);
		if (m)
		{
			r += String("\nIN/OUT: ") + IStr(m->inFrame) + String(" / ") + IStr(m->outFrame) + String("\n");
			r += String("Tracks (") + IStr((Int32)m->tracks.size()) + String("):\n");
			for (const auto& t : m->tracks)
				r += String("  - ") + t.name + (t.locked ? String(" [L]") : String("")) + (t.muted ? String(" [M]") : String("")) + (t.solo ? String(" [S]") : String("")) + String("\n");
			r += String("Clips (") + IStr((Int32)m->clips.size()) + String("):\n");
			for (const auto& c : m->clips)
			{
				r += String("  - #") + IStr(c.id) + String(" '") + c.DisplayName() + String("' take='") + c.takeName;
				r += String("' track=") + IStr(c.track);
				r += String(" [") + IStr(c.start) + String("..") + IStr(c.end - 1) + String("]");
				r += String(" status=") + String(g_statusNames[c.status >= 0 && c.status < TC_STATUS_COUNT ? c.status : 0]);
				if (c.camFollow)
					r += String(" camFollow");
				if (c.note.IsPopulated())
					r += String(" note='") + c.note + String("'");
				r += String("\n");
			}
			r += String("Markers (") + IStr((Int32)m->markers.size()) + String("):\n");
			for (const auto& mk : m->markers)
				r += String("  - ") + IStr(mk.frame) + String(" '") + mk.label + String("'\n");
			if (m->audioPath.IsPopulated())
				r += String("Audio: ") + m->audioPath + String(" offset=") + IStr(m->audioOffset) + String("\n");
		}
	}

	ApplicationOutput("@", r);

	const Filename path = GeGetStartupWritePath() + Filename(String("takegear_report.txt"));
	AutoAlloc<BaseFile> file;
	if (file && file->Open(path, FILEOPEN::WRITE, FILEDIALOG::NONE, BYTEORDER::V_INTEL))
	{
		Char* cstr = r.GetCStringCopy(STRINGENCODING::UTF8);
		if (cstr)
		{
			file->WriteBytes(cstr, (Int64)std::strlen(cstr));
			DeleteMem(cstr);
		}
		file->Close();
	}
	return path.GetString();
}

} // namespace tc
