#include "gui2_textentry.h"
#include "theme.h"
#include "clipboard.h"

#ifdef __EMSCRIPTEN__
#include <cstdint>
#include <cstdlib>
#include <emscripten.h>

EM_JS(void, ee_browser_text_input_show, (intptr_t handle, int x, int y, int w, int h, const char* text_ptr, int multiline, int password, int text_size, int select_all, int cursor_start, int cursor_end), {
    const text = text_ptr ? UTF8ToString(text_ptr) : "";
    const rootId = "ee-canvas-text-input-root";
    let root = document.getElementById(rootId);
    let input = root ? root.firstChild : null;
    if (!root) {
        root = document.createElement("div");
        root.id = rootId;
        root.style.position = "fixed";
        root.style.left = "0";
        root.style.top = "0";
        root.style.zIndex = "30";
        root.style.display = "none";
        root.style.pointerEvents = "none";
        document.body.appendChild(root);
    }
    const desiredTag = multiline ? "textarea" : "input";
    if (!input || input.tagName.toLowerCase() !== desiredTag) {
        root.textContent = "";
        input = document.createElement(desiredTag);
        input.spellcheck = false;
        input.autocomplete = "off";
        input.autocorrect = "off";
        input.autocapitalize = "off";
        input.enterKeyHint = multiline ? "default" : "done";
        input.style.boxSizing = "border-box";
        input.style.width = "100%";
        input.style.height = "100%";
        input.style.padding = "0 12px";
        input.style.margin = "0";
        input.style.border = "1px solid rgba(255, 255, 255, 0.65)";
        input.style.borderRadius = "6px";
        input.style.background = "rgba(7, 11, 18, 0.94)";
        input.style.color = "#ffffff";
        input.style.outline = "none";
        input.style.pointerEvents = "auto";
        input.style.fontFamily = "'Helvetica Neue', Helvetica, Arial, sans-serif";
        input.style.lineHeight = "1.2";
        input.style.boxShadow = "0 0 0 1px rgba(120, 189, 255, 0.15)";
        if (desiredTag === "textarea") {
            input.style.padding = "10px 12px";
            input.style.resize = "none";
        }
        const stop = (event) => {
            event.stopPropagation();
        };
        for (const eventName of ["pointerdown", "pointerup", "mousedown", "mouseup", "touchstart", "touchend", "click"]) {
            input.addEventListener(eventName, stop, true);
        }
        input.addEventListener("input", () => {
            const state = window.EmptyEpsilonCanvasTextInput;
            if (!state || state.handle !== handle) {
                return;
            }
            state.value = input.value;
            state.dirty = true;
        });
        input.addEventListener("keydown", (event) => {
            const state = window.EmptyEpsilonCanvasTextInput;
            if (!state || state.handle !== handle) {
                return;
            }
            event.stopPropagation();
            if (!multiline && event.key === "Enter") {
                event.preventDefault();
                state.value = input.value;
                state.dirty = true;
                state.submitted = true;
            }
        }, true);
        input.addEventListener("keyup", stop, true);
        root.appendChild(input);
    }

    if (desiredTag === "input") {
        input.type = password ? "password" : "text";
    }

    const canvas = Module["canvas"];
    if (!canvas) {
        return;
    }
    const bounds = canvas.getBoundingClientRect();
    const scaleX = canvas.width ? bounds.width / canvas.width : 1;
    const scaleY = canvas.height ? bounds.height / canvas.height : 1;

    root.style.display = "block";
    root.style.left = (bounds.left + x * scaleX) + "px";
    root.style.top = (bounds.top + y * scaleY) + "px";
    root.style.width = Math.max(32, w * scaleX) + "px";
    root.style.height = Math.max(32, h * scaleY) + "px";

    input.style.fontSize = Math.max(16, text_size * scaleY * 0.8) + "px";
    if (input.value !== text) {
        input.value = text;
    }

    window.EmptyEpsilonCanvasTextInput = {
        handle,
        value: input.value,
        dirty: false,
        submitted: false
    };

    input.focus({ preventScroll: true });
    const start = Math.max(0, cursor_start);
    const end = Math.max(start, cursor_end);
    if (select_all) {
        input.select();
    } else if (typeof input.setSelectionRange === "function") {
        input.setSelectionRange(start, end);
    }
});

