// Take Control — timeline user area implementation (NLE-style).
#include "tc_timeline.h"
#include "tc_settings.h"
#include "tc_thumbs.h"

#include <algorithm>

namespace tc
{

// ------------------------------------------------------------------ helpers

static inline Int32 RoundToInt(Float f)
{
	return (Int32)((f >= 0.0) ? (f + 0.5) : (f - 0.5));
}

static inline Int32 ClampInt(Int32 v, Int32 a, Int32 b)
{
	return (v < a) ? a : ((v > b) ? b : v);
}

static inline Float AbsF(Float v)
{
	return v < 0 ? -v : v;
}

static inline Vector Lighten(const Vector& c, Float f)
{
	Vector r = c * f;
	if (r.x > 1.0)
		r.x = 1.0;
	if (r.y > 1.0)
		r.y = 1.0;
	if (r.z > 1.0)
		r.z = 1.0;
	return r;
}

// palette of the user area (dark NLE theme)
static const Vector COL_BG(0.094, 0.094, 0.098);
static const Vector COL_TITLE(0.137, 0.137, 0.145);
static const Vector COL_RULER(0.110, 0.110, 0.114);
static const Vector COL_HEADER(0.153, 0.153, 0.161);
static const Vector COL_HEADER_SEL(0.196, 0.196, 0.208);
static const Vector COL_ROW(0.125, 0.125, 0.129);
static const Vector COL_ROW_SEP(0.075, 0.075, 0.078);
static const Vector COL_GRID(0.165, 0.165, 0.172);
static const Vector COL_TICK(0.42, 0.42, 0.45);
static const Vector COL_TEXT(0.82, 0.82, 0.84);
static const Vector COL_TEXT_DIM(0.46, 0.46, 0.50);
static const Vector COL_ACCENT(1.00, 0.62, 0.12); // playhead / IN / current
static const Vector COL_MARKER(0.95, 0.75, 0.25);
static const Vector COL_AUDIO_BG(0.063, 0.082, 0.118);
static const Vector COL_AUDIO_BORDER(0.25, 0.48, 0.85);
static const Vector COL_AUDIO_WAVE(0.82, 0.86, 0.92);
static const Vector COL_SELECT(0.96, 0.96, 0.96);
static const Vector COL_SCROLL_BG(0.075, 0.075, 0.078);
static const Vector COL_SCROLL_THUMB(0.27, 0.27, 0.30);
static const Vector COL_PILL_BG(0.93, 0.93, 0.93);
static const Vector COL_PILL_FG(0.10, 0.10, 0.10);
static const Vector COL_WIP_BG(0.18, 0.42, 0.85);

// clip clipboard, shared between documents within the session
static std::vector<TCClip> g_clipboard;

// ------------------------------------------------------------------ selection

Bool TCTimelineArea::IsSelected(Int32 id) const
{
	for (Int32 s : selIds)
		if (s == id)
			return true;
	return false;
}

void TCTimelineArea::SelectOnly(Int32 id)
{
	selIds.clear();
	selId = id;
	if (id != NOTOK)
		selIds.push_back(id);
}

void TCTimelineArea::AddSelect(Int32 id)
{
	if (id == NOTOK)
		return;
	for (size_t i = 0; i < selIds.size(); ++i)
	{
		if (selIds[i] == id)
		{
			selIds.erase(selIds.begin() + i); // toggle off
			selId = selIds.empty() ? NOTOK : selIds.back();
			return;
		}
	}
	selIds.push_back(id);
	selId = id;
}

void TCTimelineArea::ClearSelection()
{
	selIds.clear();
	selId = NOTOK;
}

void TCTimelineArea::SelectAll()
{
	BaseDocument* doc = GetActiveDocument();
	TCModel* m = doc ? TCGetModel(doc) : nullptr;
	if (!m)
		return;
	selIds.clear();
	for (const auto& c : m->clips)
		selIds.push_back(c.id);
	selId = selIds.empty() ? NOTOK : selIds.back();
	Redraw();
}

// ------------------------------------------------------------------ basics

Bool TCTimelineArea::GetMinSize(Int32& w, Int32& h)
{
	w = 600;
	h = 240;
	return true;
}

Int32 TCTimelineArea::TrackAtY(Int32 y, Int32 trackCount) const
{
	if (y < TopOffset())
		return NOTOK;
	const Int32 t = (y - TopOffset()) / trackH;
	if (t < 0 || t >= trackCount)
		return NOTOK;
	return t;
}

void TCTimelineArea::ContentExtents(BaseDocument* doc, const TCModel& m, Float& a, Float& b) const
{
	const Int32 fps = doc->GetFps();
	Int32 ia = doc->GetMinTime().GetFrame(fps);
	Int32 ib = doc->GetMaxTime().GetFrame(fps);
	ia = (m.SequenceMin() < ia) ? m.SequenceMin() : ia;
	ib = (m.SequenceMax() > ib) ? m.SequenceMax() : ib;
	a = (Float)ia - 2.0;
	b = (Float)ib + 10.0;
	if (b - a < 10.0)
		b = a + 10.0;
}

void TCTimelineArea::FitView()
{
	BaseDocument* doc = GetActiveDocument();
	TCModel* m = TCGetModel(doc);
	if (!doc || !m)
		return;
	Float a = 0, b = 100;
	ContentExtents(doc, *m, a, b);
	const Int32 w = GetWidth() - HEADER_W;
	if (w < 200)
	{
		// layout is not final yet — keep needFit set and try again next draw
		viewStart = a;
		return;
	}
	ppf = (Float)w / (b - a);
	if (ppf < 0.05)
		ppf = 0.05;
	if (ppf > 200.0)
		ppf = 200.0;
	viewStart = a;
	needFit = false;
}

// ------------------------------------------------------------------ rounded primitives

void TCTimelineArea::FillRoundRect(Int32 x1, Int32 y1, Int32 x2, Int32 y2, Int32 r, const Vector& col)
{
	if (x2 < x1 || y2 < y1)
		return;
	const Int32 h = y2 - y1 + 1;
	const Int32 w = x2 - x1 + 1;
	Int32 rr = r;
	if (rr * 2 > h)
		rr = h / 2;
	if (rr * 2 > w)
		rr = w / 2;
	DrawSetPen(col);
	if (rr <= 0)
	{
		DrawRectangle(x1, y1, x2, y2);
		return;
	}
	DrawRectangle(x1, y1 + rr, x2, y2 - rr);
	for (Int32 i = 0; i < rr; ++i)
	{
		const Float dy = (Float)(rr - i) - 0.5;
		const Float dx = maxon::Sqrt((Float)(rr * rr) - dy * dy);
		const Int32 inset = rr - (Int32)(dx + 0.5);
		DrawRectangle(x1 + inset, y1 + i, x2 - inset, y1 + i);
		DrawRectangle(x1 + inset, y2 - i, x2 - inset, y2 - i);
	}
}

void TCTimelineArea::FillRoundRect(Int32 x1, Int32 y1, Int32 x2, Int32 y2, Int32 r, const Vector& col, const Vector& under)
{
	if (x2 < x1 || y2 < y1)
		return;
	const Int32 h = y2 - y1 + 1;
	const Int32 w = x2 - x1 + 1;
	Int32 rr = r;
	if (rr * 2 > h)
		rr = h / 2;
	if (rr * 2 > w)
		rr = w / 2;
	DrawSetPen(col);
	if (rr <= 0)
	{
		DrawRectangle(x1, y1, x2, y2);
		return;
	}
	DrawRectangle(x1, y1 + rr, x2, y2 - rr);
	for (Int32 i = 0; i < rr; ++i)
	{
		const Float dy = (Float)(rr - i) - 0.5;
		const Float dx = maxon::Sqrt((Float)(rr * rr) - dy * dy);
		const Float insetF = (Float)rr - dx;
		Int32 inset = (Int32)insetF;
		Float frac = 1.0 - (insetF - (Float)inset); // coverage of the edge pixel
		if (frac > 1.0)
			frac = 1.0;
		const Vector edge = col * frac + under * (1.0 - frac);

		// solid middle of the row
		DrawSetPen(col);
		DrawRectangle(x1 + inset + 1, y1 + i, x2 - inset - 1, y1 + i);
		DrawRectangle(x1 + inset + 1, y2 - i, x2 - inset - 1, y2 - i);
		// blended edge pixels (cheap anti-aliasing)
		DrawSetPen(edge);
		DrawRectangle(x1 + inset, y1 + i, x1 + inset, y1 + i);
		DrawRectangle(x2 - inset, y1 + i, x2 - inset, y1 + i);
		DrawRectangle(x1 + inset, y2 - i, x1 + inset, y2 - i);
		DrawRectangle(x2 - inset, y2 - i, x2 - inset, y2 - i);
	}
	DrawSetPen(col);
}

// ------------------------------------------------------------------ glyphs

void TCTimelineArea::DrawCircleOutline(Int32 cx, Int32 cy, Int32 r)
{
	Int32 px = cx + r, py = cy;
	for (Int32 i = 1; i <= 16; ++i)
	{
		const Float a = (Float)i / 16.0 * 6.2831853;
		const Int32 x = cx + (Int32)(r * maxon::Cos(a));
		const Int32 y = cy + (Int32)(r * maxon::Sin(a));
		DrawLine(px, py, x, y);
		px = x;
		py = y;
	}
}

void TCTimelineArea::GlyphLock(Int32 cx, Int32 cy, Bool on)
{
	// color is set by the caller (chip decides contrast)
	FillRoundRect(cx - 4, cy, cx + 4, cy + 5, 1, on ? Vector(0.10) : Vector(0.55, 0.55, 0.58));
	DrawLine(cx - 2, cy - 1, cx - 2, cy - 3);
	DrawLine(cx + 2, cy - 1, cx + 2, cy - 3);
	DrawLine(cx - 2, cy - 4, cx + 2, cy - 4);
	if (!on)
	{
		DrawSetPen(Vector(0.185, 0.185, 0.195));
		DrawLine(cx + 2, cy - 1, cx + 2, cy - 3); // open shackle
	}
}

void TCTimelineArea::GlyphEye(Int32 cx, Int32 cy, Bool on)
{
	const Vector col = on ? Vector(0.10) : Vector(0.55, 0.55, 0.58);
	DrawSetPen(col);
	// almond shape
	DrawLine(cx - 6, cy, cx - 3, cy - 3);
	DrawLine(cx - 3, cy - 3, cx + 3, cy - 3);
	DrawLine(cx + 3, cy - 3, cx + 6, cy);
	DrawLine(cx - 6, cy, cx - 3, cy + 3);
	DrawLine(cx - 3, cy + 3, cx + 3, cy + 3);
	DrawLine(cx + 3, cy + 3, cx + 6, cy);
	DrawRectangle(cx - 1, cy - 1, cx + 1, cy + 1); // pupil
	if (!on)
		DrawLine(cx - 6, cy + 5, cx + 6, cy - 5);
}

void TCTimelineArea::GlyphSolo(Int32 cx, Int32 cy, Bool on)
{
	DrawSetPen(on ? Vector(0.10) : Vector(0.55, 0.55, 0.58));
	DrawCircleOutline(cx, cy, 4);
	DrawRectangle(cx - 1, cy - 1, cx + 1, cy + 1);
}

void TCTimelineArea::ToggleChip(Int32 cx, Int32 cy, Bool on, const Vector& onColor, Int32 which)
{
	// flat rounded chip; ON = colored fill with dark glyph, OFF = dark fill with grey glyph
	FillRoundRect(cx - 9, cy - 9, cx + 9, cy + 9, 5, on ? onColor : Vector(0.185, 0.185, 0.195), COL_HEADER);
	switch (which)
	{
		case 0: GlyphLock(cx, cy - 1, on); break;
		case 1: GlyphEye(cx, cy, on); break;
		case 2: GlyphSolo(cx, cy, on); break;
		default: break;
	}
}

void TCTimelineArea::GlyphSpeaker(Int32 cx, Int32 cy)
{
	DrawSetPen(Vector(0.75, 0.78, 0.85));
	DrawRectangle(cx - 6, cy - 2, cx - 3, cy + 2);
	DrawLine(cx - 3, cy - 2, cx + 1, cy - 6);
	DrawLine(cx - 3, cy + 2, cx + 1, cy + 6);
	DrawLine(cx + 1, cy - 6, cx + 1, cy + 6);
	DrawLine(cx + 4, cy - 3, cx + 4, cy + 3);
}

void TCTimelineArea::GlyphCam(Int32 cx, Int32 cy, const Vector& col)
{
	DrawSetPen(col);
	DrawRectangle(cx - 5, cy - 3, cx + 1, cy + 3); // body
	DrawLine(cx + 2, cy - 1, cx + 5, cy - 3);			 // lens
	DrawLine(cx + 5, cy - 3, cx + 5, cy + 3);
	DrawLine(cx + 5, cy + 3, cx + 2, cy + 1);
}

void TCTimelineArea::DrawPill(Int32 x, Int32 y, Int32 h, const String& text, const Vector& bg, const Vector& fg, Int32 maxRight)
{
	const Int32 tw = DrawGetTextWidth(text);
	Int32 w = tw + 12;
	if (x + w > maxRight)
		w = maxRight - x;
	if (w < 14)
		return;
	// full capsule (AA against the pill backdrop set via pillUnder)
	FillRoundRect(x, y, x + w, y + h, (h + 1) / 2, bg, pillUnder);
	DrawSetTextCol(fg, COLOR_TRANS);
	SetClippingRegion(x + 5, y, w - 10, h + 1);
	DrawText(text, x + 6, y + (h - DrawGetFontHeight()) / 2 + 1);
	ClearClippingRegion();
	SetClippingRegion(0, 0, GetWidth(), GetHeight());
}

// ------------------------------------------------------------------ drawing

void TCTimelineArea::DrawMsg(Int32 x1, Int32 y1, Int32 x2, Int32 y2, const BaseContainer& msg)
{
	OffScreenOn();
	DrawSetFont(FONT_STANDARD);
	const Int32 fh = DrawGetFontHeight();
	const Int32 w = GetWidth();
	const Int32 h = GetHeight();

	DrawSetPen(COL_BG);
	DrawRectangle(0, 0, w, h);

	BaseDocument* doc = GetActiveDocument();
	TCModel* mp = doc ? TCGetModel(doc) : nullptr;
	if (!doc || !mp)
	{
		DrawSetTextCol(COL_TEXT_DIM, COLOR_TRANS);
		DrawText(String("TakeGear: no document"), HEADER_W + 10, 10);
		return;
	}
	TCModel& m = *mp;
	if (needFit)
		FitView();

	const Int32 fps = doc->GetFps();
	const Int32 curFrame = TCCurrentFrame(doc);
	const Int32 trackCount = (Int32)m.tracks.size();
	const Int32 TOP = TopOffset();
	const Int32 audioTop = TOP + trackCount * trackH;
	const Int32 contentBottom = audioTop + AUDIO_H;
	const Bool solo = m.HasSolo();

	// ================= title bar =================
	DrawSetPen(COL_TITLE);
	DrawRectangle(0, 0, w, TITLE_H - 1);
	DrawSetTextCol(COL_TEXT, COLOR_TRANS);
	DrawText(String("TAKE SEQUENCER"), 10, (TITLE_H - fh) / 2);

	// transport buttons: go to IN | play/stop | go to OUT
	{
		const Bool playing = CheckIsRunning(CHECKISRUNNING::ANIMATIONRUNNING);
		const Int32 by = TITLE_H / 2;
		Int32 btnIndex = 0;
		auto chip = [&](Int32 x, Bool lit) {
			Vector bg = lit ? COL_ACCENT : Vector(0.205, 0.205, 0.215);
			if (hoverBtn == btnIndex && !lit)
				bg = Vector(0.275, 0.275, 0.29);
			FillRoundRect(x, by - 9, x + 22, by + 9, 5, bg, COL_TITLE);
			btnIndex++;
		};
		const Vector icon(0.88, 0.88, 0.90);
		const Vector iconDark(0.10, 0.10, 0.10);

		chip(HEADER_W + 8, false); // |<  (go to IN)
		DrawSetPen(icon);
		DrawRectangle(HEADER_W + 13, by - 5, HEADER_W + 14, by + 5);
		for (Int32 i = 0; i < 6; ++i)
			DrawLine(HEADER_W + 17 + i, by - i, HEADER_W + 17 + i, by + i); // ◀

		chip(HEADER_W + 34, playing); // play / stop
		DrawSetPen(playing ? iconDark : icon);
		if (playing)
			DrawRectangle(HEADER_W + 41, by - 4, HEADER_W + 49, by + 4);
		else
			for (Int32 i = 0; i < 6; ++i)
				DrawLine(HEADER_W + 41 + i, by - (5 - i < 0 ? 0 : 5 - i), HEADER_W + 41 + i, by + (5 - i < 0 ? 0 : 5 - i));

		chip(HEADER_W + 60, false); // >|  (go to OUT)
		DrawSetPen(icon);
		for (Int32 i = 0; i < 6; ++i)
			DrawLine(HEADER_W + 65 + i, by - (5 - i), HEADER_W + 65 + i, by + (5 - i)); // ▶
		DrawRectangle(HEADER_W + 73, by - 5, HEADER_W + 74, by + 5);
	}

	// settings chips: AUTO / MAIN / SNAP / HUD
	{
		static const Char* names[4] = { "AUTO", "MAIN", "SNAP", "HUD" };
		const Bool vals[4] = { m.autoSwitch, m.fallbackMain, m.snap, m.drawHud };
		Int32 x = HEADER_W + 94;
		const Int32 cy = TITLE_H / 2;
		for (Int32 i = 0; i < 4; ++i)
		{
			const String label(names[i]);
			const Int32 tw = DrawGetTextWidth(label);
			chipL[i] = x;
			chipR[i] = x + tw + 14;
			Vector bg = vals[i] ? COL_ACCENT : Vector(0.185, 0.185, 0.195);
			if (hoverChip == i && !vals[i])
				bg = Vector(0.26, 0.26, 0.275);
			FillRoundRect(chipL[i], cy - 8, chipR[i], cy + 8, 8, bg, COL_TITLE);
			DrawSetTextCol(vals[i] ? Vector(0.10) : COL_TEXT_DIM, COLOR_TRANS);
			DrawText(label, x + 7, (TITLE_H - fh) / 2);
			x = chipR[i] + 6;
		}

		DrawSetTextCol(COL_TEXT_DIM, COLOR_TRANS);
		DrawText(String("cam handle = offset keys | Ctrl+wheel = track height | RMB zoom | Alt pan"), x + 16, (TITLE_H - fh) / 2);
	}
	{
		String right;
		TakeData* td = doc->GetTakeData();
		if (td && td->GetCurrentTake())
			right += String(td->GetCurrentTake()->GetName()) + String("  |  ");
		right += IStr(fps) + String(" FPS");
		if (m.focusClipId != NOTOK)
			right += String("  |  FOCUS");
		const Int32 tw = DrawGetTextWidth(right);
		DrawSetTextCol(Vector(0.35, 0.62, 0.95), COLOR_TRANS);
		DrawText(right, w - tw - 10, (TITLE_H - fh) / 2);
	}

	// ================= scrollbar strip =================
	{
		DrawSetPen(COL_SCROLL_BG);
		DrawRectangle(HEADER_W, TITLE_H, w, TITLE_H + SCROLL_H - 1);
		Float ea = 0, eb = 100;
		ContentExtents(doc, m, ea, eb);
		const Float viewEnd = viewStart + (Float)(w - HEADER_W) / ppf;
		Float t1 = (viewStart - ea) / (eb - ea);
		Float t2 = (viewEnd - ea) / (eb - ea);
		t1 = t1 < 0 ? 0 : (t1 > 1 ? 1 : t1);
		t2 = t2 < 0 ? 0 : (t2 > 1 ? 1 : t2);
		const Int32 tx1 = HEADER_W + (Int32)(t1 * (w - HEADER_W));
		const Int32 tx2 = HEADER_W + (Int32)(t2 * (w - HEADER_W));
		FillRoundRect(tx1, TITLE_H + 2, (tx2 > tx1 + 10) ? tx2 : tx1 + 10, TITLE_H + SCROLL_H - 3, (SCROLL_H - 5) / 2 + 1, COL_SCROLL_THUMB);
	}

	// ================= ruler =================
	DrawSetPen(COL_RULER);
	DrawRectangle(HEADER_W, TITLE_H + SCROLL_H, w, TOP - 1);

	Int32 step = 10000;
	{
		static const Int32 steps[] = { 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000 };
		for (Int32 s : steps)
		{
			if ((Float)s * ppf >= 56.0)
			{
				step = s;
				break;
			}
		}
	}
	const Int32 rulerY = TITLE_H + SCROLL_H;

	// dense minor ticks (per frame) like a film strip
	if (ppf >= 3.0)
	{
		DrawSetPen(Vector(0.22, 0.22, 0.235));
		const Int32 f0 = (Int32)XToFrame(HEADER_W) - 1;
		const Int32 f1 = (Int32)XToFrame(w) + 1;
		for (Int32 f = f0; f <= f1; ++f)
		{
			const Int32 x = FrameToX((Float)f);
			if (x >= HEADER_W)
				DrawLine(x, TOP - 7, x, TOP - 1);
		}
	}

	const Int32 firstF = (Int32)(XToFrame(HEADER_W) / step) * step - step;
	const Int32 lastF = (Int32)(XToFrame(w)) + step;
	for (Int32 f = firstF; f <= lastF; f += step)
	{
		const Int32 x = FrameToX((Float)f);
		if (x < HEADER_W)
			continue;
		DrawSetPen(COL_TICK);
		DrawLine(x, TOP - 13, x, TOP - 1);
		DrawSetPen(COL_GRID);
		DrawLine(x, TOP, x, contentBottom);
		DrawSetTextCol(COL_TEXT_DIM, COLOR_TRANS);
		DrawText(IStr(f), x + 3, rulerY + 1);
	}

	// ================= track rows =================
	// frames outside the document range get a darker background (like an NLE)
	const Int32 docMinX = FrameToX((Float)doc->GetMinTime().GetFrame(fps));
	const Int32 docMaxX = FrameToX((Float)(doc->GetMaxTime().GetFrame(fps) + 1));
	auto dimOutsideDoc = [&](Int32 top, Int32 bot, const Vector& base) {
		DrawSetPen(base * 0.55);
		if (docMinX > HEADER_W)
			DrawRectangle(HEADER_W, top, (docMinX < w) ? docMinX : w, bot);
		if (docMaxX < w)
			DrawRectangle((docMaxX > HEADER_W) ? docMaxX : HEADER_W, top, w, bot);
	};

	for (Int32 t = 0; t < trackCount; ++t)
	{
		const Int32 top = TrackTop(t);
		Vector col = COL_ROW;
		if (solo ? !m.tracks[t].solo : m.tracks[t].muted)
			col = col * 0.6;
		DrawSetPen(col);
		DrawRectangle(HEADER_W, top, w, top + trackH - 2);
		dimOutsideDoc(top, top + trackH - 2, col);
		DrawSetPen(COL_ROW_SEP);
		DrawRectangle(HEADER_W, top + trackH - 2, w, top + trackH - 1);
	}

	// ================= clips =================
	// refresh the camera info cache at most every 400ms (it survives redraws)
	{
		const Float64 now = GeGetMilliSeconds();
		if (now - camCacheStamp > 400.0)
		{
			camCache.clear();
			camCacheStamp = now;
		}
	}

	for (const auto& c : m.clips)
	{
		if (c.track < 0 || c.track >= trackCount)
			continue;
		const Int32 cx1 = FrameToX((Float)c.start);
		const Int32 cx2 = FrameToX((Float)c.end);
		if (cx2 < HEADER_W || cx1 > w)
			continue;
		const Int32 top = TrackTop(c.track) + 3;
		const Int32 bot = TrackTop(c.track) + trackH - 6;

		Float dim = 1.0;
		const TCTrack& tr = m.tracks[c.track];
		if (solo ? !tr.solo : tr.muted)
			dim = 0.45;
		if (m.focusClipId != NOTOK && m.focusClipId != c.id)
			dim *= 0.5;

		const Bool active = c.Contains(curFrame);
		const Int32 dx1 = (cx1 < HEADER_W) ? HEADER_W : cx1;

		// flat rounded body with a subtle outline; selection = bright frame
		const Int32 RAD = 6;
		const Bool isSel = IsSelected(c.id);
		Vector frameCol;
		if (isSel)
			frameCol = COL_SELECT;
		else if (active)
			frameCol = COL_ACCENT;
		else if (c.id == hoverClipId)
			frameCol = Lighten(c.color, 1.45) * dim; // hover: brighter frame
		else
			frameCol = c.color * 0.5 * dim;
		FillRoundRect(dx1, top, cx2 - 1, bot, RAD, frameCol, COL_ROW);
		const Int32 inset = (isSel || active) ? 2 : 1;
		const Vector bodyCol = c.color * (0.82 * dim);
		FillRoundRect(dx1 + inset, top + inset, cx2 - 1 - inset, bot - inset, RAD - inset, bodyCol, frameCol);
		pillUnder = bodyCol;

		// ---- frame thumbnail (left part of the body, between pills and handle)
		{
			const Int32 thTop = top + fh + 6;
			const Int32 thBot = bot - CAMLANE_H - 2;
			const Int32 thH = thBot - thTop;
			if (thH >= 24 && cx2 - dx1 > 60)
			{
				BaseBitmap* thumb = TCThumbGet(doc, c.takeName, c.start);
				if (thumb)
				{
					const Int32 thW = thH * TC_THUMB_W / TC_THUMB_H;
					if (dx1 + 4 + thW < cx2 - 6)
					{
						DrawBitmap(thumb, dx1 + 4, thTop, thW, thH, 0, 0, thumb->GetBw(), thumb->GetBh(), BMP_NORMALSCALED);
						DrawSetPen(Vector(0.05));
						DrawLine(dx1 + 4, thTop, dx1 + 4 + thW, thTop);
						DrawLine(dx1 + 4, thBot, dx1 + 4 + thW, thBot);
						DrawLine(dx1 + 4, thTop, dx1 + 4, thBot);
						DrawLine(dx1 + 4 + thW, thTop, dx1 + 4 + thW, thBot);
					}
				}
			}
		}

		// ---- camera key-offset handle (bottom strip): grab and drag horizontally
		// to slide the take camera keys in time (zone 3). pure handle — the camera
		// name lives in the clip body, not here.
		const CamInfo& cam = CamInfoFor(doc, c.takeName);
		const Int32 laneTop = bot - CAMLANE_H;
		if (cx2 - dx1 > 16)
		{
			const Vector laneCol = Vector(0.07, 0.07, 0.08) + c.color * (0.10 * dim);
			FillRoundRect(dx1 + 3, laneTop, cx2 - 4, bot - 3, 4, laneCol, bodyCol);

			// grip dots on both ends — affordance for "this strip is draggable"
			DrawSetPen(Vector(0.45, 0.47, 0.52) * dim);
			const Int32 gy = (laneTop + bot - 3) / 2;
			for (Int32 g = 0; g < 3; ++g)
			{
				DrawRectangle(dx1 + 8 + g * 4, gy - 1, dx1 + 9 + g * 4, gy);
				DrawRectangle(cx2 - 10 - g * 4, gy - 1, cx2 - 9 - g * 4, gy);
			}

			// keys (with the pending offset while dragging the lane)
			const Bool camDragging = (c.id == dragCamClipId);
			const Int32 keyOff = camDragging ? dragCamOffset : 0;
			if (!cam.keys.empty())
			{
				// colored range bar: first key .. last key. while dragging it may
				// stick out of the clip so the new key positions are obvious.
				const Int32 kFirst = cam.keys.front() + keyOff;
				const Int32 kLast = cam.keys.back() + keyOff;
				Int32 bx1 = FrameToX((Float)kFirst);
				Int32 bx2 = FrameToX((Float)kLast) + 1;
				if (!camDragging)
				{
					bx1 = ClampInt(bx1, dx1 + 3, cx2 - 4);
					bx2 = ClampInt(bx2, dx1 + 3, cx2 - 4);
				}
				else
				{
					bx1 = ClampInt(bx1, HEADER_W, w);
					bx2 = ClampInt(bx2, HEADER_W, w);
				}
				if (bx2 > bx1)
					FillRoundRect(bx1, laneTop + 2, bx2, bot - 5, 3,
						camDragging ? Vector(0.55, 0.36, 0.10) : Lighten(c.color, 1.25) * (0.55 * dim),
						camDragging ? COL_ROW : laneCol);

				DrawSetPen(camDragging ? COL_ACCENT : Vector(0.92, 0.94, 0.97) * dim);
				for (Int32 kf : cam.keys)
				{
					const Int32 sf = kf + keyOff;
					// while dragging show keys even outside the clip bounds
					if (!camDragging && (sf < c.start || sf >= c.end))
						continue;
					const Int32 kx = FrameToX((Float)sf);
					const Int32 lo = camDragging ? HEADER_W : dx1 + 2;
					const Int32 hi = camDragging ? w : cx2 - 2;
					if (kx >= lo && kx < hi)
					{
						DrawLine(kx, laneTop + 2, kx, bot - 4);
						DrawLine(kx + 1, laneTop + 2, kx + 1, bot - 4);
					}
				}
			}

			// live offset readout while dragging
			if (c.id == dragCamClipId && dragCamOffset != 0)
			{
				const String ds = ((dragCamOffset > 0) ? String("+") : String("")) + IStr(dragCamOffset) + String("f");
				const Int32 dw = DrawGetTextWidth(ds);
				FillRoundRect(cx2 - dw - 16, laneTop, cx2 - 5, bot - 3, 4, Vector(0.05, 0.05, 0.05), laneCol);
				DrawSetTextCol(COL_ACCENT, COLOR_TRANS);
				DrawText(ds, cx2 - dw - 11, laneTop + (CAMLANE_H - fh) / 2 + 1);
			}
		}

		// (camera name is drawn on the top pill row, centered — see below)

		// focus marker: small accent dot row at the clip top
		if (m.focusClipId == c.id)
		{
			DrawSetPen(COL_ACCENT);
			for (Int32 x = dx1 + 8; x < cx2 - 8; x += 8)
				DrawRectangle(x, top + 2, x + 2, top + 3);
		}

		// name pill + tags
		if (cx2 - dx1 > 30)
		{
			const Int32 pillH = fh + 1;
			const Int32 pillY = top + 3;

			// status tag on the right (drawn first so the name pill can respect it)
			Int32 nameMaxRight = cx2 - 8;
			const String st = String(TCStatusNames()[(c.status >= 0 && c.status < TC_STATUS_COUNT) ? c.status : 0]);
			const Int32 stw = DrawGetTextWidth(st) + 10;
			const Int32 sx = cx2 - stw - 6;
			if (cx2 - dx1 > stw + 60)
			{
				const Vector bg = (c.status == 1) ? COL_WIP_BG : TCStatusColor(c.status);
				DrawPill(sx, pillY, pillH, st, bg * dim, Vector(0.97), cx2 - 4);
				nameMaxRight = sx - 6;
			}

			String label = c.DisplayName();
			if (c.note.IsPopulated())
				label += String(" *");
			DrawPill(dx1 + 5, pillY, pillH, label, COL_PILL_BG, COL_PILL_FG, nameMaxRight);

			// camera-follow lock glyph right after the name pill
			const Int32 nameEnd = dx1 + 5 + DrawGetTextWidth(label) + 14;
			if (c.camFollow)
			{
				const Int32 lx = nameEnd + 8;
				if (lx < nameMaxRight - 6)
					GlyphLock(lx, pillY + 4, true);
			}

			// camera name centered on the top row (between name and status pills)
			{
				const String camLabel = cam.camName.IsPopulated() ? cam.camName : String();
				if (camLabel.IsPopulated())
				{
					const Int32 ctw = DrawGetTextWidth(camLabel);
					const Int32 cxm = (dx1 + cx2) / 2;
					const Int32 lo = nameEnd + (c.camFollow ? 26 : 10);
					if (cxm - ctw / 2 - 12 > lo && cxm + ctw / 2 < nameMaxRight - 4)
					{
						const Vector camCol = c.camFollow ? COL_ACCENT : Vector(0.13, 0.13, 0.15);
						GlyphCam(cxm - ctw / 2 - 10, pillY + pillH / 2, camCol);
						DrawSetTextCol(camCol, COLOR_TRANS);
						DrawText(camLabel, cxm - ctw / 2 + 2, pillY + (pillH - fh) / 2 + 1);
					}
				}
			}
		}

		// locked hatching
		if (tr.locked)
		{
			DrawSetPen(Vector(0.05));
			for (Int32 x = dx1; x < cx2 - 1; x += 9)
				DrawLine(x, bot, (x + 6 < cx2 - 1) ? x + 6 : cx2 - 1, top);
		}
	}

	// ================= audio row =================
	DrawSetPen(COL_AUDIO_BG);
	DrawRectangle(HEADER_W, audioTop, w, contentBottom - 1);
	dimOutsideDoc(audioTop, contentBottom - 1, COL_AUDIO_BG);

	if (!m.audioPeaks.empty() && m.audioSeconds > 0.0)
	{
		const Int32 lenF = (Int32)(m.audioSeconds * fps);
		const Int32 ax1 = FrameToX((Float)m.audioOffset);
		const Int32 ax2 = FrameToX((Float)(m.audioOffset + lenF));
		const Int32 bx1 = (ax1 < HEADER_W) ? HEADER_W : ax1;

		const Int32 mid = audioTop + (AUDIO_H + 10) / 2;
		const Int32 amp = (AUDIO_H - 16) / 2;
		const Int32 nb = (Int32)m.audioPeaks.size();
		DrawSetPen(COL_AUDIO_WAVE);
		for (Int32 x = bx1; x < ((ax2 < w) ? ax2 : w); ++x)
		{
			const Float f = XToFrame(x);
			const Float sec = (f - (Float)m.audioOffset) / (Float)fps;
			if (sec < 0.0 || sec >= m.audioSeconds)
				continue;
			Int32 b = (Int32)(sec / m.audioSeconds * nb);
			b = ClampInt(b, 0, nb - 1);
			const Int32 a = (Int32)(m.audioPeaks[b] * amp);
			DrawLine(x, mid - a, x, mid + a);
		}

		// block border + file name
		DrawSetPen(COL_AUDIO_BORDER);
		DrawLine(bx1, audioTop + 1, (ax2 < w) ? ax2 : w, audioTop + 1);
		DrawLine(bx1, contentBottom - 2, (ax2 < w) ? ax2 : w, contentBottom - 2);
		if (ax1 >= HEADER_W)
			DrawLine(ax1, audioTop + 1, ax1, contentBottom - 2);
		if (ax2 < w)
			DrawLine(ax2, audioTop + 1, ax2, contentBottom - 2);
		DrawSetTextCol(Vector(0.65, 0.75, 0.95), COLOR_TRANS);
		DrawText(String(Filename(m.audioPath).GetFileString()), bx1 + 6, audioTop + 3);
	}
	else
	{
		const String hint("no reference audio — load via toolbar 'Audio...'");
		const Int32 hw = DrawGetTextWidth(hint);
		DrawSetPen(COL_AUDIO_BG);
		DrawRectangle(HEADER_W + 4, audioTop + (AUDIO_H - fh) / 2 - 2, HEADER_W + 12 + hw, audioTop + (AUDIO_H + fh) / 2 + 2);
		DrawSetTextCol(COL_TEXT_DIM, COLOR_TRANS);
		DrawText(hint, HEADER_W + 8, audioTop + (AUDIO_H - fh) / 2);
	}
	DrawSetPen(COL_ROW_SEP);
	DrawRectangle(HEADER_W, contentBottom - 1, w, contentBottom);

	// ================= IN / OUT =================
	// minimal: thin accent lines with small triangular grab handles in the ruler
	{
		const Int32 xi = FrameToX((Float)m.inFrame);
		const Int32 xo = FrameToX((Float)m.outFrame);
		if (xi >= HEADER_W)
		{
			DrawSetPen(COL_ACCENT);
			DrawLine(xi, rulerY, xi, contentBottom);
			for (Int32 i = 0; i < 5; ++i)
				DrawLine(xi + i, rulerY + i, xi + i, rulerY + 8 - i); // ▶ into the range
		}
		if (xo >= HEADER_W)
		{
			DrawSetPen(COL_ACCENT);
			DrawLine(xo, rulerY, xo, contentBottom);
			for (Int32 i = 0; i < 5; ++i)
				DrawLine(xo - i, rulerY + i, xo - i, rulerY + 8 - i); // ◀ into the range
		}

		// live frame number, only while a handle is being dragged
		if (dragRange != 0)
		{
			const Int32 hx = (dragRange == 1) ? xi : xo;
			const String fs = IStr((dragRange == 1) ? m.inFrame : m.outFrame);
			const Int32 tw = DrawGetTextWidth(fs);
			Int32 bx = (dragRange == 1) ? hx + 7 : hx - tw - 17;
			FillRoundRect(bx, rulerY + 1, bx + tw + 10, rulerY + fh + 3, (fh + 2) / 2, Vector(0.05, 0.05, 0.05), COL_RULER);
			DrawSetTextCol(COL_ACCENT, COLOR_TRANS);
			DrawText(fs, bx + 5, rulerY + 2);
		}
	}

	// ================= markers =================
	for (const auto& mk : m.markers)
	{
		const Int32 x = FrameToX((Float)mk.frame);
		if (x < HEADER_W || x > w)
			continue;
		// dim line through the content only
		DrawSetPen(COL_MARKER * 0.45);
		DrawLine(x, TOP, x, contentBottom);
		// flag (stem + rounded pennant) in the lower ruler lane
		DrawSetPen(COL_MARKER);
		DrawRectangle(x - 1, TOP - 12, x, TOP - 1);
		FillRoundRect(x, TOP - 12, x + 7, TOP - 6, 2, COL_MARKER);
		// label tag with a dark rounded backing so it never collides with numbers
		if (mk.label.IsPopulated())
		{
			const Int32 tw = DrawGetTextWidth(mk.label);
			FillRoundRect(x + 9, TOP - 14, x + 9 + tw + 8, TOP - 2, 5, Vector(0.05, 0.05, 0.05));
			DrawSetTextCol(COL_MARKER, COLOR_TRANS);
			DrawText(mk.label, x + 13, TOP - 14);
		}
	}

	// ================= drag&drop ghost =================
	if (dragHover && dragTrack >= 0 && dragTrack < trackCount)
	{
		const Int32 gx1 = FrameToX((Float)dragFrame);
		const Int32 gx2 = FrameToX((Float)(dragFrame + fps));
		const Int32 gtop = TrackTop(dragTrack) + 3;
		const Int32 gbot = TrackTop(dragTrack) + trackH - 6;
		const Int32 ggx1 = (gx1 < HEADER_W) ? HEADER_W : gx1;
		FillRoundRect(ggx1, gtop, gx2 - 1, gbot, 6, COL_ACCENT);
		FillRoundRect(ggx1 + 2, gtop + 2, gx2 - 3, gbot - 2, 5, Vector(0.165, 0.165, 0.176));
		DrawSetTextCol(COL_ACCENT, COLOR_TRANS);
		SetClippingRegion(gx1 + 4, gtop, gx2 - gx1 - 8, gbot - gtop);
		DrawText(dragName, gx1 + 6, gtop + ((gbot - gtop) - fh) / 2);
		ClearClippingRegion();
		SetClippingRegion(0, 0, w, h);
	}

	// ================= empty-state hint =================
	if (m.clips.empty() && !dragHover)
	{
		DrawSetTextCol(COL_TEXT_DIM, COLOR_TRANS);
		DrawText(String("Drag a take from SOURCE TAKES onto a track, or press 'A' to add a clip for the current take"), HEADER_W + 24, TOP + trackH / 2 - fh / 2);
	}

	// ================= playhead =================
	{
		const Int32 x = FrameToX((Float)curFrame);
		if (x >= HEADER_W)
		{
			DrawSetPen(COL_ACCENT);
			DrawLine(x, TITLE_H, x, contentBottom);
			FillRoundRect(x - 3, TITLE_H + 1, x + 3, TITLE_H + SCROLL_H - 1, 3, COL_ACCENT);
			// current frame badge in the ruler numbers row (never on top of clips)
			const String fs = IStr(curFrame);
			const Int32 tw = DrawGetTextWidth(fs);
			Int32 bx = x + 5;
			if (bx + tw + 10 > w)
				bx = x - tw - 15;
			FillRoundRect(bx, rulerY + 1, bx + tw + 10, rulerY + fh + 3, (fh + 2) / 2, Vector(0.05, 0.05, 0.05));
			DrawSetTextCol(COL_ACCENT, COLOR_TRANS);
			DrawText(fs, bx + 5, rulerY + 2);
		}
	}

	// ================= marquee selection rect =================
	if (marqueeOn)
	{
		DrawSetPen(COL_ACCENT);
		DrawLine(mqX1, mqY1, mqX2, mqY1);
		DrawLine(mqX1, mqY2, mqX2, mqY2);
		DrawLine(mqX1, mqY1, mqX1, mqY2);
		DrawLine(mqX2, mqY1, mqX2, mqY2);
	}

	// ================= header column =================
	DrawSetPen(COL_HEADER);
	DrawRectangle(0, TITLE_H, HEADER_W - 1, h);
	DrawSetPen(COL_ROW_SEP);
	DrawLine(HEADER_W - 1, 0, HEADER_W - 1, h);
	DrawLine(0, TOP - 1, w, TOP - 1);

	DrawSetTextCol(COL_TEXT_DIM, COLOR_TRANS);
	DrawText(String("FRAME"), 10, rulerY + (RULER_H + SCROLL_H - fh) / 2 - 4);

	for (Int32 t = 0; t < trackCount; ++t)
	{
		const Int32 top = TrackTop(t);
		const Int32 cy = top + (trackH - 2) / 2;

		if (t == curTrack)
			FillRoundRect(2, top + 2, HEADER_W - 4, top + trackH - 4, 6, COL_HEADER_SEL);
		else if (t == hoverTrack)
			FillRoundRect(2, top + 2, HEADER_W - 4, top + trackH - 4, 6, Vector(0.172, 0.172, 0.182));
		// colored spine (rounded)
		FillRoundRect(0, top + 2, 3, top + trackH - 4, 1, m.tracks[t].color);

		ToggleChip(19, cy, m.tracks[t].locked, Vector(0.95, 0.65, 0.25), 0);
		ToggleChip(42, cy, !m.tracks[t].muted, Vector(0.62, 0.66, 0.72), 1);
		ToggleChip(65, cy, m.tracks[t].solo, Vector(0.95, 0.85, 0.30), 2);

		// color swatch (rounded)
		FillRoundRect(80, cy - 7, 94, cy + 7, 4, m.tracks[t].color);

		// name
		SetClippingRegion(100, top, HEADER_W - 104, trackH);
		DrawSetTextCol((t == curTrack) ? COL_ACCENT : COL_TEXT, COLOR_TRANS);
		DrawText(m.tracks[t].name, 102, top + (trackH - 2 - fh) / 2);
		ClearClippingRegion();
		SetClippingRegion(0, 0, w, h);

		DrawSetPen(COL_ROW_SEP);
		DrawLine(0, top + trackH - 2, HEADER_W - 1, top + trackH - 2);
	}

	// audio header
	{
		const Int32 cy = audioTop + AUDIO_H / 2;
		FillRoundRect(0, audioTop + 2, 3, contentBottom - 4, 1, COL_AUDIO_BORDER);
		GlyphSpeaker(16, cy);
		DrawSetTextCol(COL_TEXT, COLOR_TRANS);
		DrawText(String("REF AUDIO"), 30, audioTop + 8);
		DrawSetTextCol(COL_TEXT_DIM, COLOR_TRANS);
		if (!m.audioPeaks.empty() && m.audioSeconds > 0.0)
			DrawText(String("WAV ") + IStr((Int64)m.audioPeaks.size()) + String(" | ") + IStr((Int64)(m.audioSeconds * fps)) + String("f"), 30, audioTop + 10 + fh);
		else
			DrawText(String("empty"), 30, audioTop + 10 + fh);
	}

	// title bar corner over header
	DrawSetPen(COL_TITLE);
	DrawRectangle(0, 0, HEADER_W - 1, TITLE_H - 1);
	DrawSetTextCol(COL_TEXT, COLOR_TRANS);
	DrawText(String("TAKE SEQUENCER"), 10, (TITLE_H - fh) / 2);
}

// ------------------------------------------------------------------ hit tests

TCTimelineArea::ClipHit TCTimelineArea::HitClip(const TCModel& m, Int32 mx, Int32 my) const
{
	ClipHit hit;
	const Int32 trackCount = (Int32)m.tracks.size();
	const Int32 t = TrackAtY(my, trackCount);
	if (t == NOTOK || mx < HEADER_W)
		return hit;
	const TCClip* best = nullptr;
	for (const auto& c : m.clips)
	{
		if (c.track != t)
			continue;
		const Int32 x1 = FrameToX((Float)c.start);
		const Int32 x2 = FrameToX((Float)c.end);
		if (mx < x1 - 3 || mx > x2 + 3)
			continue;
		if (!best || c.start > best->start)
			best = &c;
	}
	if (!best)
		return hit;
	hit.clipId = best->id;
	const Int32 x1 = FrameToX((Float)best->start);
	const Int32 x2 = FrameToX((Float)best->end);
	if (mx <= x1 + 5)
		hit.zone = 1;
	else if (mx >= x2 - 5)
		hit.zone = 2;
	else
		hit.zone = 0;
	// camera timing lane at the clip bottom (body area only)
	if (hit.zone == 0)
	{
		const Int32 bot = TrackTop(best->track) + trackH - 6;
		if (my >= bot - CAMLANE_H && my <= bot)
			hit.zone = 3;
	}
	return hit;
}

Int32 TCTimelineArea::HitMarker(const TCModel& m, Int32 mx, Int32 my) const
{
	// markers live in the lower lane of the ruler (the flag area)
	if (my >= TopOffset() || my < TopOffset() - 15)
		return NOTOK;
	for (Int32 i = 0; i < (Int32)m.markers.size(); ++i)
	{
		const Int32 x = FrameToX((Float)m.markers[i].frame);
		if (mx >= x - 4 && mx <= x + 8)
			return i;
	}
	return NOTOK;
}

Int32 TCTimelineArea::SnapFrame(const TCModel& m, BaseDocument* doc, Float frame, Int32 ignoreClipId) const
{
	Int32 best = RoundToInt(frame);
	if (!m.snap)
		return best;
	const Float tol = 6.0 / ppf;
	Float bestD = tol;
	auto consider = [&](Int32 f) {
		const Float d = AbsF(frame - (Float)f);
		if (d < bestD)
		{
			bestD = d;
			best = f;
		}
	};
	for (const auto& c : m.clips)
	{
		if (c.id == ignoreClipId)
			continue;
		consider(c.start);
		consider(c.end);
	}
	for (const auto& mk : m.markers)
		consider(mk.frame);
	consider(m.inFrame);
	consider(m.outFrame);
	consider(TCCurrentFrame(doc));
	consider(0);
	return best;
}

// ------------------------------------------------------------------ edits

const TCTimelineArea::CamInfo& TCTimelineArea::CamInfoFor(BaseDocument* doc, const String& takeName)
{
	for (auto& p : camCache)
		if (p.take == takeName)
			return p;
	camCache.push_back({ takeName, TCTakeCameraName(doc, takeName), {} });
	TCCameraKeyFrames(doc, takeName, camCache.back().keys);
	return camCache.back();
}

void TCTimelineArea::Edited(BaseDocument* doc, Bool switchTake)
{
	editCounter++;
	camCache.clear(); // model changed: refresh camera lanes immediately
	if (switchTake)
		TCSwitchTakeForTime(doc);
	EventAdd();
	Redraw();
}

// ------------------------------------------------------------------ input

Bool TCTimelineArea::InputEvent(const BaseContainer& msg)
{
	BaseDocument* doc = GetActiveDocument();
	if (!doc)
		return false;
	TCModel* mp = TCGetModel(doc);
	if (!mp)
		return false;
	TCModel& m = *mp;

	const Int32 device = msg.GetInt32(BFM_INPUT_DEVICE);
	const Int32 channel = msg.GetInt32(BFM_INPUT_CHANNEL);

	if (device == BFM_INPUT_KEYBOARD)
		return HandleKey(doc, m, msg);
	if (device != BFM_INPUT_MOUSE)
		return false;

	const Int32 screenX = msg.GetInt32(BFM_INPUT_X);
	const Int32 screenY = msg.GetInt32(BFM_INPUT_Y);
	Int32 mx = screenX, my = screenY;
	Global2Local(&mx, &my);
	const Int32 qual = msg.GetInt32(BFM_INPUT_QUALIFIER);
	const Bool dbl = msg.GetBool(BFM_INPUT_DOUBLECLICK);

	const Int32 trackCount = (Int32)m.tracks.size();
	const Int32 TOP = TopOffset();
	const Int32 audioTop = TOP + trackCount * trackH;

	// ---- mouse wheel: zoom / pan / track height (Ctrl)
	if (channel == BFM_INPUT_MOUSEWHEEL)
	{
		const Float val = msg.GetFloat(BFM_INPUT_VALUE);
		if (qual & QCTRL)
		{
			trackH = ClampInt(trackH + ((val > 0) ? 4 : -4), 40, 96);
			Redraw();
			return true;
		}
		if (qual & QSHIFT)
		{
			viewStart += ((val > 0) ? -60.0 : 60.0) / ppf;
		}
		else
		{
			const Float anchor = XToFrame(mx);
			const Float f = (val > 0) ? 1.2 : (1.0 / 1.2);
			ppf *= f;
			if (ppf < 0.05)
				ppf = 0.05;
			if (ppf > 200.0)
				ppf = 200.0;
			viewStart = anchor - (Float)(mx - HEADER_W) / ppf;
		}
		Redraw();
		return true;
	}

	// ---- middle mouse: pan
	if (channel == BFM_INPUT_MOUSEMIDDLE)
	{
		DragPan(mx, my, BFM_INPUT_MOUSEMIDDLE);
		return true;
	}

	// ---- right mouse: drag = zoom, click = context menu
	if (channel == BFM_INPUT_MOUSERIGHT)
	{
		if (!DragZoom(mx, my))
			ContextMenu(doc, m, mx, my, screenX, screenY);
		return true;
	}

	if (channel != BFM_INPUT_MOUSELEFT)
		return false;

	// ---- Alt+LMB anywhere in the content: pan
	if ((qual & QALT) && mx >= HEADER_W)
	{
		DragPan(mx, my, BFM_INPUT_MOUSELEFT);
		return true;
	}

	// ---- title row: transport buttons + settings chips
	if (my < TITLE_H)
	{
		if (mx >= HEADER_W + 8 && mx <= HEADER_W + 30)
		{
			TCSetDocFrame(doc, m.inFrame);
			Redraw();
		}
		else if (mx >= HEADER_W + 34 && mx <= HEADER_W + 56)
		{
			if (CheckIsRunning(CHECKISRUNNING::ANIMATIONRUNNING))
				TCStopPlayback(doc);
			else
				CallCommand(CID_PLAY_FORWARD);
			Redraw();
		}
		else if (mx >= HEADER_W + 60 && mx <= HEADER_W + 82)
		{
			TCSetDocFrame(doc, m.outFrame);
			Redraw();
		}
		else
		{
			for (Int32 i = 0; i < 4; ++i)
			{
				if (mx >= chipL[i] && mx <= chipR[i] && chipR[i] > chipL[i])
				{
					TCAddUndo(doc);
					switch (i)
					{
						case 0: m.autoSwitch = !m.autoSwitch; break;
						case 1: m.fallbackMain = !m.fallbackMain; break;
						case 2: m.snap = !m.snap; break;
						case 3:
							m.drawHud = !m.drawHud;
							DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD);
							break;
					}
					TCSwitchTakeForTime(doc);
					EventAdd();
					Redraw();
					break;
				}
			}
		}
		return true;
	}

