// Take Control — data model, document persistence (scene hook) and take engine.
#ifndef TC_CORE_H__
#define TC_CORE_H__

#include <vector>

#include "c4d.h"
#include "c4d_scenehookdata.h"
#include "lib_takesystem.h"

#include "tc_ids.h"

namespace tc
{

using namespace cinema;

// int -> cinema::String (avoids maxon/cinema operator+ ambiguity in concatenations)
inline String IStr(Int64 v)
{
	return String(maxon::String::IntToString(v));
}

// ------------------------------------------------------------------ statuses

static const Int32 TC_STATUS_COUNT = 5;
const Char* const* TCStatusNames();						 // "Todo","WIP","Review","Done","Approved"
Vector TCStatusColor(Int32 status);

static const Int32 TC_PALETTE_COUNT = 10;
Vector TCPaletteColor(Int32 index);

// ------------------------------------------------------------------ model

struct TCClip
{
	Int32	 id = 0;
	Int32	 track = 0;
	Int32	 start = 0;					// frame, inclusive
	Int32	 end = 0;						// frame, exclusive (length = end - start)
	String takeName;
	String name;							// display name; empty -> takeName
	String note;
	Int32	 status = 0;				// index into TCStatusNames
	Vector color = Vector(0.35, 0.55, 0.85);
	Bool	 camFollow = false; // move camera keys when the clip is moved
	Int32	 renderState = 0;		// 0 = none, 1 = prepared/queued, 2 = done (manual)
	String renderOutput;			// output path prefix used by "Render Now" (for auto status)

	Int32 Length() const { return end - start; }
	Bool	Contains(Int32 f) const { return f >= start && f < end; }
	String DisplayName() const { return name.IsPopulated() ? name : takeName; }
};

struct TCTrack
{
	String name;
	Bool	 locked = false;
	Bool	 muted = false;	 // excluded from switching and dimmed
	Bool	 solo = false;
	Vector color = Vector(0.30, 0.60, 0.85); // header spine / swatch color
};

struct TCMarker
{
	Int32	 frame = 0;
	String label;
};

struct TCModel
{
	std::vector<TCTrack>	tracks;
	std::vector<TCClip>		clips;
	std::vector<TCMarker> markers;

	Int32 inFrame = 0;
	Int32 outFrame = 90;
	Int32 focusClipId = NOTOK;
	Int32 nextId = 1;

	Bool autoSwitch = true;		// switch takes on playback/scrub
	Bool fallbackMain = true; // outside any clip -> Main take
	Bool snap = true;
	Bool drawHud = true;			// viewport border + HUD

	// reference audio (path + offset serialized, peaks are a runtime cache)
	String audioPath;
	Int32	 audioOffset = 0; // frames
	std::vector<Float32> audioPeaks; // not serialized
	Float64 audioSeconds = 0.0;			 // not serialized

	void InitDefaults();
	TCClip* FindClip(Int32 id);
	const TCClip* FindClip(Int32 id) const;
	const TCClip* ClipAt(Int32 frame) const; // respects focus/solo/mute, top track wins
	Int32 SequenceMin() const;
	Int32 SequenceMax() const;
	Bool	HasSolo() const;
};

// ------------------------------------------------------------------ scene hook

class TakeControlHook : public SceneHookData
{
public:
	TCModel model;

	virtual Bool Init(GeListNode* node, Bool isCloneInit);
	virtual Bool Read(GeListNode* node, HyperFile* hf, Int32 level);
	virtual Bool Write(const GeListNode* node, HyperFile* hf) const;
	virtual Bool CopyTo(NodeData* dest, const GeListNode* snode, GeListNode* dnode, COPYFLAGS flags, AliasTrans* trn) const;
	virtual Bool Draw(BaseSceneHook* node, BaseDocument* doc, BaseDraw* bd, BaseDrawHelp* bh, BaseThread* bt, SCENEHOOKDRAW flags);