EM_JS(void, ee_browser_text_input_sync, (intptr_t handle, int x, int y, int w, int h, const char* text_ptr, int text_size, int cursor_start, int cursor_end), {
    const state = window.EmptyEpsilonCanvasTextInput;
    const root = document.getElementById("ee-canvas-text-input-root");
    const input = root ? root.firstChild : null;
    if (!state || state.handle !== handle || !root || !input) {
        return;
    }

    const canvas = Module["canvas"];
    if (!canvas) {
        return;
    }
    const bounds = canvas.getBoundingClientRect();
    const scaleX = canvas.width ? bounds.width / canvas.width : 1;
    const scaleY = canvas.height ? bounds.height / canvas.height : 1;

    root.style.left = (bounds.left + x * scaleX) + "px";
    root.style.top = (bounds.top + y * scaleY) + "px";
    root.style.width = Math.max(32, w * scaleX) + "px";
    root.style.height = Math.max(32, h * scaleY) + "px";
    input.style.fontSize = Math.max(16, text_size * scaleY * 0.8) + "px";

    const text = text_ptr ? UTF8ToString(text_ptr) : "";
    if (input.value !== text) {
        const active = document.activeElement === input;
        input.value = text;
        state.value = text;
        if (active && typeof input.setSelectionRange === "function") {
            const start = Math.max(0, cursor_start);
            const end = Math.max(start, cursor_end);
            input.setSelectionRange(start, end);
        }
    }
});

EM_JS(int, ee_browser_text_input_poll_flags, (intptr_t handle), {
    const state = window.EmptyEpsilonCanvasTextInput;
    if (!state || state.handle !== handle) {
        return 0;
    }
    let flags = 0;
    if (state.dirty) {
        flags |= 1;
        state.dirty = false;
    }
    if (state.submitted) {
        flags |= 2;
        state.submitted = false;
    }
    return flags;
});

EM_JS(char*, ee_browser_text_input_consume_value, (intptr_t handle), {
    const state = window.EmptyEpsilonCanvasTextInput;
    if (!state || state.handle !== handle) {
        return 0;
    }
    const value = state.value || "";
    const length = lengthBytesUTF8(value) + 1;
    const result = _malloc(length);
    stringToUTF8(value, result, length);
    return result;
});

EM_JS(void, ee_browser_text_input_hide, (intptr_t handle), {
    const state = window.EmptyEpsilonCanvasTextInput;
    if (!state || state.handle !== handle) {
        return;
    }
    const root = document.getElementById("ee-canvas-text-input-root");
    const input = root ? root.firstChild : null;
    if (input && document.activeElement === input) {
        input.blur();
    }
    if (root) {
        root.style.display = "none";
    }
    window.EmptyEpsilonCanvasTextInput = null;
});
#endif


GuiTextEntry::GuiTextEntry(GuiContainer* owner, string id, string text)
: GuiElement(owner, id), text(text), text_size(30), func(nullptr)
{
    blink_timer.repeat(blink_rate);
    front_style = theme->getStyle("textentry.front");
    back_style = theme->getStyle("textentry.back");
}

GuiTextEntry::~GuiTextEntry()
{
#ifdef __EMSCRIPTEN__
    ee_browser_text_input_hide(reinterpret_cast<intptr_t>(this));
#endif
    if (focus)
        SDL_StopTextInput();
}

