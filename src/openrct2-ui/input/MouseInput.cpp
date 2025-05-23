#pragma region Copyright (c) 2014-2017 OpenRCT2 Developers
/*****************************************************************************
 * OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
 *
 * OpenRCT2 is the work of many authors, a full list can be found in contributors.md
 * For more information, visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * A full copy of the GNU General Public License can be found in licence.txt
 *****************************************************************************/
#pragma endregion

#include <openrct2-ui/interface/Dropdown.h>
#include <openrct2-ui/windows/Window.h>
#include <openrct2/Context.h>
#include <openrct2/Game.h>
#include <openrct2/Input.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/audio/audio.h>
#include <openrct2/config/Config.h>
#include <openrct2/core/Math.hpp>
#include <openrct2/core/Util.hpp>
#include <openrct2/interface/Cursors.h>
#include <openrct2/interface/Chat.h>
#include <openrct2/interface/Console.h>
#include <openrct2-ui/interface/Viewport.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/interface/Window.h>
#include <openrct2/localisation/Localisation.h>
#include <openrct2/platform/platform.h>
#include <openrct2/ride/RideData.h>
#include <openrct2/scenario/Scenario.h>
#include <openrct2/world/Banner.h>
#include <openrct2/world/Map.h>
#include <openrct2/world/Scenery.h>
#include <openrct2/world/Sprite.h>

struct rct_mouse_data
{
    uint32 x;
    uint32 y;
    uint32 state;
};

static rct_mouse_data _mouseInputQueue[64];
static uint8 _mouseInputQueueReadIndex = 0;
static uint8 _mouseInputQueueWriteIndex = 0;

static uint32 _ticksSinceDragStart;
static widget_ref _dragWidget;
static uint8 _dragScrollIndex;
static sint32 _originalWindowWidth;
static sint32 _originalWindowHeight;

static uint8 _currentScrollIndex;
static uint8 _currentScrollArea;

sint32 gInputDragLastX;
sint32 gInputDragLastY;

uint16 gTooltipTimeout;
widget_ref gTooltipWidget;
sint32 gTooltipCursorX;
sint32 gTooltipCursorY;

static sint16 _clickRepeatTicks;

static sint32 game_get_next_input(sint32 * x, sint32 * y);
static void input_widget_over(sint32 x, sint32 y, rct_window * w, rct_widgetindex widgetIndex);
static void input_widget_over_change_check(rct_windowclass windowClass, rct_windownumber windowNumber,
                                           rct_widgetindex widgetIndex);
static void input_widget_over_flatbutton_invalidate();
void process_mouse_over(sint32 x, sint32 y);
void process_mouse_tool(sint32 x, sint32 y);
void invalidate_scroll();
static rct_mouse_data * get_mouse_input();
void tile_element_right_click(sint32 type, rct_tile_element * tileElement, sint32 x, sint32 y);
static void game_handle_input_mouse(sint32 x, sint32 y, sint32 state);
static void input_widget_left(sint32 x, sint32 y, rct_window * w, rct_widgetindex widgetIndex);
void input_state_widget_pressed(sint32 x, sint32 y, sint32 state, rct_widgetindex widgetIndex, rct_window * w,
                                rct_widget * widget);
void set_cursor(uint8 cursor_id);
static void input_window_position_continue(rct_window * w, sint32 lastX, sint32 lastY, sint32 newX, sint32 newY);
static void input_window_position_end(rct_window * w, sint32 x, sint32 y);
static void input_window_resize_begin(rct_window * w, rct_widgetindex widgetIndex, sint32 x, sint32 y);
static void input_window_resize_continue(rct_window * w, sint32 x, sint32 y);
static void input_window_resize_end();
static void input_viewport_drag_begin(rct_window * w, sint32 x, sint32 y);
static void input_viewport_drag_continue();
static void input_viewport_drag_end();
static void input_scroll_begin(rct_window * w, rct_widgetindex widgetIndex, sint32 x, sint32 y);
static void input_scroll_continue(rct_window * w, rct_widgetindex widgetIndex, sint32 state, sint32 x, sint32 y);
static void input_scroll_end();
static void input_scroll_part_update_hthumb(rct_window * w, rct_widgetindex widgetIndex, sint32 x, sint32 scroll_id);
static void input_scroll_part_update_hleft(rct_window * w, rct_widgetindex widgetIndex, sint32 scroll_id);
static void input_scroll_part_update_hright(rct_window * w, rct_widgetindex widgetIndex, sint32 scroll_id);
static void input_scroll_part_update_vthumb(rct_window * w, rct_widgetindex widgetIndex, sint32 y, sint32 scroll_id);
static void input_scroll_part_update_vtop(rct_window * w, rct_widgetindex widgetIndex, sint32 scroll_id);
static void input_scroll_part_update_vbottom(rct_window * w, rct_widgetindex widgetIndex, sint32 scroll_id);
static void input_update_tooltip(rct_window * w, rct_widgetindex widgetIndex, sint32 x, sint32 y);

#pragma region Mouse input

/**
 *
 *  rct2: 0x006EA627
 */
void game_handle_input()
{
    for (rct_window * w = g_window_list; w < gWindowNextSlot; w++)
    {
        window_event_unknown_07_call(w);
    }

    invalidate_all_windows_after_input();

    sint32 x, y, state;
    while ((state = game_get_next_input(&x, &y)) != MOUSE_STATE_RELEASED)
    {
        game_handle_input_mouse(x, y, state & 0xFF);
    }

    if (_inputFlags & INPUT_FLAG_5)
    {
        game_handle_input_mouse(x, y, state);
    }
    else if (x != MONEY32_UNDEFINED)
    {
        sint32 screenWidth = context_get_width();
        sint32 screenHeight = context_get_height();
        x = Math::Clamp(0, x, screenWidth - 1);
        y = Math::Clamp(0, y, screenHeight - 1);

        game_handle_input_mouse(x, y, state);
        process_mouse_over(x, y);
        process_mouse_tool(x, y);
    }

    for (rct_window * w = g_window_list; w < gWindowNextSlot; w++)
    {
        window_event_unknown_08_call(w);
    }
}

/**
 *
 *  rct2: 0x006E83C7
 */
static sint32 game_get_next_input(sint32 * x, sint32 * y)
{
    rct_mouse_data * input = get_mouse_input();
    if (input == nullptr)
    {
        const CursorState * cursorState = context_get_cursor_state();
        *x = cursorState->x;
        *y = cursorState->y;
        return 0;
    }
    else
    {
        *x = input->x;
        *y = input->y;
        return input->state;
    }
}

/**
 *
 *  rct2: 0x00407074
 */
static rct_mouse_data * get_mouse_input()
{
    // Check if that location has been written to yet
    if (_mouseInputQueueReadIndex == _mouseInputQueueWriteIndex)
    {
        return nullptr;
    }
    else
    {
        rct_mouse_data * result = &_mouseInputQueue[_mouseInputQueueReadIndex];
        _mouseInputQueueReadIndex = (_mouseInputQueueReadIndex + 1) % Util::CountOf(_mouseInputQueue);
        return result;
    }
}