	static NodeData* Alloc() { return NewObjClear(TakeControlHook); }
};

// model of the (active) document; nullptr if the hook is missing
TCModel* TCGetModel(BaseDocument* doc);
BaseSceneHook* TCGetHook(BaseDocument* doc);

// add an undo step for the hook node (call before mutating the model)
void TCAddUndo(BaseDocument* doc);

// ------------------------------------------------------------------ take engine

// depth-first list of all takes (main take first, depth = hierarchy level)
void TCCollectTakes(BaseDocument* doc, std::vector<std::pair<BaseTake*, Int32>>& out);
BaseTake* TCFindTakeByName(BaseDocument* doc, const String& name);

// all cameras of the scene / all render settings of the document (flattened)
void TCCollectCameras(BaseDocument* doc, std::vector<BaseObject*>& out);
void TCCollectRenderData(BaseDocument* doc, std::vector<RenderData*>& out);

// returns current frame of the document
Int32 TCCurrentFrame(BaseDocument* doc);

// evaluates the model at the current document time and sets the matching take.
// returns true if the current take was changed.
Bool TCSwitchTakeForTime(BaseDocument* doc);

// set document time (frame) + viewport redraw + time sync
void TCSetDocFrame(BaseDocument* doc, Int32 frame);

// frames of all keyframes of the camera assigned to the take (for clip ticks)
void TCCameraKeyFrames(BaseDocument* doc, const String& takeName, std::vector<Int32>& out);

// name of the camera used by the take (effective, may come from a parent); empty if none
String TCTakeCameraName(BaseDocument* doc, const String& takeName);

// renames a take AND updates every clip referencing it (clips bind by name)
void TCRenameTake(BaseDocument* doc, BaseTake* take, const String& newName);

// shift all camera keys lying inside [rangeStart, rangeEnd) by deltaFrames
void TCShiftCameraKeys(BaseDocument* doc, const String& takeName, Int32 rangeStart, Int32 rangeEnd, Int32 deltaFrames);

// ------------------------------------------------------------------ operations

// loop range = IN/OUT (or focus clip), jump to start, start playback
void TCPreviewEdit(BaseDocument* doc);
void TCStopPlayback(BaseDocument* doc);

// active render settings frame range = IN/OUT
void TCSetRenderRange(BaseDocument* doc);

// per clip: clone active render data with the clip range, assign to the take,
// mark the take for "Render Marked Takes". returns number of prepared clips.
Int32 TCRenderEdit(BaseDocument* doc);

// full batch: per clip an isolated take document is saved and added to the
// native Render Queue (output goes to <project>/TC_Render/<clip>/...).
// returns the number of queued jobs; outDir receives the output root.
Int32 TCRenderNow(BaseDocument* doc, String* outDir);

// starts the native Render Queue
void TCStartRenderQueue();

// human-readable render status for the sheet ("—", "queued", "rendering 3/30", "done")
String TCRenderStatusText(const TCClip& clip);

// reference audio: parse a PCM WAV file into peaks (16/8 bit). false on failure.
Bool TCLoadAudioPeaks(TCModel& m);

// mirrors the model's audio (path + offset) into a native Sound track on a
// managed "TC Audio" null, so the C4D transport plays it during playback.
// removes the null when no audio is set.
void TCSyncSoundTrack(BaseDocument* doc);

// camera wizard: a take (named after the camera, camera assigned) + a clip per
// scene camera, laid out sequentially from startFrame on the given track.
// returns the number of created pairs.
Int32 TCCreateTakesFromCameras(BaseDocument* doc, Int32 startFrame, Int32 track);

// shot list export (clip,take,camera,start,end,length,status,render,note)
Bool TCExportCSV(BaseDocument* doc, const Filename& path);

// plain-text debug report; written to a file, path returned
String TCWriteDebugReport(BaseDocument* doc);

} // namespace tc

#endif // TC_CORE_H__