void GuiTextEntry::onUpdate()
{
#ifdef __EMSCRIPTEN__
    if (!focus) {
        return;
    }

    syncBrowserTextInput();

    const auto flags = ee_browser_text_input_poll_flags(reinterpret_cast<intptr_t>(this));
    if (flags & 1) {
        char* browser_text = ee_browser_text_input_consume_value(reinterpret_cast<intptr_t>(this));
        if (browser_text) {
            applyBrowserText(browser_text);
            free(browser_text);
        }
        return;
    }
    if (flags & 2) {
        browser_sync_dirty = true;
        onTextInput(sp::TextInputEvent::Return);
        return;
    }
#endif
}

float GuiTextEntry::getLineSpacing() const {
    const auto& front = front_style->get(getState());
    return front.font->getLineSpacing(32) * text_size / float(32);
}

void GuiTextEntry::onDraw(sp::RenderTarget& renderer)
{
    const auto& back = back_style->get(getState());
    const auto& front = front_style->get(getState());

    if (!back.texture.empty())
        renderer.drawStretchedHV(rect, back.size, back.texture, back.color);
    if (blink_timer.isExpired())
        typing_indicator = !typing_indicator;

    std::string shown_text = text;
    if (hide_password) {
        shown_text = std::string(text.size(), '*');
    }
    if (shown_text.empty()) shown_text = " ";
    sp::Rect text_rect(rect.position.x + 16, rect.position.y, rect.size.x - 32, rect.size.y);
    auto prepared = front.font->prepare(shown_text, 32, text_size, {255,255,255,255}, text_rect.size, multiline ? sp::Alignment::TopLeft : sp::Alignment::CenterLeft, sp::Font::FlagClip);
    auto linespacing = front.font->getLineSpacing(32) * text_size / float(32);

    if (multiline) {
        // ensure the text fills the available space as much as possible
        auto min_y = std::numeric_limits<float>::infinity();
        auto max_y = -min_y;
        for(auto& d : prepared.data) {
            if (d.position.y < min_y)
                min_y = d.position.y;
            if (d.position.y > max_y)
                max_y = d.position.y;
        }
        auto clipped_from_top = -min_y - render_offset.y;
        auto space_at_bottom = rect.size.y - max_y - render_offset.y - linespacing * 0.3f;

        if (space_at_bottom > 0 && clipped_from_top > 0) {
            // the text goes off the top of the box but doesn't reach the bottom;
            // scroll until it either stops going off the top, or reaches the bottom
            render_offset.y += std::min(space_at_bottom, clipped_from_top);
        }
    }

    for(auto& d : prepared.data)
        d.position += render_offset;

    float start_x = -1;
    int selection_min = std::min(selection_start, selection_end);
    int selection_max = std::max(selection_start, selection_end);
    for(auto d : prepared.data)
    {
        if (d.string_offset == selection_end)
        {
            if (d.position.x > text_rect.size.x)
                render_offset.x -= d.position.x - text_rect.size.x;
            if (d.position.x < 0.0f)
                render_offset.x -= d.position.x;
            if (multiline && d.position.y > text_rect.size.y - linespacing * 0.3f)
                render_offset.y -= d.position.y - text_rect.size.y + linespacing * 0.3f;
            if (multiline && d.position.y < linespacing)
                render_offset.y -= d.position.y - linespacing;
        }
        if (d.string_offset == selection_min)
        {
            start_x = d.position.x;
        }
        if (focus) {
            if ((d.string_offset == selection_max) || (d.char_code == 0 && start_x > -1.0f))
            {
                float end_x = d.position.x;
                float start_y = d.position.y - text_size;
                float end_y = start_y + text_size * 1.1f;
                if (end_y < 0.0f)
                    continue;
                if (start_y > text_rect.size.y)
                    continue;
                start_y = std::max(0.0f, start_y);
                end_x = std::min(text_rect.size.x, end_x);
                end_y = std::min(text_rect.size.y, end_y);
                if (end_x != start_x)
                {
                    renderer.fillRect(
                        sp::Rect(rect.position + glm::vec2{start_x + 16, start_y},
                        glm::vec2{end_x - start_x, end_y - start_y}),
                        {255, 255, 255, 128});
                }
                if (d.string_offset == selection_max)
                    start_x = -1.0f;
                else
                    start_x = 0.0f;
            }
            if (d.string_offset == selection_end && typing_indicator)
            {
                float start_y = d.position.y - text_size;
                float end_y = start_y + text_size * 1.1f;
                if (end_y < 0.0f)
                    continue;
                if (start_y > text_rect.size.y)
                    continue;
                start_y = std::max(0.0f, start_y);
                end_y = std::min(text_rect.size.y, end_y);

                renderer.fillRect(
                    sp::Rect(rect.position + glm::vec2{d.position.x + 16 - text_size * 0.05f, start_y},
                    glm::vec2{text_size * 0.1f, end_y - start_y}),
                    {255, 255, 255, 255});
            }
        }
    }
    renderer.drawText(text_rect, prepared, sp::Font::FlagClip);
}