	// ---- scrollbar strip
	if (my < TITLE_H + SCROLL_H && mx >= HEADER_W)
	{
		DragScrollbar(doc, m, mx, my);
		return true;
	}

	// ---- header column
	if (mx < HEADER_W)
	{
		// drag a row boundary = resize track height
		for (Int32 t = 0; t < trackCount; ++t)
		{
			const Int32 boundary = TrackTop(t) + trackH - 2;
			if (my >= boundary - 3 && my <= boundary + 3)
			{
				Int32 x = mx, y = my;
				while (PollDrag(BFM_INPUT_MOUSELEFT, x, y))
				{
					const Int32 nh = (y - TopOffset()) / (t + 1);
					trackH = ClampInt(nh, 40, 96);
					Redraw();
				}
				return true;
			}
		}

		const Int32 t = TrackAtY(my, trackCount);
		if (t != NOTOK)
		{
			const Int32 cy = TrackTop(t) + (trackH - 2) / 2;
			if (my >= cy - 9 && my <= cy + 9)
			{
				Bool handled = true;
				if (mx >= 10 && mx <= 28)
				{
					TCAddUndo(doc);
					m.tracks[t].locked = !m.tracks[t].locked;
				}
				else if (mx >= 33 && mx <= 51)
				{
					TCAddUndo(doc);
					m.tracks[t].muted = !m.tracks[t].muted;
				}
				else if (mx >= 56 && mx <= 74)
				{
					TCAddUndo(doc);
					m.tracks[t].solo = !m.tracks[t].solo;
				}
				else if (mx >= 80 && mx <= 94)
				{
					// track color popup
					curTrack = t;
					BaseContainer menu;
					for (Int32 i = 0; i < TC_PALETTE_COUNT; ++i)
						menu.InsData(POP_TRACKCOLOR_BASE + i, String("Color ") + IStr(i + 1));
					const Int32 res = ShowPopupMenu(nullptr, MOUSEPOS, MOUSEPOS, menu);
					if (res >= POP_TRACKCOLOR_BASE && res < POP_TRACKCOLOR_BASE + TC_PALETTE_COUNT)
					{
						TCAddUndo(doc);
						m.tracks[t].color = TCPaletteColor(res - POP_TRACKCOLOR_BASE);
					}
				}
				else
				{
					handled = false;
				}
				if (handled)
				{
					Edited(doc);
					return true;
				}
			}
			curTrack = t;
			if (dbl)
			{
				String name = m.tracks[t].name;
				if (RenameDialog(&name))
				{
					TCAddUndo(doc);
					m.tracks[t].name = name;
					Edited(doc, false);
				}
			}
			Redraw();
		}
		return true;
	}