/**
 *
 *  rct2: 0x006E957F
 */
static void input_scroll_drag_begin(sint32 x, sint32 y, rct_window * w, rct_widget * widget, rct_widgetindex widgetIndex)
{
    _inputState = INPUT_STATE_SCROLL_RIGHT;
    gInputDragLastX = x;
    gInputDragLastY = y;
    _dragWidget.window_classification = w->classification;
    _dragWidget.window_number = w->number;
    _dragWidget.widget_index = widgetIndex;
    _ticksSinceDragStart = 0;

    _dragScrollIndex = window_get_scroll_data_index(w, widgetIndex);
    context_hide_cursor();
}

/**
 * Based on (heavily changed)
 *  rct2: 0x006E9E0E,  0x006E9ED0
 */
static void input_scroll_drag_continue(sint32 x, sint32 y, rct_window * w)
{
    rct_widgetindex widgetIndex = _dragWidget.widget_index;
    uint8 scrollIndex = _dragScrollIndex;

    rct_widget * widget = &w->widgets[widgetIndex];
    rct_scroll * scroll = &w->scrolls[scrollIndex];

    sint32 dx, dy;
    dx = x - gInputDragLastX;
    dy = y - gInputDragLastY;

    if (scroll->flags & HSCROLLBAR_VISIBLE)
    {
        sint16 size = widget->right - widget->left - 1;
        if (scroll->flags & VSCROLLBAR_VISIBLE)
            size -= 11;
        size = Math::Max(0, scroll->h_right - size);
        scroll->h_left = Math::Min<uint16>(Math::Max(0, scroll->h_left + dx), size);
    }

    if (scroll->flags & VSCROLLBAR_VISIBLE)
    {
        sint16 size = widget->bottom - widget->top - 1;
        if (scroll->flags & HSCROLLBAR_VISIBLE)
            size -= 11;
        size = Math::Max(0, scroll->v_bottom - size);
        scroll->v_top = Math::Min<uint16>(Math::Max(0, scroll->v_top + dy), size);
    }

    widget_scroll_update_thumbs(w, widgetIndex);
    window_invalidate_by_number(w->classification, w->number);

    sint32 fixedCursorPositionX = (sint32)ceilf(gInputDragLastX * gConfigGeneral.window_scale);
    sint32 fixedCursorPositionY = (sint32)ceilf(gInputDragLastY * gConfigGeneral.window_scale);

    context_set_cursor_position(fixedCursorPositionX, fixedCursorPositionY);
}

/**
 *
 *  rct2: 0x006E8ACB
 */
static void input_scroll_right(sint32 x, sint32 y, sint32 state)
{
    rct_window * w = window_find_by_number(_dragWidget.window_classification, _dragWidget.window_number);
    if (w == nullptr)
    {
        context_show_cursor();
        _inputState = INPUT_STATE_RESET;
        return;
    }

    switch (state)
    {
    case MOUSE_STATE_RELEASED:
        _ticksSinceDragStart += gTicksSinceLastUpdate;
        if (x != 0 || y != 0)
        {
            _ticksSinceDragStart = 1000;
            input_scroll_drag_continue(x, y, w);
        }
        break;
    case MOUSE_STATE_RIGHT_RELEASE:
        _inputState = INPUT_STATE_RESET;
        context_show_cursor();
        break;
    }
}

/**
 *
 *  rct2: 0x006E8655
 */
static void game_handle_input_mouse(sint32 x, sint32 y, sint32 state)
{
    rct_window * w;
    rct_widget * widget;
    rct_widgetindex widgetIndex;

    // Get window and widget under cursor position
    w = window_find_from_point(x, y);
    widgetIndex = w == nullptr ? -1 : window_find_widget_from_point(w, x, y);
    widget = widgetIndex == -1 ? nullptr : &w->widgets[widgetIndex];

    switch (_inputState)
    {
    case INPUT_STATE_RESET:
        window_tooltip_reset(x, y);
        // fall-through
    case INPUT_STATE_NORMAL:
        switch (state)
        {
        case MOUSE_STATE_RELEASED:
            input_widget_over(x, y, w, widgetIndex);
            break;
        case MOUSE_STATE_LEFT_PRESS:
            input_widget_left(x, y, w, widgetIndex);
            break;
        case MOUSE_STATE_RIGHT_PRESS:
            window_close_by_class(WC_TOOLTIP);

            if (w != nullptr)
            {
                w = window_bring_to_front(w);
            }

            if (widgetIndex != -1)
            {
                switch (widget->type)
                {
                case WWT_VIEWPORT:
                    if (!(gScreenFlags & (SCREEN_FLAGS_TRACK_MANAGER | SCREEN_FLAGS_TITLE_DEMO)))
                    {
                        //get vita joy
                        input_viewport_drag_begin(w, x, y);
                    }
                    break;
                case WWT_SCROLL:
                    input_scroll_drag_begin(x, y, w, widget, widgetIndex);
                    break;
                }
            }
            break;
        }
        break;
    case INPUT_STATE_WIDGET_PRESSED:
        input_state_widget_pressed(x, y, state, widgetIndex, w, widget);
        break;
    case INPUT_STATE_POSITIONING_WINDOW:
        w = window_find_by_number(_dragWidget.window_classification, _dragWidget.window_number);
        if (w == nullptr)
        {
            _inputState = INPUT_STATE_RESET;
        }
        else
        {
            input_window_position_continue(w, gInputDragLastX, gInputDragLastY, x, y);
            if (state == MOUSE_STATE_LEFT_RELEASE)
            {
                input_window_position_end(w, x, y);
            }
        }
        break;
    case INPUT_STATE_VIEWPORT_RIGHT:
        if (state == MOUSE_STATE_RELEASED)
        {
            input_viewport_drag_continue();
        }
        else if (state == MOUSE_STATE_RIGHT_RELEASE)
        {
            input_viewport_drag_end();
            if (_ticksSinceDragStart < 500)
            {
                // If the user pressed the right mouse button for less than 500 ticks, interpret as right click
                viewport_interaction_right_click(x, y);
            }
        }
        break;
    case INPUT_STATE_DROPDOWN_ACTIVE:
        input_state_widget_pressed(x, y, state, widgetIndex, w, widget);
        break;
    case INPUT_STATE_VIEWPORT_LEFT:
        w = window_find_by_number(_dragWidget.window_classification, _dragWidget.window_number);
        if (w == nullptr)
        {
            _inputState = INPUT_STATE_RESET;
            break;
        }

        switch (state)
        {
        case MOUSE_STATE_RELEASED:
            if (w->viewport == nullptr)
            {
                _inputState = INPUT_STATE_RESET;
                break;
            }

            if (w->classification != _dragWidget.window_classification || w->number != _dragWidget.window_number ||
                !(_inputFlags & INPUT_FLAG_TOOL_ACTIVE))
            {
                break;
            }

            w = window_find_by_number(gCurrentToolWidget.window_classification, gCurrentToolWidget.window_number);
            if (w == nullptr)
            {
                break;
            }

            window_event_tool_drag_call(w, gCurrentToolWidget.widget_index, x, y);
            break;
        case MOUSE_STATE_LEFT_RELEASE:
            _inputState = INPUT_STATE_RESET;
            if (_dragWidget.window_number == w->number)
            {
                if ((_inputFlags & INPUT_FLAG_TOOL_ACTIVE))
                {
                    w = window_find_by_number(gCurrentToolWidget.window_classification, gCurrentToolWidget.window_number);
                    if (w != nullptr)
                    {
                        window_event_tool_up_call(w, gCurrentToolWidget.widget_index, x, y);
                    }
                }
                else if (!(_inputFlags & INPUT_FLAG_4))
                {
                    viewport_interaction_left_click(x, y);
                }
            }
            break;
        }
        break;
    case INPUT_STATE_SCROLL_LEFT:
        switch (state)
        {
        case MOUSE_STATE_RELEASED:
            input_scroll_continue(w, widgetIndex, state, x, y);
            break;
        case MOUSE_STATE_LEFT_RELEASE:
            input_scroll_end();
            break;
        }
        break;
    case INPUT_STATE_RESIZING:
        w = window_find_by_number(_dragWidget.window_classification, _dragWidget.window_number);
        if (w == nullptr)
        {
            _inputState = INPUT_STATE_RESET;
        }
        else
        {
            if (state == MOUSE_STATE_LEFT_RELEASE)
            {
                input_window_resize_end();
            }
            if (state == MOUSE_STATE_RELEASED || state == MOUSE_STATE_LEFT_RELEASE)
            {
                input_window_resize_continue(w, x, y);
            }
        }
        break;
    case INPUT_STATE_SCROLL_RIGHT:
        input_scroll_right(x, y, state);
        break;
    }
}

