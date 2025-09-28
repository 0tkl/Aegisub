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

#include <algorithm>
#include <cmath>

namespace {
	inline wxSize DipSize(const wxWindow& win, int width, int height) {
		return win.FromDIP(wxSize(width, height));
	}

	inline int DipX(const wxWindow& win, int logical) {
		return win.FromDIP(wxSize(logical, 0)).GetWidth();
	}

	inline int DipY(const wxWindow& win, int logical) {
		return win.FromDIP(wxSize(0, logical)).GetHeight();
	}

	inline int DipLineWidth(const wxWindow& win, int logical = 1) {
		return std::max(DipX(win, logical), 1);
	}
}

VideoSlider::VideoSlider (wxWindow* parent, agi::Context *c)
: wxWindow(parent, -1, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS | wxFULL_REPAINT_ON_RESIZE)
, c(c)
, connections(agi::signal::make_vector({
	OPT_SUB("Video/Slider/Show Keyframes", [this] { Refresh(false); }),
	c->videoController->AddSeekListener(&VideoSlider::SetValue, this),
	c->project->AddVideoProviderListener(&VideoSlider::VideoOpened, this),
	c->project->AddKeyframesListener(&VideoSlider::KeyframesChanged, this),
}))
{
	const wxSize minClient = DipSize(*this, 20, 25);
	SetClientSize(minClient);
	SetMinClientSize(minClient);
	SetMinSize(minClient);
	SetInitialSize(minClient);
	SetBackgroundStyle(wxBG_STYLE_PAINT);

	Bind(wxEVT_DPI_CHANGED, [this](wxDPIChangedEvent& event) {
		const wxSize scaledMin = DipSize(*this, 20, 25);
		SetMinClientSize(scaledMin);
		SetMinSize(scaledMin);
		Refresh(false);
		event.Skip();
	});

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
	const int w = GetClientSize().GetWidth();
	const int margin = DipX(*this, 5);
	const int trackWidth = w - margin * 2;

	if (trackWidth <= 0 || max <= 0) return 0;

	const int clamped = mid(margin, x, std::max(margin, w - margin));
	return static_cast<int>((static_cast<int64_t>(clamped - margin) * max) / trackWidth);
}

