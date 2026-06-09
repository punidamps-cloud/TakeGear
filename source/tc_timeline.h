// Take Control — multitrack timeline user area (NLE-style).
#ifndef TC_TIMELINE_H__
#define TC_TIMELINE_H__

#include "tc_core.h"

namespace tc
{

class TCTimelineArea : public GeUserArea
{
public:
	// view state
	Float viewStart = -5.0; // frame at the left edge of the clip area
	Float ppf = 6.0;				// pixels per frame
	Bool	needFit = true;

	Int32 selId = NOTOK;		// primary selected clip id (inspector row follows it)
	std::vector<Int32> selIds; // full multi-selection (always contains selId)
	Int32 curTrack = 0;			// active track (new clips land here)
	UInt32 editCounter = 0; // bumped on every model edit (dialog polls this)

	Bool IsSelected(Int32 id) const;
	void SelectOnly(Int32 id);
	void AddSelect(Int32 id); // toggles
	void ClearSelection();
	void SelectAll();

	// drag&drop (takes dropped from the source panel)
	Bool	 dragHover = false;
	Int32	 dragTrack = 0;
	Int32	 dragFrame = 0;
	String dragName;

	// live preview while dragging a clip's camera lane (key offset in frames)
	Int32 dragCamClipId = NOTOK;
	Int32 dragCamOffset = 0;

	// settings chips in the title bar (AUTO/MAIN/SNAP/HUD); rects cached at draw time
	Int32 chipL[4] = { 0, 0, 0, 0 };
	Int32 chipR[4] = { 0, 0, 0, 0 };

	// transient frame readout while an IN/OUT handle is being dragged
	Int32 dragRange = 0; // 0 = none, 1 = IN, 2 = OUT

	// marquee (rubber-band) selection on empty timeline space
	Bool	marqueeOn = false;
	Int32 mqX1 = 0, mqY1 = 0, mqX2 = 0, mqY2 = 0;
	void DragMarquee(BaseDocument* doc, TCModel& m, Int32 mx, Int32 my);

	// hover state (tracked via BFM_GETCURSORINFO, cleared on BFM_CURSORINFO_REMOVE)
	Int32 hoverClipId = NOTOK;
	Int32 hoverBtn = NOTOK;	 // transport button 0..2
	Int32 hoverChip = NOTOK; // settings chip 0..3
	Int32 hoverTrack = NOTOK; // header row

	// per-take camera info, cached between redraws (rebuilt at most every 400ms;
	// collecting CTrack keys per redraw was a major playback hotspot)
	struct CamInfo
	{
		String take;
		String camName;
		std::vector<Int32> keys;
	};
	std::vector<CamInfo> camCache;
	Float64 camCacheStamp = 0.0;
	const CamInfo& CamInfoFor(BaseDocument* doc, const String& takeName);

	virtual Bool GetMinSize(Int32& w, Int32& h);
	virtual void DrawMsg(Int32 x1, Int32 y1, Int32 x2, Int32 y2, const BaseContainer& msg);
	virtual Bool InputEvent(const BaseContainer& msg);
	virtual Int32 Message(const BaseContainer& msg, BaseContainer& result);

	// operations (shared by toolbar, hotkeys and context menu)
	void FitView();
	void AddClipForTake(const String& takeName);
	void AddClipFromCurrentTake();
	void SplitAtPlayhead();
	void DuplicateSelected();
	void DeleteSelected();
	void ToggleFocusSelected();
	void AddMarkerAtPlayhead();
	void SetInAtPlayhead();
	void SetOutAtPlayhead();
	void NudgeSelected(Int32 delta);
	void AddTrack();
	void DeleteCurrentTrack();
	void RippleDeleteSelected(); // delete + close the gap (all tracks, markers)
	void CopySelected();
	void PasteAtPlayhead();

private:
	// layout constants
	static const Int32 HEADER_W = 170;
	static const Int32 TITLE_H = 24;
	static const Int32 SCROLL_H = 12;
	static const Int32 RULER_H = 30;
	static const Int32 AUDIO_H = 56;
	static const Int32 CAMLANE_H = 14; // camera key-offset handle at the clip bottom