	// ---- ruler
	if (my < TOP)
	{
		const Int32 mk = HitMarker(m, mx, my);
		if (mk != NOTOK)
		{
			if (dbl)
			{
				String label = m.markers[mk].label;
				if (RenameDialog(&label))
				{
					TCAddUndo(doc);
					m.markers[mk].label = label;
					Edited(doc, false);
				}
				return true;
			}
			DragMarker(doc, m, mk, mx, my);
			return true;
		}
		// hit priority like in Resolve: scrubbing owns the ruler; the IN/OUT
		// handles are grabbed only within ±6px of their line. The big tags are
		// visual labels, not click targets — so the playhead can always be
		// grabbed even right next to a range handle.
		const Int32 xi = FrameToX((Float)m.inFrame);
		const Int32 xo = FrameToX((Float)m.outFrame);
		const Float dIn = AbsF((Float)(mx - xi));
		const Float dOut = AbsF((Float)(mx - xo));
		if (dIn <= 6.0 || dOut <= 6.0)
		{
			DragInOut(doc, m, dIn <= dOut, mx, my);
			return true;
		}
		DragScrub(doc, mx, my);
		return true;
	}

	// ---- audio row
	if (my >= audioTop && my < audioTop + AUDIO_H)
	{
		if (!m.audioPeaks.empty())
			DragAudio(doc, m, mx, my);
		return true;
	}