#pragma region Window positioning / resizing

void input_window_position_begin(rct_window * w, rct_widgetindex widgetIndex, sint32 x, sint32 y)
{
    _inputState = INPUT_STATE_POSITIONING_WINDOW;
    gInputDragLastX = x - w->x;
    gInputDragLastY = y - w->y;
    _dragWidget.window_classification = w->classification;
    _dragWidget.window_number = w->number;
    _dragWidget.widget_index = widgetIndex;
}

static void input_window_position_continue(rct_window * w, sint32 lastX, sint32 lastY, sint32 newX, sint32 newY)
{
    sint32 snapProximity;

    snapProximity = (w->flags & WF_NO_SNAPPING) ? 0 : gConfigGeneral.window_snap_proximity;
    window_move_and_snap(w, newX - lastX, newY - lastY, snapProximity);
}

static void input_window_position_end(rct_window * w, sint32 x, sint32 y)
{
    _inputState = INPUT_STATE_NORMAL;
    gTooltipTimeout = 0;
    gTooltipWidget = _dragWidget;
    window_event_moved_call(w, x, y);
}

static void input_window_resize_begin(rct_window * w, rct_widgetindex widgetIndex, sint32 x, sint32 y)
{
    _inputState = INPUT_STATE_RESIZING;
    gInputDragLastX = x;
    gInputDragLastY = y;
    _dragWidget.window_classification = w->classification;
    _dragWidget.window_number = w->number;
    _dragWidget.widget_index = widgetIndex;
    _originalWindowWidth = w->width;
    _originalWindowHeight = w->height;
}

static void input_window_resize_continue(rct_window * w, sint32 x, sint32 y)
{
    if (y < (sint32)context_get_height() - 2)
    {
        sint32 dx, dy, targetWidth, targetHeight;
        dx = x - gInputDragLastX;
        dy = y - gInputDragLastY;
        targetWidth = _originalWindowWidth + dx;
        targetHeight = _originalWindowHeight + dy;

        window_resize(w, targetWidth - w->width, targetHeight - w->height);
    }
}

static void input_window_resize_end()
{
    _inputState = INPUT_STATE_NORMAL;
    gTooltipTimeout = 0;
    gTooltipWidget = _dragWidget;
}

#pragma endregion

#pragma region Viewport dragging

static void input_viewport_drag_begin(rct_window * w, sint32 x, sint32 y)
{
    w->flags &= ~WF_SCROLLING_TO_LOCATION;
    _inputState = INPUT_STATE_VIEWPORT_RIGHT;
    _dragWidget.window_classification = w->classification;
    _dragWidget.window_number = w->number;
    _ticksSinceDragStart = 0;
    context_get_cursor_position(&gInputDragLastX, &gInputDragLastY);
    context_hide_cursor();

    window_unfollow_sprite(w);
    // gInputFlags |= INPUT_FLAG_5;
}

static void input_viewport_drag_continue()
{
    sint32 dx, dy, newDragX, newDragY;
    rct_window * w;
    rct_viewport * viewport;

    context_get_cursor_position(&newDragX, &newDragY);

    dx = newDragX - gInputDragLastX;
    dy = newDragY - gInputDragLastY;
    w = window_find_by_number(_dragWidget.window_classification, _dragWidget.window_number);

    // #3294: Window can be closed during a drag session, so just finish
    //        the session if the window no longer exists
    if (w == nullptr)
    {
        input_viewport_drag_end();
        return;
    }

    viewport = w->viewport;
    _ticksSinceDragStart += gTicksSinceLastUpdate;
    if (viewport == nullptr)
    {
        context_show_cursor();
        _inputState = INPUT_STATE_RESET;
    }
    else if (dx != 0 || dy != 0)
    {
        if (!(w->flags & WF_NO_SCROLLING))
        {
            // User dragged a scrollable viewport

            // If the drag time is less than 500 the "drag" is usually interpreted as a right click.
            // As the user moved the mouse, don't interpret it as right click in any case.
            _ticksSinceDragStart = 1000;

            dx *= 1 << (viewport->zoom + 1);
            dy *= 1 << (viewport->zoom + 1);
            if (gConfigGeneral.invert_viewport_drag)
            {
                w->saved_view_x -= dx;
                w->saved_view_y -= dy;
            }
            else
            {
                w->saved_view_x += dx;
                w->saved_view_y += dy;
            }
        }
    }

    context_set_cursor_position(gInputDragLastX, gInputDragLastY);
}

static void input_viewport_drag_end()
{
    _inputState = INPUT_STATE_RESET;
    context_show_cursor();
}

#pragma endregion

#pragma region Scroll bars