	Int32 trackH = 52; // user-adjustable track height (Ctrl+wheel)

	static Int32 TopOffset() { return TITLE_H + SCROLL_H + RULER_H; }

	Int32 FrameToX(Float f) const { return HEADER_W + (Int32)((f - viewStart) * ppf); }
	Float XToFrame(Int32 x) const { return viewStart + (Float)(x - HEADER_W) / ppf; }
	Int32 TrackTop(Int32 track) const { return TopOffset() + track * trackH; }
	Int32 TrackAtY(Int32 y, Int32 trackCount) const;

	struct ClipHit
	{
		Int32 clipId = NOTOK;
		Int32 zone = 0; // 0 = body, 1 = left edge, 2 = right edge, 3 = camera lane
	};
	ClipHit HitClip(const TCModel& m, Int32 mx, Int32 my) const;
	Int32 HitMarker(const TCModel& m, Int32 mx, Int32 my) const; // marker index or NOTOK

	Int32 SnapFrame(const TCModel& m, BaseDocument* doc, Float frame, Int32 ignoreClipId) const;

	void Edited(BaseDocument* doc, Bool switchTake = true);

	// content extents for the scrollbar
	void ContentExtents(BaseDocument* doc, const TCModel& m, Float& a, Float& b) const;

	// rounded primitives + small vector glyphs
	void FillRoundRect(Int32 x1, Int32 y1, Int32 x2, Int32 y2, Int32 r, const Vector& col);
	// anti-aliased variant: corner edge pixels are blended towards `under`
	void FillRoundRect(Int32 x1, Int32 y1, Int32 x2, Int32 y2, Int32 r, const Vector& col, const Vector& under);
	void DrawCircleOutline(Int32 cx, Int32 cy, Int32 r);
	void GlyphLock(Int32 cx, Int32 cy, Bool on);
	void GlyphEye(Int32 cx, Int32 cy, Bool on);
	void GlyphSolo(Int32 cx, Int32 cy, Bool on);
	void GlyphSpeaker(Int32 cx, Int32 cy);
	void GlyphCam(Int32 cx, Int32 cy, const Vector& col);
	void ToggleChip(Int32 cx, Int32 cy, Bool on, const Vector& onColor, Int32 which); // which: 0=lock 1=eye 2=solo
	Vector pillUnder = Vector(0.125, 0.125, 0.129); // backdrop color used for pill AA
	void DrawPill(Int32 x, Int32 y, Int32 h, const String& text, const Vector& bg, const Vector& fg, Int32 maxRight);

	// polls absolute local mouse coordinates while the button is held;
	// returns false once the button is released (no delta sign ambiguity)
	Bool PollDrag(Int32 channel, Int32& x, Int32& y);

	void DragScrub(BaseDocument* doc, Int32 mxStart, Int32 myStart);
	void DragClip(BaseDocument* doc, TCModel& m, const ClipHit& hit, Int32 mx, Int32 my, Int32 qualifier);
	void DragMarker(BaseDocument* doc, TCModel& m, Int32 markerIndex, Int32 mx, Int32 my);
	void DragInOut(BaseDocument* doc, TCModel& m, Bool isIn, Int32 mx, Int32 my);
	void DragAudio(BaseDocument* doc, TCModel& m, Int32 mx, Int32 my);
	void DragCamLane(BaseDocument* doc, TCModel& m, Int32 clipId, Int32 mx, Int32 my);
	void DragPan(Int32 mx, Int32 my, Int32 button);
	Bool DragZoom(Int32 mx, Int32 my); // returns true if zoomed (false = it was a click)
	void DragScrollbar(BaseDocument* doc, TCModel& m, Int32 mx, Int32 my);

	void ContextMenu(BaseDocument* doc, TCModel& m, Int32 mx, Int32 my, Int32 screenX, Int32 screenY);
	Bool HandleKey(BaseDocument* doc, TCModel& m, const BaseContainer& msg);
};

} // namespace tc

#endif // TC_TIMELINE_H__