bool GuiTextEntry::onMouseDown(sp::io::Pointer::Button button, glm::vec2 position, sp::io::Pointer::ID id)
{
    selection_start = getTextOffsetForPosition(position);
    selection_end = selection_start;
    return true;
}

void GuiTextEntry::onMouseDrag(glm::vec2 position, sp::io::Pointer::ID id)
{
    selection_end = getTextOffsetForPosition(position);
}

void GuiTextEntry::onTextInput(const string& text)
{
    if (readonly)
        return;
    if (blink_timer.isRunning()) {
        typing_indicator = true;
        blink_timer.repeat(blink_rate);
    }
    this->text = this->text.substr(0, std::min(selection_start, selection_end)) + text + this->text.substr(std::max(selection_start, selection_end));
    selection_end = selection_start = std::min(selection_start, selection_end) + text.length();
#ifdef __EMSCRIPTEN__
    browser_sync_dirty = true;
#endif
    runChangeCallback();
}

void GuiTextEntry::onTextInput(sp::TextInputEvent e)
{
    if (blink_timer.isRunning()) {
        typing_indicator = true;
        blink_timer.repeat(blink_rate);
    }
    switch(e)
    {
    case sp::TextInputEvent::Left:
    case sp::TextInputEvent::LeftWithSelection:
        if (selection_end > 0)
            selection_end -= 1;
        if (e != sp::TextInputEvent::LeftWithSelection)
            selection_start = selection_end;
        break;
    case sp::TextInputEvent::Right:
    case sp::TextInputEvent::RightWithSelection:
        if (selection_end < int(text.length()))
            selection_end += 1;
        if (e != sp::TextInputEvent::RightWithSelection)
            selection_start = selection_end;
        break;
    case sp::TextInputEvent::WordLeft:
    case sp::TextInputEvent::WordLeftWithSelection:
        if (selection_end > 0)
            selection_end -= 1;
        while (selection_end > 0 && !isspace(text[selection_end - 1]))
            selection_end -= 1;
        if (e != sp::TextInputEvent::WordLeftWithSelection)
            selection_start = selection_end;
        break;
    case sp::TextInputEvent::WordRight:
    case sp::TextInputEvent::WordRightWithSelection:
        while (selection_end < int(text.length()) && !isspace(text[selection_end]))
            selection_end += 1;
        if (selection_end < int(text.length()))
            selection_end += 1;
        if (e != sp::TextInputEvent::WordRightWithSelection)
            selection_start = selection_end;
        break;
    case sp::TextInputEvent::Up:
    case sp::TextInputEvent::UpWithSelection:{
        if (up_func)
        {
            up_func(text);
            return;
        }
        int end_of_line = text.substr(0, selection_end).rfind("\n");
        if (end_of_line < 0)
            return;
        int start_of_line = text.substr(0, end_of_line).rfind("\n") + 1;
        int offset = selection_end - end_of_line - 1;
        int line_length = end_of_line - start_of_line;
        selection_end = start_of_line + std::min(line_length, offset);
        if (e != sp::TextInputEvent::UpWithSelection)
            selection_start = selection_end;
        }break;
    case sp::TextInputEvent::Down:
    case sp::TextInputEvent::DownWithSelection:{
        if (down_func)
        {
            down_func(text);
            return;
        }
        int start_of_current_line = text.substr(0, selection_end).rfind("\n") + 1;
        int end_of_current_line = text.find("\n", selection_end);
        if (end_of_current_line < 0)
            return;
        int end_of_end_line = text.find("\n", end_of_current_line + 1);
        if (end_of_end_line == -1)
            end_of_end_line = text.length();
        int offset = selection_end - start_of_current_line;
        selection_end = end_of_current_line + 1 + std::min(offset, end_of_end_line - (end_of_current_line + 1));
        if (e != sp::TextInputEvent::DownWithSelection)
            selection_start = selection_end;
        }break;
    case sp::TextInputEvent::LineStart:
    case sp::TextInputEvent::LineStartWithSelection:
        selection_end = text.substr(0, selection_end).rfind("\n") + 1;
        if (e != sp::TextInputEvent::LineStartWithSelection)
            selection_start = selection_end;
        break;
    case sp::TextInputEvent::LineEnd:
    case sp::TextInputEvent::LineEndWithSelection:
        selection_end = text.find("\n", selection_start);
        if (selection_end == -1)
            selection_end = text.length();
        if (e != sp::TextInputEvent::LineEndWithSelection)
            selection_start = selection_end;
        break;
    case sp::TextInputEvent::TextStart:
    case sp::TextInputEvent::TextStartWithSelection:
        selection_end = 0;
        if (e != sp::TextInputEvent::TextStartWithSelection)
            selection_start = selection_end;
        break;
    case sp::TextInputEvent::TextEnd:
    case sp::TextInputEvent::TextEndWithSelection:
        selection_end = text.length();
        if (e != sp::TextInputEvent::TextEndWithSelection)
            selection_start = selection_end;
        break;
    case sp::TextInputEvent::SelectAll:
        selection_end = 0;
        selection_start = text.length();
        break;
    case sp::TextInputEvent::Delete:
        if (readonly)
            return;
        if (selection_start != selection_end)
            text = text.substr(0, std::min(selection_start, selection_end)) + text.substr(std::max(selection_start, selection_end));
        else
            text = text.substr(0, selection_start) + text.substr(selection_start + 1);
        selection_start = selection_end = std::min(selection_start, selection_end);
#ifdef __EMSCRIPTEN__
        browser_sync_dirty = true;
#endif
        runChangeCallback();
        break;
    case sp::TextInputEvent::Backspace:
        if (readonly)
            return;
        if (selection_start != selection_end)
        {
            onTextInput(sp::TextInputEvent::Delete);
            return;
        }
        else if (selection_start > 0)
        {
            text = text.substr(0, selection_start - 1) + text.substr(selection_start);
            selection_start -= 1;
            selection_end = selection_start;
#ifdef __EMSCRIPTEN__
            browser_sync_dirty = true;
#endif
            runChangeCallback();
        }
        break;
    case sp::TextInputEvent::Indent:
        if (readonly)
            return;
        if (selection_start == selection_end)
        {
            int start_of_line = text.substr(0, selection_end).rfind("\n") + 1;
            int offset = selection_end - start_of_line;
            int add = 4 - (offset % 4);
            onTextInput(string(" ") * add);
        }
        else
        {
            int start_of_line = text.substr(0, std::min(selection_start, selection_end)).rfind("\n") + 1;
            auto data = text.substr(start_of_line, std::max(selection_start, selection_end));
            data = "    " + data.replace("\n", "\n    ");
            int extra_length = data.length() - (std::max(selection_start, selection_end) - start_of_line) - 4;
            text = text.substr(0, start_of_line) + data + text.substr(std::max(selection_start, selection_end));

            if (start_of_line != selection_start)
                selection_start += 4;
            if (start_of_line != selection_end)
                selection_end += 4;
            if (selection_start > selection_end)
                selection_start += extra_length;
            else
                selection_end += extra_length;
#ifdef __EMSCRIPTEN__
            browser_sync_dirty = true;
#endif
            runChangeCallback();
        }
        break;
    case sp::TextInputEvent::Unindent:
        if (readonly)
            return;
        if (selection_start == selection_end)
        {
        }
        else
        {
            int start_of_line = text.substr(0, std::min(selection_start, selection_end)).rfind("\n") + 1;
            auto data = text.substr(start_of_line, std::max(selection_start, selection_end));
            for(int n=0; n<4; n++)
            {
                if (data.startswith(" "))
                    data = data.substr(1);
                data = data.replace("\n ", "\n");
            }
            int removed_length = (std::max(selection_start, selection_end) - start_of_line) - data.length();
            text = text.substr(0, start_of_line) + data + text.substr(std::max(selection_start, selection_end));

            if (selection_start > selection_end)
                selection_start -= removed_length;
            else
                selection_end -= removed_length;
#ifdef __EMSCRIPTEN__
            browser_sync_dirty = true;
#endif
            runChangeCallback();
        }
        break;
    case sp::TextInputEvent::Return:
        if (readonly)
            return;
        if (multiline)
        {
            onTextInput("\n");
        }
        else if (enter_func)
        {
            auto f = enter_func;
            f(text);
        }
        break;
    case sp::TextInputEvent::Copy:
        Clipboard::setClipboard(text.substr(std::min(selection_start, selection_end), std::max(selection_start, selection_end)));
        break;
    case sp::TextInputEvent::Paste:
        if (readonly)
            return;
        onTextInput(Clipboard::readClipboard());
        break;
    case sp::TextInputEvent::Cut:
        Clipboard::setClipboard(text.substr(std::min(selection_start, selection_end), std::max(selection_start, selection_end)));
        if (readonly)
            return;
        if (selection_start != selection_end)
            onTextInput(sp::TextInputEvent::Delete);
        break;
    }
}