static void input_scroll_begin(rct_window * w, rct_widgetindex widgetIndex, sint32 x, sint32 y)
{
    rct_widget * widget;

    widget = &w->widgets[widgetIndex];

    _inputState = INPUT_STATE_SCROLL_LEFT;
    gPressedWidget.window_classification = w->classification;
    gPressedWidget.window_number = w->number;
    gPressedWidget.widget_index = widgetIndex;
    gTooltipCursorX = x;
    gTooltipCursorY = y;

    sint32 eax, ebx, scroll_area, scroll_id;
    scroll_id = 0; // safety
    widget_scroll_get_part(w, widget, x, y, &eax, &ebx, &scroll_area, &scroll_id);

    _currentScrollArea = scroll_area;
    _currentScrollIndex = scroll_id;
    window_event_unknown_15_call(w, scroll_id, scroll_area);
    if (scroll_area == SCROLL_PART_VIEW)
    {
        window_event_scroll_mousedown_call(w, scroll_id, eax, ebx);
        return;
    }

    rct_widget * widg = &w->widgets[widgetIndex];
    rct_scroll * scroll = &w->scrolls[scroll_id];

    sint32 widget_width = widg->right - widg->left - 1;
    if (scroll->flags & VSCROLLBAR_VISIBLE)
        widget_width -= 11;
    sint32 widget_content_width = Math::Max(scroll->h_right - widget_width, 0);

    sint32 widget_height = widg->bottom - widg->top - 1;
    if (scroll->flags & HSCROLLBAR_VISIBLE)
        widget_height -= 11;
    sint32 widget_content_height = Math::Max(scroll->v_bottom - widget_height, 0);

    switch (scroll_area)
    {
    case SCROLL_PART_HSCROLLBAR_LEFT:
        scroll->h_left = Math::Max(scroll->h_left - 3, 0);
        break;
    case SCROLL_PART_HSCROLLBAR_RIGHT:
        scroll->h_left = Math::Min(scroll->h_left + 3, widget_content_width);
        break;
    case SCROLL_PART_HSCROLLBAR_LEFT_TROUGH:
        scroll->h_left = Math::Max(scroll->h_left - widget_width, 0);
        break;
    case SCROLL_PART_HSCROLLBAR_RIGHT_TROUGH:
        scroll->h_left = Math::Min(scroll->h_left + widget_width, widget_content_width);
        break;
    case SCROLL_PART_VSCROLLBAR_TOP:
        scroll->v_top = Math::Max(scroll->v_top - 3, 0);
        break;
    case SCROLL_PART_VSCROLLBAR_BOTTOM:
        scroll->v_top = Math::Min(scroll->v_top + 3, widget_content_height);
        break;
    case SCROLL_PART_VSCROLLBAR_TOP_TROUGH:
        scroll->v_top = Math::Max(scroll->v_top - widget_height, 0);
        break;
    case SCROLL_PART_VSCROLLBAR_BOTTOM_TROUGH:
        scroll->v_top = Math::Min(scroll->v_top + widget_height, widget_content_height);
        break;
    default:
        break;
    }
    widget_scroll_update_thumbs(w, widgetIndex);
    window_invalidate_by_number(widgetIndex, w->classification);
}

static void input_scroll_continue(rct_window * w, rct_widgetindex widgetIndex, sint32 state, sint32 x, sint32 y)
{
    rct_widget * widget;
    sint32 scroll_part, scroll_id;
    sint32 x2, y2;

    assert(w != nullptr);

    widget = &w->widgets[widgetIndex];
    if (w->classification != gPressedWidget.window_classification || w->number != gPressedWidget.window_number ||
        widgetIndex != gPressedWidget.widget_index)
    {
        invalidate_scroll();
        return;
    }

    widget_scroll_get_part(w, widget, x, y, &x2, &y2, &scroll_part, &scroll_id);

    if (_currentScrollArea == SCROLL_PART_HSCROLLBAR_THUMB)
    {
        sint32 originalTooltipCursorX = gTooltipCursorX;
        gTooltipCursorX = x;
        input_scroll_part_update_hthumb(w, widgetIndex, x - originalTooltipCursorX, scroll_id);
        return;
    }

    if (_currentScrollArea == SCROLL_PART_VSCROLLBAR_THUMB)
    {
        sint32 originalTooltipCursorY = gTooltipCursorY;
        gTooltipCursorY = y;
        input_scroll_part_update_vthumb(w, widgetIndex, y - originalTooltipCursorY, scroll_id);
        return;
    }

    x = x2;
    y = y2;

    if (scroll_part != _currentScrollArea)
    {
        invalidate_scroll();
        return;
    }

    switch (scroll_part)
    {
    case SCROLL_PART_VIEW:
        window_event_scroll_mousedrag_call(w, scroll_id, x, y);
        break;
    case SCROLL_PART_HSCROLLBAR_LEFT:
        input_scroll_part_update_hleft(w, widgetIndex, scroll_id);
        break;
    case SCROLL_PART_HSCROLLBAR_RIGHT:
        input_scroll_part_update_hright(w, widgetIndex, scroll_id);
        break;
    case SCROLL_PART_VSCROLLBAR_TOP:
        input_scroll_part_update_vtop(w, widgetIndex, scroll_id);
        break;
    case SCROLL_PART_VSCROLLBAR_BOTTOM:
        input_scroll_part_update_vbottom(w, widgetIndex, scroll_id);
        break;
    }
}

static void input_scroll_end()
{
    _inputState = INPUT_STATE_RESET;
    invalidate_scroll();
}

/**
 *
 *  rct2: 0x006E98F2
 */
static void input_scroll_part_update_hthumb(rct_window * w, rct_widgetindex widgetIndex, sint32 x, sint32 scroll_id)
{
    rct_widget * widget = &w->widgets[widgetIndex];

    if (window_find_by_number(w->classification, w->number))
    {
        sint32 newLeft;
        newLeft = w->scrolls[scroll_id].h_right;
        newLeft *= x;
        x = widget->right - widget->left - 21;
        if (w->scrolls[scroll_id].flags & VSCROLLBAR_VISIBLE)
            x -= 11;
        newLeft /= x;
        x = newLeft;
        w->scrolls[scroll_id].flags |= HSCROLLBAR_THUMB_PRESSED;
        newLeft = w->scrolls[scroll_id].h_left;
        newLeft += x;
        if (newLeft < 0)
            newLeft = 0;
        x = widget->right - widget->left - 1;
        if (w->scrolls[scroll_id].flags & VSCROLLBAR_VISIBLE)
            x -= 11;
        x *= -1;
        x += w->scrolls[scroll_id].h_right;
        if (x < 0)
            x = 0;
        if (newLeft > x)
            newLeft = x;
        w->scrolls[scroll_id].h_left = newLeft;
        widget_scroll_update_thumbs(w, widgetIndex);
        widget_invalidate_by_number(w->classification, w->number, widgetIndex);
    }
}

/**
 *
 *  rct2: 0x006E99A9
 */
static void input_scroll_part_update_vthumb(rct_window * w, rct_widgetindex widgetIndex, sint32 y, sint32 scroll_id)
{
    assert(w != nullptr);
    rct_widget * widget = &w->widgets[widgetIndex];

    if (window_find_by_number(w->classification, w->number))
    {
        sint32 newTop;
        newTop = w->scrolls[scroll_id].v_bottom;
        newTop *= y;
        y = widget->bottom - widget->top - 21;
        if (w->scrolls[scroll_id].flags & HSCROLLBAR_VISIBLE)
            y -= 11;
        newTop /= y;
        y = newTop;
        w->scrolls[scroll_id].flags |= VSCROLLBAR_THUMB_PRESSED;
        newTop = w->scrolls[scroll_id].v_top;
        newTop += y;
        if (newTop < 0)
            newTop = 0;
        y = widget->bottom - widget->top - 1;
        if (w->scrolls[scroll_id].flags & HSCROLLBAR_VISIBLE)
            y -= 11;
        y *= -1;
        y += w->scrolls[scroll_id].v_bottom;
        if (y < 0)
            y = 0;
        if (newTop > y)
            newTop = y;
        w->scrolls[scroll_id].v_top = newTop;
        widget_scroll_update_thumbs(w, widgetIndex);
        widget_invalidate_by_number(w->classification, w->number, widgetIndex);
    }
}

