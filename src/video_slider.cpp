// Copyright (c) 2005, Rodrigo Braz Monteiro
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

/// @file video_slider.cpp
/// @brief Seek-bar control for video
/// @ingroup custom_control
///

#include "video_slider.h"

#include "async_video_provider.h"
#include "base_grid.h"
#include "command/command.h"
#include "include/aegisub/context.h"
#include "include/aegisub/hotkey.h"
#include "options.h"
#include "project.h"
#include "utils.h"
#include "video_controller.h"

#include <wx/dcbuffer.h>
#include <wx/settings.h>

VideoSlider::VideoSlider(wxWindow *parent, agi::Context *c)
: wxWindow(parent, -1, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS | wxFULL_REPAINT_ON_RESIZE)
, c(c)
, connections(agi::signal::make_vector({
	OPT_SUB("Video/Slider/Show Keyframes", [this] { Refresh(false); }),
	c->videoController->AddSeekListener(&VideoSlider::SetValue, this),
	c->project->AddVideoProviderListener(&VideoSlider::VideoOpened, this),
	c->project->AddKeyframesListener(&VideoSlider::KeyframesChanged, this),
}))
{
	// DPI-aware sizing
	SetClientSize(FromDIP(wxSize(20, 25)));
	SetMinSize(FromDIP(wxSize(20, 25)));
	SetBackgroundStyle(wxBG_STYLE_PAINT);

	c->videoSlider = this;
	VideoOpened(c->project->VideoProvider());
}

void VideoSlider::SetValue(int value) {
	if (val == value) return;
	value = mid(0, value, max);
	if (GetXAtValue(val) != GetXAtValue(value))
		Refresh(false);
	val = value;
}

void VideoSlider::VideoOpened(AsyncVideoProvider *provider) {
	if (provider) {
		max = provider->GetFrameCount() - 1;
		Refresh(false);
	}
}

void VideoSlider::KeyframesChanged(std::vector<int> const& newKeyframes) {
	keyframes = newKeyframes;
	Refresh(false);
}

int VideoSlider::GetValueAtX(int x) {
	int w = GetClientSize().GetWidth();
	int margin = FromDIP(5);

	// Special case: prevent division by zero or negative ranges if window is tiny
	if (w <= 2 * margin) return 0;

	return (int64_t)(x - margin) * (int64_t)max / (int64_t)(w - 2 * margin);
}

int VideoSlider::GetXAtValue(int value) {
	if (max <= 0) return 0;

	int w = GetClientSize().GetWidth();
	int margin = FromDIP(5);

	return (int64_t)value * (int64_t)(w - 2 * margin) / (int64_t)max + margin;
}

BEGIN_EVENT_TABLE(VideoSlider, wxWindow)
	EVT_MOUSE_EVENTS(VideoSlider::OnMouse)
	EVT_KEY_DOWN(VideoSlider::OnKeyDown)
	EVT_CHAR_HOOK(VideoSlider::OnCharHook)
	EVT_PAINT(VideoSlider::OnPaint)
	EVT_SET_FOCUS(VideoSlider::OnFocus)
	EVT_KILL_FOCUS(VideoSlider::OnFocus)
END_EVENT_TABLE()

void VideoSlider::OnMouse(wxMouseEvent &event) {
	bool had_focus = HasFocus();
	if (event.ButtonDown())
		SetFocus();

	if (event.LeftIsDown()) {
		int x = event.GetX();

		// If the slider didn't already have focus, don't seek if the user
		// clicked very close to the current location as they were probably
		// just trying to focus the slider
		if (!had_focus && abs(x - GetXAtValue(val)) < FromDIP(4))
			return;

		// Shift click to snap to keyframe
		if (event.ShiftDown() && keyframes.size()) {
			int clickedFrame = GetValueAtX(x);
			auto pos = lower_bound(keyframes.begin(), keyframes.end(), clickedFrame);
			if (pos == keyframes.end())
				--pos;
			else if (pos + 1 != keyframes.end() && clickedFrame - *pos > (*pos + 1) - clickedFrame)
				++pos;

			if (*pos == val) return;
			SetValue(*pos);
		}
		// Normal click
		else {
			int go = GetValueAtX(x);
			if (go == val) return;
			SetValue(go);
		}

		c->videoController->JumpToFrame(val);
	}
	else if (event.GetWheelRotation() != 0 && ForwardMouseWheelEvent(this, event)) {
		// If mouse is over the slider, use wheel to step by frames or keyframes (when Shift is held)
		if (event.ShiftDown())
			if (event.GetWheelRotation() < 0)
				cmd::call("video/frame/next/keyframe", c);
			else
				cmd::call("video/frame/prev/keyframe", c);
		else {
			SetValue(val + (event.GetWheelRotation() > 0 ? -1 : 1));
			c->videoController->JumpToFrame(val);
		}
	}
}

void VideoSlider::OnCharHook(wxKeyEvent &event) {
	hotkey::check("Video", c, event);
}