	// ---- clip area
	const ClipHit hit = HitClip(m, mx, my);
	if (hit.clipId != NOTOK)
	{
		// Shift+click toggles membership; clicking an already selected clip
		// keeps the group (so a group drag can start from any member)
		if (qual & QSHIFT)
		{
			AddSelect(hit.clipId);
			Redraw();
			return true;
		}
		if (!IsSelected(hit.clipId))
			SelectOnly(hit.clipId);
		else
			selId = hit.clipId; // make it the primary
		const TCClip* c = m.FindClip(hit.clipId);
		if (c)
			curTrack = c->track;
		if (dbl)
		{
			TCClip* cc = m.FindClip(hit.clipId);
			if (cc)
			{
				String name = cc->DisplayName();
				if (RenameDialog(&name))
				{
					TCAddUndo(doc);
					cc->name = name;
					Edited(doc, false);
				}
			}
			return true;
		}
		Redraw();
		if (hit.zone == 3)
			DragCamLane(doc, m, hit.clipId, mx, my);
		else
			DragClip(doc, m, hit, mx, my, qual);
		return true;
	}

	// empty area: ctrl-drag pans, plain drag = marquee selection
	if (qual & QCTRL)
	{
		DragPan(mx, my, BFM_INPUT_MOUSELEFT);
		return true;
	}
	DragMarquee(doc, m, mx, my);
	return true;
}