/**
 *
 *  rct2: 0x006E9A60
 */
static void input_scroll_part_update_hleft(rct_window * w, rct_widgetindex widgetIndex, sint32 scroll_id)
{
    assert(w != nullptr);
    if (window_find_by_number(w->classification, w->number))
    {
        w->scrolls[scroll_id].flags |= HSCROLLBAR_LEFT_PRESSED;
        if (w->scrolls[scroll_id].h_left >= 3)
            w->scrolls[scroll_id].h_left -= 3;
        widget_scroll_update_thumbs(w, widgetIndex);
        widget_invalidate_by_number(w->classification, w->number, widgetIndex);
    }
}

/**
 *
 *  rct2: 0x006E9ABF
 */
static void input_scroll_part_update_hright(rct_window * w, rct_widgetindex widgetIndex, sint32 scroll_id)
{
    assert(w != nullptr);
    rct_widget * widget = &w->widgets[widgetIndex];
    if (window_find_by_number(w->classification, w->number))
    {
        w->scrolls[scroll_id].flags |= HSCROLLBAR_RIGHT_PRESSED;
        w->scrolls[scroll_id].h_left += 3;
        sint32 newLeft = widget->right - widget->left - 1;
        if (w->scrolls[scroll_id].flags & VSCROLLBAR_VISIBLE)
            newLeft -= 11;
        newLeft *= -1;
        newLeft += w->scrolls[scroll_id].h_right;
        if (newLeft < 0)
            newLeft = 0;
        if (w->scrolls[scroll_id].h_left > newLeft)
            w->scrolls[scroll_id].h_left = newLeft;
        widget_scroll_update_thumbs(w, widgetIndex);
        widget_invalidate_by_number(w->classification, w->number, widgetIndex);
    }
}

/**
 *
 *  rct2: 0x006E9C37
 */
static void input_scroll_part_update_vtop(rct_window * w, rct_widgetindex widgetIndex, sint32 scroll_id)
{
    assert(w != nullptr);
    if (window_find_by_number(w->classification, w->number))
    {
        w->scrolls[scroll_id].flags |= VSCROLLBAR_UP_PRESSED;
        if (w->scrolls[scroll_id].v_top >= 3)
            w->scrolls[scroll_id].v_top -= 3;
        widget_scroll_update_thumbs(w, widgetIndex);
        widget_invalidate_by_number(w->classification, w->number, widgetIndex);
    }
}

/**
 *
 *  rct2: 0x006E9C96
 */
static void input_scroll_part_update_vbottom(rct_window * w, rct_widgetindex widgetIndex, sint32 scroll_id)
{
    assert(w != nullptr);
    rct_widget * widget = &w->widgets[widgetIndex];
    if (window_find_by_number(w->classification, w->number))
    {
        w->scrolls[scroll_id].flags |= VSCROLLBAR_DOWN_PRESSED;
        w->scrolls[scroll_id].v_top += 3;
        sint32 newTop = widget->bottom - widget->top - 1;
        if (w->scrolls[scroll_id].flags & HSCROLLBAR_VISIBLE)
            newTop -= 11;
        newTop *= -1;
        newTop += w->scrolls[scroll_id].v_bottom;
        if (newTop < 0)
            newTop = 0;
        if (w->scrolls[scroll_id].v_top > newTop)
            w->scrolls[scroll_id].v_top = newTop;
        widget_scroll_update_thumbs(w, widgetIndex);
        widget_invalidate_by_number(w->classification, w->number, widgetIndex);
    }
}

#pragma endregion

#pragma region Widgets

/**
 *
 *  rct2: 0x006E9253
 */
static void input_widget_over(sint32 x, sint32 y, rct_window * w, rct_widgetindex widgetIndex)
{
    rct_windowclass windowClass = WC_NULL;
    rct_windownumber windowNumber = 0;
    rct_widget * widget = nullptr;

    if (w != nullptr)
    {
        windowClass = w->classification;
        windowNumber = w->number;
        widget = &w->widgets[widgetIndex];
    }

    input_widget_over_change_check(windowClass, windowNumber, widgetIndex);

    if (w != nullptr && widgetIndex != -1 && widget->type == WWT_SCROLL)
    {
        sint32 eax, ebx, scroll_part, edx;
        widget_scroll_get_part(w, widget, x, y, &eax, &ebx, &scroll_part, &edx);

        if (scroll_part != SCROLL_PART_VIEW)
            window_tooltip_close();
        else
        {
            window_event_scroll_mouseover_call(w, edx, eax, ebx);
            input_update_tooltip(w, widgetIndex, x, y);
        }
    }
    else
    {
        input_update_tooltip(w, widgetIndex, x, y);
    }

    gTooltipTimeout = 0;
    gTooltipCursorX = x;
    gTooltipCursorY = y;
}

/**
 *
 *  rct2: 0x006E9269
 */
static void input_widget_over_change_check(rct_windowclass windowClass, rct_windownumber windowNumber,
                                           rct_widgetindex widgetIndex)
{
    // Prevents invalid widgets being clicked source of bug is elsewhere
    if (widgetIndex == -1)
        return;

    // Check if the widget that the cursor was over, has changed
    if (windowClass != gHoverWidget.window_classification || windowNumber != gHoverWidget.window_number ||
        widgetIndex != gHoverWidget.widget_index)
    {
        // Invalidate last widget cursor was on if widget is a flat button
        input_widget_over_flatbutton_invalidate();

        // Set new cursor over widget
        gHoverWidget.window_classification = windowClass;
        gHoverWidget.window_number = windowNumber;
        gHoverWidget.widget_index = widgetIndex;

        // Invalidate new widget cursor is on if widget is a flat button
        if (windowClass != 255)
            input_widget_over_flatbutton_invalidate();
    }
}

/**
 * Used to invalidate flat button widgets when the mouse leaves and enters them. This should be generalised so that all widgets
 * can use this in the future.
 */
static void input_widget_over_flatbutton_invalidate()
{
    rct_window * w = window_find_by_number(gHoverWidget.window_classification, gHoverWidget.window_number);
    if (w != nullptr)
    {
        window_event_invalidate_call(w);
        if (w->widgets[gHoverWidget.widget_index].type == WWT_FLATBTN)
        {
            widget_invalidate_by_number(gHoverWidget.window_classification, gHoverWidget.window_number,
                                        gHoverWidget.widget_index);
        }
    }
}

/**
 *
 *  rct2: 0x006E95F9
 */