void GuiTextEntry::onFocusGained()
{
    if (select_on_focus) {
		selection_end = 0;
		selection_start = text.length();
    }
    typing_indicator = true;
    blink_timer.repeat(blink_rate);
#ifdef __EMSCRIPTEN__
    browser_sync_dirty = true;
    syncBrowserTextInput();
#else
    SDL_StartTextInput();
#endif
}

void GuiTextEntry::onFocusLost()
{
#ifdef __EMSCRIPTEN__
    ee_browser_text_input_hide(reinterpret_cast<intptr_t>(this));
    browser_synced_rect = sp::Rect{0, 0, 0, 0};
    browser_sync_dirty = true;
#endif
    SDL_StopTextInput();
}

void GuiTextEntry::setAttribute(const string& key, const string& value)
{
    if (key == "style") {
        front_style = theme->getStyle(value + ".front");
        back_style = theme->getStyle(value + ".back");
    } else if (key == "readonly") {
        readonly = value.toBool();
    } else {
        GuiElement::setAttribute(key, value);
    }
}

string GuiTextEntry::getText() const
{
    return text;
}

GuiTextEntry* GuiTextEntry::setText(string text)
{
    this->text = text;
    selection_start = std::min(selection_start, int(text.length()));
    selection_end = std::min(selection_end, int(text.length()));
#ifdef __EMSCRIPTEN__
    if (!browser_applying_text) {
        browser_sync_dirty = true;
    }
#endif
    return this;
}