// ------------------------------------------------------------------ drag&drop receive

Int32 TCTimelineArea::Message(const BaseContainer& msg, BaseContainer& result)
{
	switch (msg.GetId())
	{
		case BFM_CURSORINFO_REMOVE:
		{
			if (hoverClipId != NOTOK || hoverBtn != NOTOK || hoverChip != NOTOK || hoverTrack != NOTOK)
			{
				hoverClipId = hoverBtn = hoverChip = hoverTrack = NOTOK;
				Redraw();
			}
			break;
		}
		case BFM_GETCURSORINFO:
		{
			// context-sensitive mouse cursors + hover tracking
			BaseDocument* doc = GetActiveDocument();
			TCModel* m = doc ? TCGetModel(doc) : nullptr;
			if (!doc || !m)
				break;
			BaseContainer state;
			if (!GetInputState(BFM_INPUT_MOUSE, BFM_INPUT_MOUSELEFT, state))
				break;
			Int32 mxg = state.GetInt32(BFM_INPUT_X);
			Int32 myg = state.GetInt32(BFM_INPUT_Y);
			Global2Local(&mxg, &myg);

			// ---- hover targets
			Int32 nClip = NOTOK, nBtn = NOTOK, nChip = NOTOK, nTrk = NOTOK;
			if (myg < TITLE_H)
			{
				if (mxg >= HEADER_W + 8 && mxg <= HEADER_W + 30)
					nBtn = 0;
				else if (mxg >= HEADER_W + 34 && mxg <= HEADER_W + 56)
					nBtn = 1;
				else if (mxg >= HEADER_W + 60 && mxg <= HEADER_W + 82)
					nBtn = 2;
				else
					for (Int32 i = 0; i < 4; ++i)
						if (mxg >= chipL[i] && mxg <= chipR[i] && chipR[i] > chipL[i])
							nChip = i;
			}
			else if (mxg < HEADER_W)
			{
				nTrk = TrackAtY(myg, (Int32)m->tracks.size());
			}
			else if (myg >= TopOffset())
			{
				const ClipHit hh = HitClip(*m, mxg, myg);
				nClip = hh.clipId;
			}
			if (nClip != hoverClipId || nBtn != hoverBtn || nChip != hoverChip || nTrk != hoverTrack)
			{
				hoverClipId = nClip;
				hoverBtn = nBtn;
				hoverChip = nChip;
				hoverTrack = nTrk;
				Redraw();
			}

			Int32 cursor = MOUSE_NORMAL;
			if (myg >= TITLE_H && myg < TITLE_H + SCROLL_H && mxg >= HEADER_W)
			{
				cursor = MOUSE_ARROW_H;
			}
			else if (mxg < HEADER_W && myg >= TopOffset())
			{
				cursor = MOUSE_POINT_HAND;
				// row boundary = vertical resize cursor
				const Int32 nTracks = (Int32)m->tracks.size();
				for (Int32 t = 0; t < nTracks; ++t)
				{
					const Int32 boundary = TrackTop(t) + trackH - 2;
					if (myg >= boundary - 3 && myg <= boundary + 3)
					{
						cursor = MOUSE_ARROW_V;
						break;
					}
				}
			}
			else if (myg >= TITLE_H + SCROLL_H && myg < TopOffset() && mxg >= HEADER_W)
			{
				cursor = MOUSE_ARROW_H; // ruler: scrub / markers / IN-OUT
			}
			else if (mxg >= HEADER_W)
			{
				const ClipHit ch = HitClip(*m, mxg, myg);
				if (ch.clipId != NOTOK)
					cursor = (ch.zone == 1 || ch.zone == 2 || ch.zone == 3) ? MOUSE_ARROW_H : MOUSE_MOVE;
			}
			result.SetInt32(RESULT_CURSOR, cursor);
			return true;
		}
		case BFM_DRAGRECEIVE:
		{
			BaseDocument* doc = GetActiveDocument();
			TCModel* m = doc ? TCGetModel(doc) : nullptr;
			if (!doc || !m)
				break;

			if (msg.GetInt32(BFM_DRAG_LOST))
			{
				if (dragHover)
				{
					dragHover = false;
					Redraw();
				}
				break;
			}

			Int32 type = 0;
			void* object = nullptr;
			if (!GetDragObject(msg, &type, &object) || type != DRAGTYPE_ATOMARRAY || !object)
				break;
			AtomArray* arr = static_cast<AtomArray*>(object);
			if (arr->GetCount() < 1)
				break;
			BaseList2D* node = static_cast<BaseList2D*>(arr->GetIndex(0));
			if (!node)
				break;
			const String takeName = node->GetName();

			Int32 px = 0, py = 0;
			GetDragPosition(msg, &px, &py);

			const Int32 trackCount = (Int32)m->tracks.size();
			Int32 track = TrackAtY(py, trackCount);
			// allow dropping onto the empty area below the tracks -> last track
			if (track == NOTOK && py >= TopOffset() && px >= HEADER_W)
				track = trackCount - 1;

			const Bool inside = (px >= HEADER_W && track != NOTOK && !m->tracks[track].locked);
			const Int32 frame = SnapFrame(*m, doc, XToFrame(px), NOTOK);

			if (msg.GetInt32(BFM_DRAG_FINISHED))
			{
				dragHover = false;
				if (inside)
				{
					TCAddUndo(doc);
					TCClip c;
					c.id = m->nextId++;
					c.track = track;
					c.start = frame;
					c.end = frame + doc->GetFps();
					c.takeName = takeName;
					c.name = takeName;
					c.color = TCPaletteColor(c.id % TC_PALETTE_COUNT);
					m->clips.push_back(c);
					selId = c.id;
					curTrack = track;
					Edited(doc);
				}
				else
				{
					Redraw();
				}
			}
			else
			{
				dragHover = inside;
				dragTrack = track;
				dragFrame = frame;
				dragName = takeName;
				SetDragDestination(inside ? MOUSE_INSERTCOPY : MOUSE_FORBIDDEN);
				Redraw();
			}
			return true;
		}
		default:
			break;
	}
	return GeUserArea::Message(msg, result);
}

// ------------------------------------------------------------------ drags
//
// All drags poll ABSOLUTE mouse positions via GetInputState (the same pattern
// as the asynctest SDK example). GeUserArea::MouseDrag delta values are
// inverted and caused mirrored interactions.