static void input_widget_left(sint32 x, sint32 y, rct_window * w, rct_widgetindex widgetIndex)
{
    rct_windowclass windowClass = WC_NULL;
    rct_windownumber windowNumber = 0;
    rct_widget * widget;

    if (w != nullptr)
    {
        windowClass = w->classification;
        windowNumber = w->number;
    }

    window_close_by_class(WC_ERROR);
    window_close_by_class(WC_TOOLTIP);

    // Window might have changed position in the list, therefore find it again
    w = window_find_by_number(windowClass, windowNumber);
    if (w == nullptr)
        return;

    w = window_bring_to_front(w);
    if (widgetIndex == -1)
        return;

    if (windowClass != gCurrentTextBox.window.classification || windowNumber != gCurrentTextBox.window.number ||
        widgetIndex != gCurrentTextBox.widget_index)
    {
        window_cancel_textbox();
    }

    widget = &w->widgets[widgetIndex];

    switch (widget->type)
    {
    case WWT_FRAME:
    case WWT_RESIZE:
        if (window_can_resize(w) && (x >= w->x + w->width - 19 && y >= w->y + w->height - 19))
            input_window_resize_begin(w, widgetIndex, x, y);
        break;
    case WWT_VIEWPORT:
        _inputState = INPUT_STATE_VIEWPORT_LEFT;
        gInputDragLastX = x;
        gInputDragLastY = y;
        _dragWidget.window_classification = windowClass;
        _dragWidget.window_number = windowNumber;
        if (_inputFlags & INPUT_FLAG_TOOL_ACTIVE)
        {
            w = window_find_by_number(gCurrentToolWidget.window_classification, gCurrentToolWidget.window_number);
            if (w != nullptr)
            {
                window_event_tool_down_call(w, gCurrentToolWidget.widget_index, x, y);
                _inputFlags |= INPUT_FLAG_4;
            }
        }
        break;
    case WWT_CAPTION:
        input_window_position_begin(w, widgetIndex, x, y);
        break;
    case WWT_SCROLL:
        input_scroll_begin(w, widgetIndex, x, y);
        break;
    default:
        if (widget_is_enabled(w, widgetIndex) && !widget_is_disabled(w, widgetIndex))
        {
            audio_play_sound(SOUND_CLICK_1, 0, w->x + ((widget->left + widget->right) / 2));

            // Set new cursor down widget
            gPressedWidget.window_classification = windowClass;
            gPressedWidget.window_number = windowNumber;
            gPressedWidget.widget_index = widgetIndex;
            _inputFlags |= INPUT_FLAG_WIDGET_PRESSED;
            _inputState = INPUT_STATE_WIDGET_PRESSED;
            _clickRepeatTicks = 1;

            widget_invalidate_by_number(windowClass, windowNumber, widgetIndex);
            window_event_mouse_down_call(w, widgetIndex);
        }
        break;
    }
}

#pragma endregion

/**
 *
 *  rct2: 0x006ED833
 */
void process_mouse_over(sint32 x, sint32 y)
{
    rct_window * window;

    sint32 cursorId;

    cursorId = CURSOR_ARROW;
    set_map_tooltip_format_arg(0, rct_string_id, STR_NONE);
    window = window_find_from_point(x, y);

    if (window != nullptr)
    {
        sint32 ebx, edi;
        rct_window * subWindow;
        rct_widgetindex widgetId = window_find_widget_from_point(window, x, y);
        if (widgetId != -1)
        {
            switch (window->widgets[widgetId].type)
            {

            case WWT_VIEWPORT:
                if (!(_inputFlags & INPUT_FLAG_TOOL_ACTIVE))
                {
                    if (viewport_interaction_left_over(x, y))
                    {
                        set_cursor(CURSOR_HAND_POINT);
                        return;
                    }
                    break;
                }
                cursorId = gCurrentToolId;
                subWindow = window_find_by_number(gCurrentToolWidget.window_classification, gCurrentToolWidget.window_number);
                if (subWindow == nullptr)
                    break;

                ebx = 0;
                edi = cursorId;
                // Window event WE_UNKNOWN_0E was called here, but no windows actually implemented a handler and
                // it's not known what it was for
                cursorId = edi;
                if ((ebx & 0xFF) != 0)
                {
                    set_cursor(cursorId);
                    return;
                }
                break;

            case WWT_FRAME:
            case WWT_RESIZE:
                if (!(window->flags & WF_RESIZABLE))
                    break;

                if (window->min_width == window->max_width && window->min_height == window->max_height)
                    break;

                if (x < window->x + window->width - 0x13)
                    break;

                if (y < window->y + window->height - 0x13)
                    break;

                cursorId = CURSOR_DIAGONAL_ARROWS;
                break;

            case WWT_SCROLL:
            {
                sint32 output_scroll_area, scroll_id;
                sint32 scroll_x, scroll_y;
                widget_scroll_get_part(window, &window->widgets[widgetId], x, y, &scroll_x, &scroll_y, &output_scroll_area,
                                       &scroll_id);
                cursorId = scroll_id;
                if (output_scroll_area != SCROLL_PART_VIEW)
                {
                    cursorId = CURSOR_ARROW;
                    break;
                }
                // Same as default but with scroll_x/y
                cursorId = window_event_cursor_call(window, widgetId, scroll_x, scroll_y);
                if (cursorId == -1)
                    cursorId = CURSOR_ARROW;
                break;
            }
            default:
                cursorId = window_event_cursor_call(window, widgetId, x, y);
                if (cursorId == -1)
                    cursorId = CURSOR_ARROW;
                break;
            }
        }
    }

    viewport_interaction_right_over(x, y);
    set_cursor(cursorId);
}

/**
 *
 *  rct2: 0x006ED801
 */
void process_mouse_tool(sint32 x, sint32 y)
{
    if (_inputFlags & INPUT_FLAG_TOOL_ACTIVE)
    {
        rct_window * w = window_find_by_number(gCurrentToolWidget.window_classification, gCurrentToolWidget.window_number);

        if (!w)
            tool_cancel();
        else
            window_event_tool_update_call(w, gCurrentToolWidget.widget_index, x, y);
    }
}

/**
 *
 *  rct2: 0x006E8DA7
 */