void VideoSlider::OnKeyDown(wxKeyEvent &event) {
	// Forward up/down/pgup/pgdn/home/end to grid as those aren't yet handled by commands
	switch (event.GetKeyCode()) {
		case WXK_UP:
		case WXK_DOWN:
		case WXK_PAGEUP:
		case WXK_PAGEDOWN:
		case WXK_HOME:
		case WXK_END:
			c->subsGrid->GetEventHandler()->ProcessEvent(event);
			break;
		default:
			event.Skip();
	}
}

void VideoSlider::OnPaint(wxPaintEvent &) {
	wxAutoBufferedPaintDC dc(this);
	int w, h;
	GetClientSize(&w, &h);

	// Colors
	wxColour shad = wxSystemSettings::GetColour(wxSYS_COLOUR_3DDKSHADOW);
	wxColour high = wxSystemSettings::GetColour(wxSYS_COLOUR_3DLIGHT);
	wxColour face = wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE);
	wxColour sel(123, 251, 232);
	wxColour notSel(sel.Red() * 2 / 5, sel.Green() * 2 / 5, sel.Blue() * 2 / 5);
	wxColour bord(0, 0, 0);

	// DPI Scaled metrics
	const int margin = FromDIP(5);
	const int track_y_margin = FromDIP(8);
	const int tick_start = FromDIP(2);
	const int tick_end = FromDIP(8);

	// Cursor/Thumb metrics
	const int t_w = FromDIP(4); // Thumb half width
	const int t_h_tip = FromDIP(2); // Triangle tip height
	const int t_h_body = FromDIP(5); // Body extension below track

	const int x1 = margin;
	const int x2 = w - margin;
	const int y1 = track_y_margin;
	const int y2 = h - track_y_margin;

	// Background
	dc.SetPen(*wxTRANSPARENT_PEN);
	dc.SetBrush(face);
	dc.DrawRectangle(0, 0, w, h);

	// Selection border
	if (HasFocus()) {
		dc.SetBrush(*wxTRANSPARENT_BRUSH);
		dc.SetPen(wxPen(shad, 1, wxPENSTYLE_DOT));
		dc.DrawRectangle(0, 0, w, h);
	}

	// Draw slider
	dc.SetPen(wxPen(shad));
	dc.DrawLine(x1, y1, x2, y1);
	dc.DrawLine(x1, y1, x1, y2);
	dc.SetPen(wxPen(high));
	dc.DrawLine(x1, y2, x2, y2);
	dc.DrawLine(x2, y1, x2, y2);

	// Draw keyframes
	int curX;
	if (OPT_GET("Video/Slider/Show Keyframes")->GetBool()) {
		dc.SetPen(wxPen(shad));
		for (int frame: keyframes) {
			curX = GetXAtValue(frame);
			dc.DrawLine(curX, tick_start, curX, tick_end);
		}
	}

	// Draw cursor
	curX = GetXAtValue(val);

	// Fill bg
	dc.SetBrush(wxBrush(face));
	dc.SetPen(*wxTRANSPARENT_PEN);
	// Adjusted for scaled thumb metrics
	dc.DrawRectangle(curX - (t_w / 2), y1 - 1, t_w, y2 - y1 + t_h_body);
	dc.SetBrush(wxNullBrush);

	// Draw cursor highlights
	dc.SetPen(wxPen(high));
	dc.DrawLine(curX, y1 - t_h_tip, curX - t_w, y1 + t_h_tip);
	dc.DrawLine(curX - (t_w - 1), y1 + t_h_tip, curX - (t_w - 1), y2 + t_h_body);

	// Draw cursor shades
	dc.SetPen(wxPen(shad));
	dc.DrawLine(curX + 1, y1 - 1, curX + t_w, y1 + t_h_tip);
	dc.DrawLine(curX + (t_w - 1), y1 + t_h_tip, curX + (t_w - 1), y2 + t_h_body);
	dc.DrawLine(curX - (t_w - 1), y2 + (t_h_body - 1), curX + (t_w - 1), y2 + (t_h_body - 1));

	// Draw cursor outline
	dc.SetPen(wxPen(bord));
	// Top Triangle
	dc.DrawLine(curX, y1 - (t_h_tip + 1), curX - t_w, y1 + 1);
	dc.DrawLine(curX, y1 - (t_h_tip + 1), curX + t_w, y1 + 1);
	// Sides
	dc.DrawLine(curX - t_w, y1 + 1, curX - t_w, y2 + t_h_body);
	dc.DrawLine(curX + t_w, y1 + 1, curX + t_w, y2 + t_h_body);
	// Bottom
	dc.DrawLine(curX - (t_w - 1), y2 + t_h_body, curX + t_w, y2 + t_h_body);
	// Inner horizontal accent
	dc.DrawLine(curX - (t_w - 1), y2, curX + t_w, y2);

	// Draw selection indicator (box at bottom)
	dc.SetPen(*wxTRANSPARENT_PEN);
	dc.SetBrush(HasFocus() ? wxBrush(sel) : wxBrush(notSel));
	dc.DrawRectangle(curX - (t_w - 1), y2 + 1, (t_w * 2) - 1, t_h_body - 1);
}

void VideoSlider::OnFocus(wxFocusEvent &) {
	Refresh(false);
}