GuiTextEntry* GuiTextEntry::setTextSize(float size)
{
    this->text_size = size;
    return this;
}

GuiTextEntry* GuiTextEntry::setMultiline(bool enabled)
{
    multiline = enabled;
    return this;
}

GuiTextEntry* GuiTextEntry::setSelectOnFocus(bool enabled)
{
    select_on_focus = enabled;
    return this;
}

GuiTextEntry* GuiTextEntry::setHidePassword(bool enabled)
{
    hide_password = enabled;
    return this;
}

GuiTextEntry* GuiTextEntry::callback(func_t func)
{
    this->func = func;
    return this;
}

GuiTextEntry* GuiTextEntry::enterCallback(func_t func)
{
    this->enter_func = func;
    return this;
}

GuiTextEntry* GuiTextEntry::upCallback(func_t func)
{
    this->up_func = func;
    return this;
}

GuiTextEntry* GuiTextEntry::downCallback(func_t func)
{
    this->down_func = func;
    return this;
}

void GuiTextEntry::setCursorPosition(int offset)
{
    selection_start = selection_end = std::clamp(offset, 0, int(text.size()));
#ifdef __EMSCRIPTEN__
    browser_sync_dirty = true;
#endif
}

int GuiTextEntry::getTextOffsetForPosition(glm::vec2 position)
{
    position = position - rect.position - render_offset;
    position.x -= 16.0f;
    int result = text.size();
    const auto& front = front_style->get(getState());
    //if (vertical_scroll)
    //    position.y -= vertical_scroll->getValue();
    std::string shown_text = text;
    if (hide_password) {
        shown_text = std::string(text.size(), '*');
    }
    auto pfs = front.font->prepare(shown_text, 32, text_size, {255,255,255,255}, rect.size - glm::vec2(32, 0), multiline ? sp::Alignment::TopLeft : sp::Alignment::CenterLeft);
    unsigned int n;
    for(n=0; n<pfs.data.size(); n++)
    {
        auto& d = pfs.data[n];
        if (d.position.y > position.y)
            break;
    }
    if (n == pfs.data.size())
    {
        return text.size();
    }
    float line_y = pfs.data[n].position.y;
    for(; n<pfs.data.size(); n++)
    {
        auto& d = pfs.data[n];
        if (d.position.x > position.x)
            break;
        if (d.position.y > line_y)
            break;
        result = d.string_offset;
    }

    return result;
}