void input_state_widget_pressed(sint32 x, sint32 y, sint32 state, rct_widgetindex widgetIndex, rct_window * w,
                                rct_widget * widget)
{
    rct_windowclass cursor_w_class;
    rct_windownumber cursor_w_number;
    cursor_w_class = gPressedWidget.window_classification;
    cursor_w_number = gPressedWidget.window_number;
    rct_widgetindex cursor_widgetIndex = gPressedWidget.widget_index;

    rct_window * cursor_w = window_find_by_number(cursor_w_class, cursor_w_number);
    if (cursor_w == nullptr)
    {
        _inputState = INPUT_STATE_RESET;
        return;
    }

    switch (state)
    {
    case MOUSE_STATE_RELEASED:
        if (!w || cursor_w_class != w->classification || cursor_w_number != w->number || widgetIndex != cursor_widgetIndex)
            break;

        if (w->disabled_widgets & (1ULL << widgetIndex))
            break;

        if (_clickRepeatTicks != 0)
        {
            _clickRepeatTicks++;

            // Handle click repeat
            if (_clickRepeatTicks >= 16 && (_clickRepeatTicks & 3) == 0)
            {
                if (w->hold_down_widgets & (1ULL << widgetIndex))
                {
                    window_event_mouse_down_call(w, widgetIndex);
                }
            }
        }

        if (_inputFlags & INPUT_FLAG_WIDGET_PRESSED)
        {
            if (_inputState == INPUT_STATE_DROPDOWN_ACTIVE)
            {
                gDropdownHighlightedIndex = gDropdownDefaultIndex;
                window_invalidate_by_class(WC_DROPDOWN);
            }
            return;
        }

        _inputFlags |= INPUT_FLAG_WIDGET_PRESSED;
        widget_invalidate_by_number(cursor_w_class, cursor_w_number, widgetIndex);
        return;
    case MOUSE_STATE_LEFT_RELEASE:
    case MOUSE_STATE_RIGHT_PRESS:
        if (_inputState == INPUT_STATE_DROPDOWN_ACTIVE)
        {
            if (w)
            {
                sint32 dropdown_index = 0;

                if (w->classification == WC_DROPDOWN)
                {
                    dropdown_index = dropdown_index_from_point(x, y, w);
                    if (dropdown_index == -1)
                    {
                        goto dropdown_cleanup;
                    }

                    if (dropdown_index < DROPDOWN_ITEMS_MAX_SIZE && dropdown_is_disabled(dropdown_index))
                    {
                        goto dropdown_cleanup;
                    }

                    if (gDropdownItemsFormat[dropdown_index] == DROPDOWN_SEPARATOR)
                    {
                        goto dropdown_cleanup;
                    }
                }
                else
                {
                    if (cursor_w_class != w->classification || cursor_w_number != w->number ||
                        widgetIndex != cursor_widgetIndex)
                        goto dropdown_cleanup;
                    dropdown_index = -1;
                    if (_inputFlags & INPUT_FLAG_DROPDOWN_STAY_OPEN)
                    {
                        if (!(_inputFlags & INPUT_FLAG_DROPDOWN_MOUSE_UP))
                        {
                            _inputFlags |= INPUT_FLAG_DROPDOWN_MOUSE_UP;
                            return;
                        }
                    }
                }

                window_close_by_class(WC_DROPDOWN);
                cursor_w = window_find_by_number(cursor_w_class, cursor_w_number);
                if (_inputFlags & INPUT_FLAG_WIDGET_PRESSED)
                {
                    _inputFlags &= ~INPUT_FLAG_WIDGET_PRESSED;
                    widget_invalidate_by_number(cursor_w_class, cursor_w_number, cursor_widgetIndex);
                }

                _inputState = INPUT_STATE_NORMAL;
                gTooltipTimeout = 0;
                gTooltipWidget.widget_index = cursor_widgetIndex;
                gTooltipWidget.window_classification = cursor_w_class;
                gTooltipWidget.window_number = cursor_w_number;

                if (dropdown_index == -1)
                {
                    if (!dropdown_is_disabled(gDropdownDefaultIndex))
                    {
                        dropdown_index = gDropdownDefaultIndex;
                    }
                }
                window_event_dropdown_call(cursor_w, cursor_widgetIndex, dropdown_index);
            }
        dropdown_cleanup:
            window_close_by_class(WC_DROPDOWN);
        }

        _inputState = INPUT_STATE_NORMAL;

        if (state == MOUSE_STATE_RIGHT_PRESS)
        {
            return;
        }

        gTooltipTimeout = 0;
        gTooltipWidget.widget_index = cursor_widgetIndex;

        if (!w)
            break;

        if (!widget)
            break;

        {
            sint32 mid_point_x = (widget->left + widget->right) / 2 + w->x;
            audio_play_sound(SOUND_CLICK_2, 0, mid_point_x);
        }
        if (cursor_w_class != w->classification || cursor_w_number != w->number || widgetIndex != cursor_widgetIndex)
            break;

        if (w->disabled_widgets & (1ULL << widgetIndex))
            break;

        widget_invalidate_by_number(cursor_w_class, cursor_w_number, widgetIndex);
        window_event_mouse_up_call(w, widgetIndex);
    default:
        return;
    }

    _clickRepeatTicks = 0;
    if (_inputState != INPUT_STATE_DROPDOWN_ACTIVE)
    {
        // Hold down widget and drag outside of area??
        if (_inputFlags & INPUT_FLAG_WIDGET_PRESSED)
        {
            _inputFlags &= ~INPUT_FLAG_WIDGET_PRESSED;
            widget_invalidate_by_number(cursor_w_class, cursor_w_number, cursor_widgetIndex);
        }
        return;
    }

    gDropdownHighlightedIndex = -1;
    window_invalidate_by_class(WC_DROPDOWN);
    if (w == nullptr)
    {
        return;
    }

    if (w->classification == WC_DROPDOWN)
    {
        sint32 dropdown_index = dropdown_index_from_point(x, y, w);
        if (dropdown_index == -1)
        {
            return;
        }

        if (gDropdownIsColour && gDropdownLastColourHover != dropdown_index)
        {
            gDropdownLastColourHover = dropdown_index;
            window_tooltip_close();

            static constexpr const rct_string_id colourTooltips[] = {
                STR_COLOUR_BLACK_TIP,
                STR_COLOUR_GREY_TIP,
                STR_COLOUR_WHITE_TIP,
                STR_COLOUR_DARK_PURPLE_TIP,
                STR_COLOUR_LIGHT_PURPLE_TIP,
                STR_COLOUR_BRIGHT_PURPLE_TIP,
                STR_COLOUR_DARK_BLUE_TIP,
                STR_COLOUR_LIGHT_BLUE_TIP,
                STR_COLOUR_ICY_BLUE_TIP,
                STR_COLOUR_TEAL_TIP,
                STR_COLOUR_AQUAMARINE_TIP,
                STR_COLOUR_SATURATED_GREEN_TIP,
                STR_COLOUR_DARK_GREEN_TIP,
                STR_COLOUR_MOSS_GREEN_TIP,
                STR_COLOUR_BRIGHT_GREEN_TIP,
                STR_COLOUR_OLIVE_GREEN_TIP,
                STR_COLOUR_DARK_OLIVE_GREEN_TIP,
                STR_COLOUR_BRIGHT_YELLOW_TIP,
                STR_COLOUR_YELLOW_TIP,
                STR_COLOUR_DARK_YELLOW_TIP,
                STR_COLOUR_LIGHT_ORANGE_TIP,
                STR_COLOUR_DARK_ORANGE_TIP,
                STR_COLOUR_LIGHT_BROWN_TIP,
                STR_COLOUR_SATURATED_BROWN_TIP,
                STR_COLOUR_DARK_BROWN_TIP,
                STR_COLOUR_SALMON_PINK_TIP,
                STR_COLOUR_BORDEAUX_RED_TIP,
                STR_COLOUR_SATURATED_RED_TIP,
                STR_COLOUR_BRIGHT_RED_TIP,
                STR_COLOUR_DARK_PINK_TIP,
                STR_COLOUR_BRIGHT_PINK_TIP,
                STR_COLOUR_LIGHT_PINK_TIP,
            };

            window_tooltip_show(colourTooltips[dropdown_index], x, y);
        }

        if (dropdown_index < DROPDOWN_ITEMS_MAX_SIZE && dropdown_is_disabled(dropdown_index))
        {
            return;
        }

        if (gDropdownItemsFormat[dropdown_index] == DROPDOWN_SEPARATOR)
        {
            return;
        }

        gDropdownHighlightedIndex = dropdown_index;
        window_invalidate_by_class(WC_DROPDOWN);
    }
    else
    {
        gDropdownLastColourHover = -1;
        window_tooltip_close();
    }
}