Bool TCTimelineArea::PollDrag(Int32 channel, Int32& x, Int32& y)
{
	BaseContainer state;
	if (!GetInputState(BFM_INPUT_MOUSE, channel, state))
		return false;
	if (state.GetInt32(BFM_INPUT_VALUE) == 0)
		return false;
	Int32 gx = state.GetInt32(BFM_INPUT_X);
	Int32 gy = state.GetInt32(BFM_INPUT_Y);
	Global2Local(&gx, &gy);
	x = gx;
	y = gy;
	return true;
}

void TCTimelineArea::DragScrub(BaseDocument* doc, Int32 mxStart, Int32 myStart)
{
	const Int32 fps = doc->GetFps();
	Int32 dmin = doc->GetMinTime().GetFrame(fps);
	Int32 dmax = doc->GetMaxTime().GetFrame(fps);
	TCModel* m = TCGetModel(doc);
	if (m)
	{
		dmin = (m->SequenceMin() < dmin) ? m->SequenceMin() : dmin;
		dmax = (m->SequenceMax() > dmax) ? m->SequenceMax() : dmax;
	}
	Int32 last = -2147483647;
	Int32 x = mxStart, y = myStart;
	do
	{
		Int32 f = RoundToInt(XToFrame(x));
		f = ClampInt(f, dmin, dmax);
		if (f != last)
		{
			last = f;
			TCSetDocFrame(doc, f);
			Redraw();
		}
	} while (PollDrag(BFM_INPUT_MOUSELEFT, x, y));
}

void TCTimelineArea::DragClip(BaseDocument* doc, TCModel& m, const ClipHit& hit, Int32 mx, Int32 my, Int32 qualifier)
{
	TCClip* c = m.FindClip(hit.clipId);
	if (!c)
		return;
	if (c->track >= 0 && c->track < (Int32)m.tracks.size() && m.tracks[c->track].locked)
		return; // locked: selection only

	TCAddUndo(doc);

	// the grab set: all selected clips on unlocked tracks (trim = grabbed only)
	struct Grab
	{
		Int32	 id;
		Int32	 start, end, track;
		String take;
		Bool	 camFollow;
	};
	std::vector<Grab> grabs;
	const Bool duplicated = (qualifier & QCTRL) != 0;

	std::vector<Int32> sourceIds = (hit.zone == 0) ? selIds : std::vector<Int32>{ hit.clipId };
	if (duplicated)
	{
		std::vector<Int32> copies;
		for (Int32 sid : sourceIds)
		{
			const TCClip* sc = m.FindClip(sid);
			if (!sc)
				continue;
			TCClip copy = *sc;
			copy.id = m.nextId++;
			m.clips.push_back(copy);
			copies.push_back(copy.id);
		}
		selIds = copies;
		selId = copies.empty() ? NOTOK : copies.back();
		sourceIds = copies;
	}
	Int32 grabbedId = duplicated ? (sourceIds.empty() ? NOTOK : sourceIds.back()) : hit.clipId;
	for (Int32 sid : sourceIds)
	{
		const TCClip* sc = m.FindClip(sid);
		if (!sc)
			continue;
		if (sc->track >= 0 && sc->track < (Int32)m.tracks.size() && m.tracks[sc->track].locked)
			continue;
		grabs.push_back({ sc->id, sc->start, sc->end, sc->track, sc->takeName, sc->camFollow });
	}
	if (grabs.empty() || grabbedId == NOTOK)
		return;

	// reference clip for snapping = the one actually grabbed
	Grab ref = grabs[0];
	for (const auto& g : grabs)
		if (g.id == grabbedId)
			ref = g;

	Bool moved = false;
	Int32 appliedDelta = 0;
	Int32 x = mx, y = my;
	Int32 lastX = mx, lastY = my;

	while (PollDrag(BFM_INPUT_MOUSELEFT, x, y))
	{
		if (x == lastX && y == lastY)
			continue;
		lastX = x;
		lastY = y;
		const Float deltaF = (Float)(x - mx) / ppf;

		if (hit.zone == 1 || hit.zone == 2)
		{
			TCClip* cc = m.FindClip(grabbedId);
			if (!cc)
				break;
			if (hit.zone == 1)
			{
				Int32 ns = SnapFrame(m, doc, (Float)ref.start + deltaF, grabbedId);
				if (ns > cc->end - 1)
					ns = cc->end - 1;
				cc->start = ns;
			}
			else
			{
				Int32 ne = SnapFrame(m, doc, (Float)ref.end + deltaF, grabbedId);
				if (ne < cc->start + 1)
					ne = cc->start + 1;
				cc->end = ne;
			}
		}
		else
		{
			// group move: snap the grabbed clip, apply the same delta to all
			const Int32 len = ref.end - ref.start;
			const Int32 ns = SnapFrame(m, doc, (Float)ref.start + deltaF, grabbedId);
			const Int32 ne = SnapFrame(m, doc, (Float)ref.end + deltaF, grabbedId);
			Int32 newStart;
			if (AbsF(((Float)ref.start + deltaF) - (Float)ns) <= AbsF(((Float)ref.end + deltaF) - (Float)ne))
				newStart = ns;
			else
				newStart = ne - len;
			appliedDelta = newStart - ref.start;

			Int32 trackDelta = 0;
			const Int32 nt = TrackAtY(y, (Int32)m.tracks.size());
			if (nt != NOTOK)
				trackDelta = nt - ref.track;

			for (const auto& g : grabs)
			{
				TCClip* cc = m.FindClip(g.id);
				if (!cc)
					continue;
				cc->start = g.start + appliedDelta;
				cc->end = g.end + appliedDelta;
				const Int32 t = ClampInt(g.track + trackDelta, 0, (Int32)m.tracks.size() - 1);
				if (!m.tracks[t].locked)
					cc->track = t;
			}
		}
		moved = true;
		editCounter++;
		TCSwitchTakeForTime(doc);
		Redraw();
	}

	TCClip* cc = m.FindClip(grabbedId);
	if (cc)
		curTrack = cc->track;
	if (moved && !duplicated && hit.zone == 0 && appliedDelta != 0)
	{
		for (const auto& g : grabs)
			if (g.camFollow)
				TCShiftCameraKeys(doc, g.take, g.start, g.end, appliedDelta);
	}
	Edited(doc);
}

void TCTimelineArea::DragMarquee(BaseDocument* doc, TCModel& m, Int32 mx, Int32 my)
{
	marqueeOn = false;
	Bool dragged = false;
	Int32 x = mx, y = my;
	while (PollDrag(BFM_INPUT_MOUSELEFT, x, y))
	{
		if (!dragged && (AbsF((Float)(x - mx)) + AbsF((Float)(y - my))) > 4.0)
			dragged = true;
		if (!dragged)
			continue;
		marqueeOn = true;
		mqX1 = (mx < x) ? mx : x;
		mqX2 = (mx < x) ? x : mx;
		mqY1 = (my < y) ? my : y;
		mqY2 = (my < y) ? y : my;

		// live selection of intersecting clips
		selIds.clear();
		for (const auto& c : m.clips)
		{
			if (c.track < 0 || c.track >= (Int32)m.tracks.size())
				continue;
			const Int32 cx1 = FrameToX((Float)c.start);
			const Int32 cx2 = FrameToX((Float)c.end);
			const Int32 ty1 = TrackTop(c.track);
			const Int32 ty2 = ty1 + trackH - 2;
			if (cx2 >= mqX1 && cx1 <= mqX2 && ty2 >= mqY1 && ty1 <= mqY2)
				selIds.push_back(c.id);
		}
		selId = selIds.empty() ? NOTOK : selIds.back();
		Redraw();
	}
	if (marqueeOn)
	{
		marqueeOn = false;
		Redraw();
	}
	else if (!dragged)
	{
		ClearSelection(); // plain click on empty space
		Redraw();
	}
}

void TCTimelineArea::DragMarker(BaseDocument* doc, TCModel& m, Int32 markerIndex, Int32 mx, Int32 my)
{
	if (markerIndex < 0 || markerIndex >= (Int32)m.markers.size())
		return;
	TCAddUndo(doc);
	const Int32 orig = m.markers[markerIndex].frame;
	Int32 x = mx, y = my, lastX = mx;
	while (PollDrag(BFM_INPUT_MOUSELEFT, x, y))
	{
		if (x == lastX)
			continue;
		lastX = x;
		m.markers[markerIndex].frame = RoundToInt((Float)orig + (Float)(x - mx) / ppf);
		editCounter++;
		Redraw();
	}
	Edited(doc, false);
}

void TCTimelineArea::DragInOut(BaseDocument* doc, TCModel& m, Bool isIn, Int32 mx, Int32 my)
{
	TCAddUndo(doc);
	const Int32 fps = doc->GetFps();
	const Int32 orig = isIn ? m.inFrame : m.outFrame;
	dragRange = isIn ? 1 : 2;
	Int32 x = mx, y = my, lastX = mx;
	while (PollDrag(BFM_INPUT_MOUSELEFT, x, y))
	{
		if (x == lastX)
			continue;
		lastX = x;
		const Int32 f = RoundToInt((Float)orig + (Float)(x - mx) / ppf);
		if (isIn)
			m.inFrame = (f > m.outFrame) ? m.outFrame : f;
		else
			m.outFrame = (f < m.inFrame) ? m.inFrame : f;
		// IN/OUT directly drive the document preview range for instant feedback
		doc->SetLoopMinTime(BaseTime(m.inFrame, fps));
		doc->SetLoopMaxTime(BaseTime(m.outFrame, fps));
		editCounter++;
		Redraw();
	}
	dragRange = 0;
	Edited(doc, false);
}

void TCTimelineArea::DragAudio(BaseDocument* doc, TCModel& m, Int32 mx, Int32 my)
{
	TCAddUndo(doc);
	const Int32 orig = m.audioOffset;
	Int32 x = mx, y = my, lastX = mx;
	while (PollDrag(BFM_INPUT_MOUSELEFT, x, y))
	{
		if (x == lastX)
			continue;
		lastX = x;
		m.audioOffset = RoundToInt((Float)orig + (Float)(x - mx) / ppf);
		editCounter++;
		Redraw();
	}
	TCSyncSoundTrack(doc); // keep the native sound track offset in sync
	Edited(doc, false);
}

void TCTimelineArea::DragCamLane(BaseDocument* doc, TCModel& m, Int32 clipId, Int32 mx, Int32 my)
{
	TCClip* c = m.FindClip(clipId);
	if (!c)
		return;
	std::vector<Int32> keys;
	TCCameraKeyFrames(doc, c->takeName, keys);
	if (keys.empty())
		return; // no camera keys to offset

	dragCamClipId = clipId;
	dragCamOffset = 0;
	Int32 x = mx, y = my, lastX = mx;
	while (PollDrag(BFM_INPUT_MOUSELEFT, x, y))
	{
		if (x == lastX)
			continue;
		lastX = x;
		dragCamOffset = RoundToInt((Float)(x - mx) / ppf);
		Redraw();
	}
	const Int32 delta = dragCamOffset;
	const String takeName = c->takeName;
	dragCamClipId = NOTOK;
	dragCamOffset = 0;
	if (delta != 0)
	{
		// the lane shifts the camera timing as a whole (all keys of the take camera)
		TCShiftCameraKeys(doc, takeName, -100000000, 100000000, delta);
	}
	Edited(doc, false);
}

void TCTimelineArea::DragPan(Int32 mx, Int32 my, Int32 button)
{
	Int32 x = mx, y = my, lastX = mx;
	while (PollDrag(button, x, y))
	{
		if (x == lastX)
			continue;
		viewStart -= (Float)(x - lastX) / ppf;
		lastX = x;
		Redraw();
	}
}

Bool TCTimelineArea::DragZoom(Int32 mx, Int32 my)
{
	const Float anchor = XToFrame(mx);
	Bool zoomed = false;
	Int32 x = mx, y = my, lastX = mx;
	while (PollDrag(BFM_INPUT_MOUSERIGHT, x, y))
	{
		if (x == lastX)
			continue;
		if (!zoomed && (x - mx > 3 || mx - x > 3))
			zoomed = true;
		if (zoomed)
		{
			ppf *= maxon::Pow(1.01, (Float)(x - lastX));
			if (ppf < 0.05)
				ppf = 0.05;
			if (ppf > 200.0)
				ppf = 200.0;
			viewStart = anchor - (Float)(mx - HEADER_W) / ppf;
			Redraw();
		}
		lastX = x;
	}
	return zoomed;
}

void TCTimelineArea::DragScrollbar(BaseDocument* doc, TCModel& m, Int32 mx, Int32 my)
{
	Float ea = 0, eb = 100;
	ContentExtents(doc, m, ea, eb);
	const Int32 w = GetWidth();
	const Float span = eb - ea;
	const Float viewLen = (Float)(w - HEADER_W) / ppf;

	auto applyCenter = [&](Int32 px) {
		Float t = (Float)(px - HEADER_W) / (Float)(w - HEADER_W);
		t = t < 0 ? 0 : (t > 1 ? 1 : t);
		viewStart = ea + t * span - viewLen * 0.5;
		Redraw();
	};

	Int32 x = mx, y = my, lastX = -10000;
	do
	{
		if (x != lastX)
		{
			lastX = x;
			applyCenter(x);
		}
	} while (PollDrag(BFM_INPUT_MOUSELEFT, x, y));
}