int VideoSlider::GetXAtValue(int value) {
	if (max <= 0) return DipX(*this, 5);

	int w = GetClientSize().GetWidth();
	const int margin = DipX(*this, 5);
	const int trackWidth = w - margin * 2;

	if (trackWidth <= 0) return margin;

	value = mid(0, value, max);
	return static_cast<int>((static_cast<int64_t>(value) * trackWidth) / max) + margin;
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
	const bool had_focus = HasFocus();
	const int focusSlop = std::max(DipX(*this, 4), 1);

	if (event.ButtonDown())
		SetFocus();

	if (event.LeftIsDown()) {
		const int x = event.GetX();

		if (!had_focus && std::abs(x - GetXAtValue(val)) < focusSlop)
			return;

		if (event.ShiftDown() && keyframes.size()) {
			const int clickedFrame = GetValueAtX(x);
			auto pos = std::lower_bound(keyframes.begin(), keyframes.end(), clickedFrame);
			if (pos == keyframes.end())
				--pos;
			else if (pos + 1 != keyframes.end() && clickedFrame - *pos > (*(pos + 1)) - clickedFrame)
				++pos;

			if (*pos == val) return;
			SetValue(*pos);
		}
		else {
			const int go = GetValueAtX(x);
			if (go == val) return;
			SetValue(go);
		}

		c->videoController->JumpToFrame(val);
	}
	else if (event.GetWheelRotation() != 0 && ForwardMouseWheelEvent(this, event)) {
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

	wxColour shad = wxSystemSettings::GetColour(wxSYS_COLOUR_3DDKSHADOW);
	wxColour high = wxSystemSettings::GetColour(wxSYS_COLOUR_3DLIGHT);
	wxColour face = wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE);
	wxColour sel(123, 251, 232);
	wxColour notSel(sel.Red() * 2 / 5, sel.Green() * 2 / 5, sel.Blue() * 2 / 5);
	wxColour bord(0, 0, 0);

	const int margin = DipX(*this, 5);
	const int x1 = margin;
	int x2 = w - margin;
	if (x2 < x1) x2 = x1;

	const int trackTopMargin = DipY(*this, 8);
	const int trackBottomMargin = DipY(*this, 8);
	const int y1 = trackTopMargin;
	int y2 = h - trackBottomMargin;
	if (y2 < y1) y2 = y1;

	const int penWidth = DipLineWidth(*this);
	const int highlightOffsetY = std::max(DipY(*this, 2), 1);
	const int shadeOffsetY = std::max(DipY(*this, 1), 1);
	const int tipHeight = std::max(DipY(*this, 3), highlightOffsetY);
	const int bodyExtension = std::max(DipY(*this, 5), highlightOffsetY);
	const int handleHalfWidth = std::max(DipX(*this, 2), 1);
	const int innerHalfWidth = std::max(DipX(*this, 3), handleHalfWidth);
	const int outerHalfWidth = std::max(DipX(*this, 4), innerHalfWidth);
	const int shadeStartOffset = std::max(DipX(*this, 1), 1);
	const int selectionYOffset = std::max(DipY(*this, 1), 1);
	const int selectionHeight = std::max(DipY(*this, 4), 1);
	const int cursorBottom = y2 + bodyExtension;

	dc.SetPen(*wxTRANSPARENT_PEN);
	dc.SetBrush(face);
	dc.DrawRectangle(0, 0, w, h);

	if (HasFocus()) {
		dc.SetBrush(*wxTRANSPARENT_BRUSH);
		dc.SetPen(wxPen(shad, penWidth, wxPENSTYLE_DOT));
		dc.DrawRectangle(0, 0, w, h);
	}

	dc.SetPen(wxPen(shad, penWidth));
	dc.DrawLine(x1, y1, x2, y1);
	dc.DrawLine(x1, y1, x1, y2);
	dc.SetPen(wxPen(high, penWidth));
	dc.DrawLine(x1, y2, x2, y2);
	dc.DrawLine(x2, y1, x2, y2);

	if (OPT_GET("Video/Slider/Show Keyframes")->GetBool()) {
		const int keyframeTop = DipY(*this, 2);
		const int keyframeBottom = std::min(y1, DipY(*this, 8));
		if (keyframeTop < keyframeBottom) {
			dc.SetPen(wxPen(shad, penWidth));
			for (int frame : keyframes) {
				const int frameX = mid(x1, GetXAtValue(frame), x2);
				dc.DrawLine(frameX, keyframeTop, frameX, keyframeBottom);
			}
		}
	}

	int curX = mid(x1, GetXAtValue(val), x2);
	const int handleFillWidth = std::max(handleHalfWidth * 2, 1);
	const int handleFillHeight = std::max((y2 - y1) + bodyExtension, 1);
	const int handleFillTop = y1 - shadeOffsetY;

	dc.SetBrush(wxBrush(face));
	dc.SetPen(*wxTRANSPARENT_PEN);
	dc.DrawRectangle(curX - handleHalfWidth, handleFillTop, handleFillWidth, handleFillHeight);
	dc.SetBrush(wxNullBrush);

	dc.SetPen(wxPen(high, penWidth));
	dc.DrawLine(curX, y1 - highlightOffsetY, curX - outerHalfWidth, y1 + highlightOffsetY);
	dc.DrawLine(curX - innerHalfWidth, y1 + highlightOffsetY, curX - innerHalfWidth, cursorBottom);

	dc.SetPen(wxPen(shad, penWidth));
	dc.DrawLine(curX + shadeStartOffset, y1 - shadeOffsetY, curX + outerHalfWidth, y1 + highlightOffsetY);
	dc.DrawLine(curX + innerHalfWidth, y1 + highlightOffsetY, curX + innerHalfWidth, cursorBottom);
	dc.DrawLine(curX - innerHalfWidth, cursorBottom - shadeOffsetY, curX + innerHalfWidth, cursorBottom - shadeOffsetY);

	dc.SetPen(wxPen(bord, penWidth));
	dc.DrawLine(curX, y1 - tipHeight, curX - outerHalfWidth, y1 + shadeOffsetY);
	dc.DrawLine(curX, y1 - tipHeight, curX + outerHalfWidth, y1 + shadeOffsetY);
	dc.DrawLine(curX - outerHalfWidth, y1 + shadeOffsetY, curX - outerHalfWidth, cursorBottom);
	dc.DrawLine(curX + outerHalfWidth, y1 + shadeOffsetY, curX + outerHalfWidth, cursorBottom);
	dc.DrawLine(curX - innerHalfWidth, cursorBottom, curX + outerHalfWidth, cursorBottom);
	dc.DrawLine(curX - innerHalfWidth, y2, curX + outerHalfWidth, y2);

	dc.SetPen(*wxTRANSPARENT_PEN);
	dc.SetBrush(HasFocus() ? wxBrush(sel) : wxBrush(notSel));
	dc.DrawRectangle(curX - innerHalfWidth, y2 + selectionYOffset, innerHalfWidth + outerHalfWidth, selectionHeight);
}

void VideoSlider::OnFocus(wxFocusEvent &) {
	Refresh(false);
}