static void input_update_tooltip(rct_window * w, rct_widgetindex widgetIndex, sint32 x, sint32 y)
{
    if (gTooltipWidget.window_classification == 255)
    {
        if (gTooltipCursorX == x && gTooltipCursorY == y)
        {
            _tooltipNotShownTicks++;
            if (_tooltipNotShownTicks > 50)
            {
                gTooltipTimeout = 0;
                window_tooltip_open(w, widgetIndex, x, y);
            }
        }
    }
    else
    {
        reset_tooltip_not_shown();

        if (w == nullptr || gTooltipWidget.window_classification != w->classification ||
            gTooltipWidget.window_number != w->number || gTooltipWidget.widget_index != widgetIndex)
        {
            window_tooltip_close();
        }

        gTooltipTimeout += gTicksSinceLastUpdate;
        if (gTooltipTimeout >= 8000)
        {
            window_close_by_class(WC_TOOLTIP);
        }
    }
}

#pragma endregion

#pragma region Keyboard input

/**
 *
 *  rct2: 0x00406CD2
 */
sint32 get_next_key()
{
    uint8 * keysPressed = (uint8 *)context_get_keys_pressed();
    for (sint32 i = 0; i < 221; i++)
    {
        if (keysPressed[i])
        {
            keysPressed[i] = 0;
            return i;
        }
    }

    return 0;
}

#pragma endregion

/**
 *
 *  rct2: 0x006ED990
 */
void set_cursor(uint8 cursor_id)
{
    if (_inputState == INPUT_STATE_RESIZING)
    {
        cursor_id = CURSOR_DIAGONAL_ARROWS;
    }
    context_setcurrentcursor(cursor_id);
}

/**
 *
 *  rct2: 0x006E876D
 */
void invalidate_scroll()
{
    rct_window * w = window_find_by_number(gPressedWidget.window_classification, gPressedWidget.window_number);
    if (w != nullptr)
    {
        // Reset to basic scroll
        w->scrolls[_currentScrollIndex].flags &= 0xFF11;
        window_invalidate_by_number(gPressedWidget.window_classification, gPressedWidget.window_number);
    }
}

/**
 * rct2: 0x00406C96
 */
void store_mouse_input(sint32 state, sint32 x, sint32 y)
{
    uint32 writeIndex = _mouseInputQueueWriteIndex;
    uint32 nextWriteIndex = (writeIndex + 1) % Util::CountOf(_mouseInputQueue);

    // Check if the queue is full
    if (nextWriteIndex != _mouseInputQueueReadIndex)
    {
        rct_mouse_data * item = &_mouseInputQueue[writeIndex];
        item->x = x;
        item->y = y;
        item->state = state;

        _mouseInputQueueWriteIndex = nextWriteIndex;
    }
}

void game_handle_edge_scroll()
{
    rct_window * mainWindow;
    sint32 scrollX, scrollY;

    mainWindow = window_get_main();
    if (mainWindow == nullptr)
        return;
    if ((mainWindow->flags & WF_NO_SCROLLING) || (gScreenFlags & (SCREEN_FLAGS_TRACK_MANAGER | SCREEN_FLAGS_TITLE_DEMO)))
        return;
    if (mainWindow->viewport == nullptr)
        return;
    if (!context_has_focus())
        return;

    scrollX = 0;
    scrollY = 0;

    // Scroll left / right
    const CursorState * cursorState = context_get_cursor_state();
    if (cursorState->x == 0)
        scrollX = -1;
    else if (cursorState->x >= context_get_width() - 1)
        scrollX = 1;

    // Scroll up / down
    if (cursorState->y == 0)
        scrollY = -1;
    else if (cursorState->y >= context_get_height() - 1)
        scrollY = 1;

    input_scroll_viewport(scrollX, scrollY);
}

bool input_test_place_object_modifier(PLACE_OBJECT_MODIFIER modifier)
{
    return gInputPlaceObjectModifier & modifier;
}

void input_scroll_viewport(sint32 scrollX, sint32 scrollY)
{
    rct_window * mainWindow = window_get_main();
    rct_viewport * viewport = mainWindow->viewport;

    const sint32 speed = gConfigGeneral.edge_scrolling_speed;

    sint32 dx = scrollX * (speed << viewport->zoom);
    sint32 dy = scrollY * (speed << viewport->zoom);

    if (scrollX != 0)
    {
        // Speed up scrolling horizontally when at the edge of the map
        // so that the speed is consistent with vertical edge scrolling.
        sint32 x = mainWindow->saved_view_x + viewport->view_width / 2 + dx;
        sint32 y = mainWindow->saved_view_y + viewport->view_height / 2;
        sint32 y_dy = mainWindow->saved_view_y + viewport->view_height / 2 + dy;
        LocationXY16 mapCoord, mapCoord_dy;

        mapCoord = viewport_coord_to_map_coord(x, y, 0);
        mapCoord_dy = viewport_coord_to_map_coord(x, y_dy, 0);

        // Check if we're crossing the boundary
        // Clamp to the map minimum value
        sint32 at_map_edge = 0;
        sint32 at_map_edge_dy = 0;
        if (mapCoord.x < MAP_MINIMUM_X_Y || mapCoord.y < MAP_MINIMUM_X_Y)
        {
            at_map_edge = 1;
        }
        if (mapCoord_dy.x < MAP_MINIMUM_X_Y || mapCoord_dy.y < MAP_MINIMUM_X_Y)
        {
            at_map_edge_dy = 1;
        }

        // Clamp to the map maximum value (scenario specific)
        if (mapCoord.x > gMapSizeMinus2 || mapCoord.y > gMapSizeMinus2)
        {
            at_map_edge = 1;
        }
        if (mapCoord_dy.x > gMapSizeMinus2 || mapCoord_dy.y > gMapSizeMinus2)
        {
            at_map_edge_dy = 1;
        }

        // If we crossed the boundary, multiply the distance by 2
        if (at_map_edge && at_map_edge_dy)
        {
            dx *= 2;
        }

        mainWindow->saved_view_x += dx;
        _inputFlags |= INPUT_FLAG_VIEWPORT_SCROLLING;
    }
    if (scrollY != 0)
    {
        mainWindow->saved_view_y += dy;
        _inputFlags |= INPUT_FLAG_VIEWPORT_SCROLLING;
    }
}