// ------------------------------------------------------------------ operations

void TCTimelineArea::AddClipForTake(const String& takeName)
{
	BaseDocument* doc = GetActiveDocument();
	TCModel* m = doc ? TCGetModel(doc) : nullptr;
	if (!m || !takeName.IsPopulated())
		return;
	TCAddUndo(doc);
	const Int32 f = TCCurrentFrame(doc);
	TCClip c;
	c.id = m->nextId++;
	c.track = ClampInt(curTrack, 0, (Int32)m->tracks.size() - 1);
	c.start = f;
	c.end = f + doc->GetFps(); // one second by default
	c.takeName = takeName;
	c.name = takeName;
	c.color = TCPaletteColor(c.id % TC_PALETTE_COUNT);
	m->clips.push_back(c);
	SelectOnly(c.id);
	Edited(doc);
}

void TCTimelineArea::AddClipFromCurrentTake()
{
	BaseDocument* doc = GetActiveDocument();
	if (!doc)
		return;
	TakeData* td = doc->GetTakeData();
	if (!td || !td->GetCurrentTake())
		return;
	AddClipForTake(td->GetCurrentTake()->GetName());
}

void TCTimelineArea::SplitAtPlayhead()
{
	BaseDocument* doc = GetActiveDocument();
	TCModel* m = doc ? TCGetModel(doc) : nullptr;
	if (!m)
		return;
	const Int32 f = TCCurrentFrame(doc);

	TCClip* target = nullptr;
	if (selId != NOTOK)
	{
		TCClip* c = m->FindClip(selId);
		if (c && f > c->start && f < c->end)
			target = c;
	}
	if (!target)
	{
		const TCClip* c = m->ClipAt(f);
		if (c && f > c->start && f < c->end)
			target = m->FindClip(c->id);
	}
	if (!target)
		return;

	TCAddUndo(doc);
	TCClip right = *target;
	right.id = m->nextId++;
	right.start = f;
	target->end = f;
	m->clips.push_back(right);
	SelectOnly(right.id);
	Edited(doc);
}

void TCTimelineArea::DuplicateSelected()
{
	BaseDocument* doc = GetActiveDocument();
	TCModel* m = doc ? TCGetModel(doc) : nullptr;
	if (!m || selIds.empty())
		return;
	TCAddUndo(doc);
	std::vector<Int32> copies;
	for (Int32 sid : selIds)
	{
		const TCClip* c = m->FindClip(sid);
		if (!c)
			continue;
		TCClip copy = *c;
		copy.id = m->nextId++;
		const Int32 len = copy.Length();
		copy.start = c->end;
		copy.end = copy.start + len;
		m->clips.push_back(copy);
		copies.push_back(copy.id);
	}
	selIds = copies;
	selId = copies.empty() ? NOTOK : copies.back();
	Edited(doc);
}

void TCTimelineArea::DeleteSelected()
{
	BaseDocument* doc = GetActiveDocument();
	TCModel* m = doc ? TCGetModel(doc) : nullptr;
	if (!m || selIds.empty())
		return;
	TCAddUndo(doc);
	for (Int32 sid : selIds)
	{
		for (size_t i = 0; i < m->clips.size(); ++i)
		{
			if (m->clips[i].id == sid)
			{
				m->clips.erase(m->clips.begin() + i);
				break;
			}
		}
		if (m->focusClipId == sid)
			m->focusClipId = NOTOK;
	}
	ClearSelection();
	Edited(doc);
}

void TCTimelineArea::RippleDeleteSelected()
{
	BaseDocument* doc = GetActiveDocument();
	TCModel* m = doc ? TCGetModel(doc) : nullptr;
	if (!m || selIds.empty())
		return;

	// gap = union extent of the selected clips; everything later shifts left
	Int32 gapStart = 0x7FFFFFFF, gapEnd = -0x7FFFFFFF;
	for (Int32 sid : selIds)
	{
		const TCClip* c = m->FindClip(sid);
		if (!c)
			continue;
		gapStart = (c->start < gapStart) ? c->start : gapStart;
		gapEnd = (c->end > gapEnd) ? c->end : gapEnd;
	}
	if (gapEnd <= gapStart)
		return;
	const Int32 gap = gapEnd - gapStart;

	TCAddUndo(doc);
	for (Int32 sid : selIds)
	{
		for (size_t i = 0; i < m->clips.size(); ++i)
		{
			if (m->clips[i].id == sid)
			{
				m->clips.erase(m->clips.begin() + i);
				break;
			}
		}
		if (m->focusClipId == sid)
			m->focusClipId = NOTOK;
	}
	for (auto& c : m->clips)
	{
		if (c.start >= gapEnd)
		{
			c.start -= gap;
			c.end -= gap;
		}
	}
	for (auto& mk : m->markers)
		if (mk.frame >= gapEnd)
			mk.frame -= gap;
	ClearSelection();
	Edited(doc);
}

void TCTimelineArea::CopySelected()
{
	BaseDocument* doc = GetActiveDocument();
	TCModel* m = doc ? TCGetModel(doc) : nullptr;
	if (!m || selIds.empty())
		return;
	g_clipboard.clear();
	for (Int32 sid : selIds)
	{
		const TCClip* c = m->FindClip(sid);
		if (c)
			g_clipboard.push_back(*c);
	}
}

void TCTimelineArea::PasteAtPlayhead()
{
	BaseDocument* doc = GetActiveDocument();
	TCModel* m = doc ? TCGetModel(doc) : nullptr;
	if (!m || g_clipboard.empty())
		return;
	Int32 minStart = 0x7FFFFFFF;
	for (const auto& c : g_clipboard)
		minStart = (c.start < minStart) ? c.start : minStart;
	const Int32 offset = TCCurrentFrame(doc) - minStart;

	TCAddUndo(doc);
	std::vector<Int32> pasted;
	for (const auto& src : g_clipboard)
	{
		TCClip c = src;
		c.id = m->nextId++;
		c.start += offset;
		c.end += offset;
		c.track = ClampInt(c.track, 0, (Int32)m->tracks.size() - 1);
		m->clips.push_back(c);
		pasted.push_back(c.id);
	}
	selIds = pasted;
	selId = pasted.empty() ? NOTOK : pasted.back();
	Edited(doc);
}

void TCTimelineArea::ToggleFocusSelected()
{
	BaseDocument* doc = GetActiveDocument();
	TCModel* m = doc ? TCGetModel(doc) : nullptr;
	if (!m)
		return;
	TCAddUndo(doc);
	if (m->focusClipId == selId || selId == NOTOK)
	{
		m->focusClipId = NOTOK;
	}
	else
	{
		m->focusClipId = selId;
		const TCClip* c = m->FindClip(selId);
		if (c)
		{
			const Int32 fps = doc->GetFps();
			doc->SetLoopMinTime(BaseTime(c->start, fps));
			doc->SetLoopMaxTime(BaseTime(c->end - 1, fps));
		}
	}
	Edited(doc);
}

void TCTimelineArea::AddMarkerAtPlayhead()
{
	BaseDocument* doc = GetActiveDocument();
	TCModel* m = doc ? TCGetModel(doc) : nullptr;
	if (!m)
		return;
	const Int32 frame = TCCurrentFrame(doc);
	for (const auto& mk : m->markers)
		if (mk.frame == frame)
			return; // one marker per frame
	TCAddUndo(doc);
	TCMarker mk;
	mk.frame = frame;
	mk.label = String("M") + IStr((Int32)m->markers.size() + 1);
	m->markers.push_back(mk);
	Edited(doc, false);
}

void TCTimelineArea::SetInAtPlayhead()
{
	BaseDocument* doc = GetActiveDocument();
	TCModel* m = doc ? TCGetModel(doc) : nullptr;
	if (!m)
		return;
	TCAddUndo(doc);
	m->inFrame = TCCurrentFrame(doc);
	if (m->outFrame < m->inFrame)
		m->outFrame = m->inFrame;
	Edited(doc, false);
}

void TCTimelineArea::SetOutAtPlayhead()
{
	BaseDocument* doc = GetActiveDocument();
	TCModel* m = doc ? TCGetModel(doc) : nullptr;
	if (!m)
		return;
	TCAddUndo(doc);
	m->outFrame = TCCurrentFrame(doc);
	if (m->inFrame > m->outFrame)
		m->inFrame = m->outFrame;
	Edited(doc, false);
}

void TCTimelineArea::NudgeSelected(Int32 delta)
{
	BaseDocument* doc = GetActiveDocument();
	TCModel* m = doc ? TCGetModel(doc) : nullptr;
	if (!m || selIds.empty() || delta == 0)
		return;
	TCAddUndo(doc);
	for (Int32 sid : selIds)
	{
		TCClip* c = m->FindClip(sid);
		if (!c)
			continue;
		if (c->track >= 0 && c->track < (Int32)m->tracks.size() && m->tracks[c->track].locked)
			continue;
		const Int32 oldStart = c->start, oldEnd = c->end;
		c->start += delta;
		c->end += delta;
		if (c->camFollow)
			TCShiftCameraKeys(doc, c->takeName, oldStart, oldEnd, delta);
	}
	Edited(doc);
}

void TCTimelineArea::AddTrack()
{
	BaseDocument* doc = GetActiveDocument();
	TCModel* m = doc ? TCGetModel(doc) : nullptr;
	if (!m)
		return;
	TCAddUndo(doc);
	TCTrack t;
	t.name = String("T") + IStr((Int32)m->tracks.size() + 1);
	t.color = TCPaletteColor((Int32)m->tracks.size() % TC_PALETTE_COUNT);
	m->tracks.push_back(t);
	Edited(doc, false);
}

void TCTimelineArea::DeleteCurrentTrack()
{
	BaseDocument* doc = GetActiveDocument();
	TCModel* m = doc ? TCGetModel(doc) : nullptr;
	if (!m || (Int32)m->tracks.size() <= 1)
		return;
	if (!QuestionDialog(String("Delete track '") + m->tracks[curTrack].name + String("' and all its clips?")))
		return;
	TCAddUndo(doc);
	const Int32 t = curTrack;
	for (size_t i = m->clips.size(); i > 0; --i)
	{
		TCClip& c = m->clips[i - 1];
		if (c.track == t)
		{
			if (m->focusClipId == c.id)
				m->focusClipId = NOTOK;
			if (selId == c.id)
				selId = NOTOK;
			m->clips.erase(m->clips.begin() + (i - 1));
		}
		else if (c.track > t)
		{
			c.track--;
		}
	}
	m->tracks.erase(m->tracks.begin() + t);
	if (curTrack >= (Int32)m->tracks.size())
		curTrack = (Int32)m->tracks.size() - 1;
	// prune dangling ids from the multi-selection
	for (size_t i = selIds.size(); i > 0; --i)
		if (!m->FindClip(selIds[i - 1]))
			selIds.erase(selIds.begin() + (i - 1));
	if (selId != NOTOK && !m->FindClip(selId))
		selId = selIds.empty() ? NOTOK : selIds.back();
	Edited(doc);
}

// ------------------------------------------------------------------ context menu