void GuiTextEntry::runChangeCallback()
{
    if (func)
    {
        func_t f = func;
        f(text);
    }
}

#ifdef __EMSCRIPTEN__
void GuiTextEntry::syncBrowserTextInput()
{
    const auto current_rect = getRect();
    const auto current_cursor_start = std::min(selection_start, selection_end);
    const auto current_cursor_end = std::max(selection_start, selection_end);
    if (
        !browser_sync_dirty &&
        browser_synced_text == text &&
        browser_synced_rect.position.x == current_rect.position.x &&
        browser_synced_rect.position.y == current_rect.position.y &&
        browser_synced_rect.size.x == current_rect.size.x &&
        browser_synced_rect.size.y == current_rect.size.y &&
        browser_synced_cursor == current_cursor_end
    ) {
        return;
    }

    const auto handle = reinterpret_cast<intptr_t>(this);
    if (browser_synced_rect.size.x <= 0 || browser_synced_rect.size.y <= 0) {
        ee_browser_text_input_show(
            handle,
            int(current_rect.position.x),
            int(current_rect.position.y),
            int(current_rect.size.x),
            int(current_rect.size.y),
            text.c_str(),
            multiline ? 1 : 0,
            hide_password ? 1 : 0,
            int(text_size),
            select_on_focus ? 1 : 0,
            current_cursor_start,
            current_cursor_end
        );
    } else {
        ee_browser_text_input_sync(
            handle,
            int(current_rect.position.x),
            int(current_rect.position.y),
            int(current_rect.size.x),
            int(current_rect.size.y),
            text.c_str(),
            int(text_size),
            current_cursor_start,
            current_cursor_end
        );
    }

    browser_synced_text = text;
    browser_synced_rect = current_rect;
    browser_synced_cursor = current_cursor_end;
    browser_sync_dirty = false;
}

void GuiTextEntry::applyBrowserText(string next_text)
{
    browser_applying_text = true;
    text = next_text;
    selection_start = int(text.length());
    selection_end = selection_start;
    browser_synced_text = text;
    browser_synced_cursor = selection_end;
    browser_applying_text = false;
    runChangeCallback();
}
#endif