void TCTimelineArea::ContextMenu(BaseDocument* doc, TCModel& m, Int32 mx, Int32 my, Int32 screenX, Int32 screenY)
{
	// ---- audio track: its own context menu
	const Int32 audioTop0 = TopOffset() + (Int32)m.tracks.size() * trackH;
	if (my >= audioTop0 && my < audioTop0 + AUDIO_H && mx >= HEADER_W)
	{
		BaseContainer am;
		am.InsData(POP_AUDIO_LOAD, String("Load Audio (WAV)..."));
		if (m.audioPath.IsPopulated())
		{
			am.InsData(POP_AUDIO_CLEAR, String("Clear Audio"));
			am.InsData(0, String(""));
			am.InsData(POP_AUDIO_RESET, String("Offset -> 0"));
			am.InsData(POP_AUDIO_TOPLAYHEAD, String("Offset -> Playhead"));
			am.InsData(POP_AUDIO_SYNC, String("Re-Sync Sound Track"));
		}
		const Int32 ares = ShowPopupMenu(nullptr, MOUSEPOS, MOUSEPOS, am);
		switch (ares)
		{
			case POP_AUDIO_LOAD:
			{
				Filename fn;
				if (fn.FileSelect(FILESELECTTYPE::ANYTHING, FILESELECT::LOAD, String("Load reference audio (PCM WAV)")))
				{
					TCAddUndo(doc);
					m.audioPath = fn.GetString();
					if (!TCLoadAudioPeaks(m))
						MessageDialog(String("Could not read this file as PCM WAV (16/8 bit)."));
					TCSyncSoundTrack(doc);
					Edited(doc, false);
				}
				break;
			}
			case POP_AUDIO_CLEAR:
				TCAddUndo(doc);
				m.audioPath = String();
				m.audioPeaks.clear();
				m.audioSeconds = 0.0;
				TCSyncSoundTrack(doc);
				Edited(doc, false);
				break;
			case POP_AUDIO_RESET:
				TCAddUndo(doc);
				m.audioOffset = 0;
				TCSyncSoundTrack(doc);
				Edited(doc, false);
				break;
			case POP_AUDIO_TOPLAYHEAD:
				TCAddUndo(doc);
				m.audioOffset = TCCurrentFrame(doc);
				TCSyncSoundTrack(doc);
				Edited(doc, false);
				break;
			case POP_AUDIO_SYNC:
				TCSyncSoundTrack(doc);
				break;
			default:
				break;
		}
		return;
	}

	const ClipHit hit = HitClip(m, mx, my);
	const Int32 mkIdx = HitMarker(m, mx, my);
	std::vector<std::pair<BaseTake*, Int32>> takes;
	TCCollectTakes(doc, takes);

	BaseContainer menu;
	if (hit.clipId != NOTOK)
	{
		if (!IsSelected(hit.clipId))
			SelectOnly(hit.clipId);
		else
			selId = hit.clipId;
		Redraw();
		TCClip* c = m.FindClip(hit.clipId);
		if (!c)
			return;
		menu.InsData(POP_CLIPSETTINGS, String("Settings..."));
		menu.InsData(0, String(""));
		menu.InsData(POP_RENAME, String("Rename..."));
		menu.InsData(POP_NOTE, String("Edit Note..."));
		menu.InsData(POP_COPY, String("Copy (Ctrl+C)"));
		menu.InsData(POP_GOTOTAKE, String("Set As Current Take"));
		menu.InsData(POP_CAMFOLLOW, String(c->camFollow ? "Camera Follow: ON" : "Camera Follow: OFF"));
		menu.InsData(0, String("")); // separator
		BaseContainer st;
		st.InsData(1, String("Status"));
		for (Int32 i = 0; i < TC_STATUS_COUNT; ++i)
			st.InsData(POP_STATUS_BASE + i, String(TCStatusNames()[i]) + ((c->status == i) ? String("  (x)") : String("")));
		menu.InsData(0, st);
		BaseContainer cm;
		cm.InsData(1, String("Color"));
		for (Int32 i = 0; i < TC_PALETTE_COUNT; ++i)
			cm.InsData(POP_COLOR_BASE + i, String("Color ") + IStr(i + 1));
		menu.InsData(0, cm);
		menu.InsData(0, String(""));
		menu.InsData(POP_SPLIT, String("Split At Playhead"));
		menu.InsData(POP_DUP, String("Duplicate"));
		menu.InsData(POP_DELETE, String("Delete"));
		menu.InsData(POP_RIPPLEDEL, String("Ripple Delete (close gap)"));
		menu.InsData(POP_FOCUS, String((m.focusClipId == c->id) ? "Unfocus" : "Focus"));
	}
	else if (mkIdx != NOTOK)
	{
		menu.InsData(POP_RENMARKER, String("Rename Marker..."));
		menu.InsData(POP_DELMARKER, String("Delete Marker"));
	}
	else
	{
		const Int32 t = TrackAtY(my, (Int32)m.tracks.size());
		if (t != NOTOK)
			curTrack = t;
		BaseContainer add;
		add.InsData(1, String("Add Clip For Take"));
		for (Int32 i = 0; i < (Int32)takes.size(); ++i)
		{
			String label;
			for (Int32 d = 0; d < takes[i].second; ++d)
				label += String("   ");
			label += takes[i].first->GetName();
			add.InsData(POP_ADDCLIP_BASE + i, label);
		}
		menu.InsData(0, add);
		menu.InsData(POP_ADDMARKER, String("Add Marker"));
		if (!g_clipboard.empty())
			menu.InsData(POP_PASTE, String("Paste At Playhead (Ctrl+V)"));
		menu.InsData(0, String(""));
		menu.InsData(POP_ADDTRACK, String("Add Track"));
		menu.InsData(POP_RENTRACK, String("Rename Track..."));
		menu.InsData(POP_DELTRACK, String("Delete Track"));
		menu.InsData(0, String(""));
		menu.InsData(POP_CAMWIZARD, String("Create Takes From Cameras"));
		menu.InsData(POP_EXPORTCSV, String("Export Shot List (CSV)..."));
		menu.InsData(0, String(""));
		menu.InsData(POP_FIT, String("Fit View"));
	}

	const Int32 res = ShowPopupMenu(nullptr, MOUSEPOS, MOUSEPOS, menu);
	if (res <= 0)
		return;

	TCClip* c = (selId != NOTOK) ? m.FindClip(selId) : nullptr;

	if (res == POP_CLIPSETTINGS && c)
	{
		if (TCOpenSettings(c->id, c->takeName))
			Edited(doc);
	}
	else if (res == POP_RENAME && c)
	{
		String name = c->DisplayName();
		if (RenameDialog(&name))
		{
			TCAddUndo(doc);
			c->name = name;
			Edited(doc, false);
		}
	}
	else if (res == POP_NOTE && c)
	{
		String note = c->note;
		if (RenameDialog(&note))
		{
			TCAddUndo(doc);
			c->note = note;
			Edited(doc, false);
		}
	}
	else if (res == POP_GOTOTAKE && c)
	{
		TakeData* td = doc->GetTakeData();
		BaseTake* take = TCFindTakeByName(doc, c->takeName);
		if (td && take)
		{
			td->SetCurrentTake(take);
			EventAdd();
		}
	}
	else if (res == POP_CAMFOLLOW && c)
	{
		TCAddUndo(doc);
		c->camFollow = !c->camFollow;
		Edited(doc, false);
	}
	else if (res >= POP_STATUS_BASE && res < POP_STATUS_BASE + TC_STATUS_COUNT && c)
	{
		TCAddUndo(doc);
		c->status = res - POP_STATUS_BASE;
		Edited(doc, false);
	}
	else if (res >= POP_COLOR_BASE && res < POP_COLOR_BASE + TC_PALETTE_COUNT && c)
	{
		TCAddUndo(doc);
		c->color = TCPaletteColor(res - POP_COLOR_BASE);
		Edited(doc, false);
	}
	else if (res == POP_SPLIT)
	{
		SplitAtPlayhead();
	}
	else if (res == POP_DUP)
	{
		DuplicateSelected();
	}
	else if (res == POP_DELETE)
	{
		DeleteSelected();
	}
	else if (res == POP_RIPPLEDEL)
	{
		RippleDeleteSelected();
	}
	else if (res == POP_COPY)
	{
		CopySelected();
	}
	else if (res == POP_PASTE)
	{
		PasteAtPlayhead();
	}
	else if (res == POP_CAMWIZARD)
	{
		const Int32 n = TCCreateTakesFromCameras(doc, RoundToInt(XToFrame(mx)), curTrack);
		if (n > 0)
			Edited(doc);
		MessageDialog(IStr(n) + String(" take/clip pair(s) created from scene cameras."));
	}
	else if (res == POP_EXPORTCSV)
	{
		Filename fn;
		fn.SetFile(Filename(String("shotlist.csv")));
		if (fn.FileSelect(FILESELECTTYPE::ANYTHING, FILESELECT::SAVE, String("Export shot list (CSV)")))
		{
			if (!fn.CheckSuffix(String("csv")))
				fn.SetSuffix(String("csv"));
			if (TCExportCSV(doc, fn))
				MessageDialog(String("Shot list exported:\n") + fn.GetString());
			else
				MessageDialog(String("Export failed (could not write file)."));
		}
	}
	else if (res == POP_FOCUS)
	{
		ToggleFocusSelected();
	}
	else if (res == POP_RENMARKER && mkIdx != NOTOK)
	{
		String label = m.markers[mkIdx].label;
		if (RenameDialog(&label))
		{
			TCAddUndo(doc);
			m.markers[mkIdx].label = label;
			Edited(doc, false);
		}
	}
	else if (res == POP_DELMARKER && mkIdx != NOTOK)
	{
		TCAddUndo(doc);
		m.markers.erase(m.markers.begin() + mkIdx);
		Edited(doc, false);
	}
	else if (res == POP_ADDMARKER)
	{
		const Int32 frame = RoundToInt(XToFrame(mx));
		Bool exists = false;
		for (const auto& mk : m.markers)
			if (mk.frame == frame)
				exists = true;
		if (!exists)
		{
			TCAddUndo(doc);
			TCMarker mk;
			mk.frame = frame;
			mk.label = String("M") + IStr((Int32)m.markers.size() + 1);
			m.markers.push_back(mk);
			Edited(doc, false);
		}
	}
	else if (res == POP_ADDTRACK)
	{
		AddTrack();
	}
	else if (res == POP_RENTRACK)
	{
		String name = m.tracks[curTrack].name;
		if (RenameDialog(&name))
		{
			TCAddUndo(doc);
			m.tracks[curTrack].name = name;
			Edited(doc, false);
		}
	}
	else if (res == POP_DELTRACK)
	{
		DeleteCurrentTrack();
	}
	else if (res == POP_FIT)
	{
		FitView();
		Redraw();
	}
	else if (res >= POP_ADDCLIP_BASE && res < POP_ADDCLIP_BASE + (Int32)takes.size())
	{
		const Int32 ti = res - POP_ADDCLIP_BASE;
		TCAddUndo(doc);
		TCClip nc;
		nc.id = m.nextId++;
		nc.track = ClampInt(curTrack, 0, (Int32)m.tracks.size() - 1);
		nc.start = RoundToInt(XToFrame(mx));
		nc.end = nc.start + doc->GetFps();
		nc.takeName = takes[ti].first->GetName();
		nc.name = nc.takeName;
		nc.color = TCPaletteColor(nc.id % TC_PALETTE_COUNT);
		m.clips.push_back(nc);
		SelectOnly(nc.id);
		Edited(doc);
	}
}

// ------------------------------------------------------------------ keyboard

Bool TCTimelineArea::HandleKey(BaseDocument* doc, TCModel& m, const BaseContainer& msg)
{
	const Int32 chan = msg.GetInt32(BFM_INPUT_CHANNEL);
	const Int32 qual = msg.GetInt32(BFM_INPUT_QUALIFIER);

	switch (chan)
	{
		case KEY_SPACE:
			// transport toggle, like the native timeline
			if (CheckIsRunning(CHECKISRUNNING::ANIMATIONRUNNING))
				TCStopPlayback(doc);
			else
				CallCommand(CID_PLAY_FORWARD);
			return true;
		case KEY_DELETE:
		case KEY_BACKSPACE:
			if (qual & QSHIFT)
				RippleDeleteSelected(); // close the gap
			else
				DeleteSelected();
			return true;
		case KEY_LEFT:
			NudgeSelected((qual & QSHIFT) ? -10 : -1);
			return true;
		case KEY_RIGHT:
			NudgeSelected((qual & QSHIFT) ? 10 : 1);
			return true;
		case KEY_HOME:
			FitView();
			Redraw();
			return true;
		default:
			break;
	}

	// letter hotkeys must work regardless of the keyboard layout: compare the
	// raw channel key code AND the typed character in both EN and RU layouts
	const String asc = msg.GetString(BFM_INPUT_ASC).ToUpper();
	auto isKey = [&](Char en, const Char* enLow, const Char* ruUtf8) -> Bool {
		if (chan == (Int32)en || chan == (Int32)enLow[0])
			return true;
		const Char buf[2] = { en, 0 };
		return asc == String(buf) || asc == String(ruUtf8);
	};

	// clipboard / select-all (checked before the plain letters)
	if (qual & QCTRL)
	{
		if (isKey('C', "c", "С"))
		{
			CopySelected();
			return true;
		}
		if (isKey('V', "v", "М"))
		{
			PasteAtPlayhead();
			return true;
		}
		if (isKey('A', "a", "Ф"))
		{
			SelectAll();
			return true;
		}
		return false;
	}

	if (isKey('S', "s", "Ы"))
	{
		SplitAtPlayhead();
		return true;
	}
	if (isKey('D', "d", "В"))
	{
		DuplicateSelected();
		return true;
	}
	if (isKey('F', "f", "А"))
	{
		ToggleFocusSelected();
		return true;
	}
	if (isKey('M', "m", "Ь"))
	{
		AddMarkerAtPlayhead();
		return true;
	}
	if (isKey('I', "i", "Ш"))
	{
		SetInAtPlayhead();
		return true;
	}
	if (isKey('O', "o", "Щ"))
	{
		SetOutAtPlayhead();
		return true;
	}
	if (isKey('A', "a", "Ф"))
	{
		AddClipFromCurrentTake();
		return true;
	}
	if (isKey('H', "h", "Р"))
	{
		FitView();
		Redraw();
		return true;
	}
	return false;
}

} // namespace tc
