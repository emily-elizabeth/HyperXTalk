/* Copyright (C) 2003-2015 LiveCode Ltd.

This file is part of LiveCode.

LiveCode is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with LiveCode.  If not see <http://www.gnu.org/licenses/>.  */

// fieldmarkdown.cpp
// Import and export of CommonMark-subset Markdown for MCField.
//
// Supported on import (setter):
//   Block: ATX headings (#–######), unordered lists (-, *, +), ordered lists
//          (N.), fenced code blocks (``` or ~~~), indented code blocks (4sp/tab),
//          blockquotes (>), horizontal rules (---/***/___ → blank paragraph),
//          setext headings (== / -- underlines), soft line breaks (trailing ••)
//   Inline: ***bold-italic***, **bold**, *italic*, `code span`, [text](url),
//           backslash escapes
//
// Supported on export (getter):
//   Lists → - / N., headings (detected by paragraph metadata set at import,
//   or by character style text-size), blockquote indent → > prefix,
//   bold → **, italic → *, bold+italic → ***, code span font → `...`,
//   links → [text](url)
//
// Lossy: field styles not expressible in Markdown (colours, mixed fonts within
// a run, etc.) are silently dropped on export, matching the behaviour of rtfText.

#include "prefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"

#include "field.h"
#include "paragraf.h"
#include "text.h"
#include "osspec.h"

#include "mcstring.h"
#include "uidc.h"
#include "globals.h"
#include "util.h"

////////////////////////////////////////////////////////////////////////////////
// Helpers shared between import and export
////////////////////////////////////////////////////////////////////////////////

// Map heading level (1–6) to point size, matching the HTML importer.
static uint16_t s_md_heading_sizes[6] = { 34, 24, 18, 14, 12, 10 };

// Return heading level (1–6) for a known heading point size, or 0 if not a heading.
static uint32_t md_size_to_heading_level(uint16_t p_size)
{
    for (uint32_t i = 0; i < 6; i++)
        if (s_md_heading_sizes[i] == p_size)
            return i + 1;
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// IMPORTER
////////////////////////////////////////////////////////////////////////////////

struct import_md_t
{
    MCField        *field;
    MCParagraph    *paragraphs;     // circular list being built
    bool            need_paragraph; // true when we need a new paragraph before the next block

    // Style for the paragraph currently being assembled.
    MCFieldParagraphStyle para_style;
    bool                  has_para_style;

    // Style for the current character run.
    MCFieldCharacterStyle char_style;

    // Raw byte buffer for the current run.
    // When is_unicode is false: native (single-byte) encoding.
    // When is_unicode is true:  UTF-16 (two bytes per code unit).
    uint8_t  *bytes;
    uint32_t  byte_count;
    uint32_t  byte_capacity;
    bool      is_unicode;

    // Fenced code-block state.
    bool     in_fenced_code;
    char     fence_char;
    uint32_t fence_len;
};

// Forward declarations (these functions are mutually dependent).
static void import_md_flush(import_md_t& ctxt);

static bool import_md_ensure_bytes(import_md_t& ctxt, uint32_t p_amount)
{
    if (ctxt.byte_count + p_amount > ctxt.byte_capacity)
    {
        uint32_t t_new = (ctxt.byte_count + p_amount + 4096u) & ~4095u;
        if (!MCMemoryResizeArray(t_new, ctxt.bytes, ctxt.byte_capacity))
            return false;
    }
    return true;
}

static void import_md_append_bytes(import_md_t& ctxt, const char *p, uint32_t len)
{
    if (import_md_ensure_bytes(ctxt, len))
    {
        memcpy(ctxt.bytes + ctxt.byte_count, p, len);
        ctxt.byte_count += len;
    }
}

// Append a single UTF-16 code unit.  Flushes any pending native bytes first.
static void import_md_append_unicode_char(import_md_t& ctxt, unichar_t p_char)
{
    if (!ctxt.is_unicode && ctxt.byte_count > 0)
        import_md_flush(ctxt);
    if (!import_md_ensure_bytes(ctxt, 2))
        return;
    ctxt.is_unicode = true;
    memcpy(ctxt.bytes + ctxt.byte_count, &p_char, 2);
    ctxt.byte_count += 2;
}

// Append a UTF-8 byte sequence, decoding multi-byte characters to UTF-16.
// ASCII bytes are kept as native (single-byte); non-ASCII bytes are decoded
// from UTF-8 and stored as UTF-16, flushing between mode switches.
static void import_md_append_utf8(import_md_t& ctxt, const char *p, uint32_t len)
{
    while (len > 0)
    {
        // Collect a run of ASCII bytes (high bit clear).
        const char *q = p;
        while (len > 0 && (unsigned char)*q < 128) { q++; len--; }
        if (q > p)
        {
            // Flush if we were in unicode mode before writing native bytes.
            if (ctxt.is_unicode && ctxt.byte_count > 0)
                import_md_flush(ctxt);
            import_md_append_bytes(ctxt, p, (uint32_t)(q - p));
        }
        p = q;

        // Collect a run of non-ASCII bytes (high bit set = multi-byte UTF-8).
        while (len > 0 && (unsigned char)*q >= 128) { q++; len--; }
        if (q > p)
        {
            // Decode the UTF-8 sequence to a temporary MCStringRef (UTF-16).
            MCAutoStringRef t_str;
            /* UNCHECKED */ MCStringCreateWithBytes((const byte_t *)p, (uint32_t)(q - p),
                                                    kMCStringEncodingUTF8, false, &t_str);
            uindex_t t_len = MCStringGetLength(*t_str);
            const unichar_t *t_chars = MCStringGetCharPtr(*t_str);
            for (uindex_t i = 0; i < t_len; i++)
                import_md_append_unicode_char(ctxt, t_chars[i]);
        }
        p = q;
    }
}

static void import_md_flush(import_md_t& ctxt)
{
    if (ctxt.byte_count == 0)
        return;

    if (ctxt.need_paragraph)
    {
        ctxt.field->importparagraph(ctxt.paragraphs, ctxt.has_para_style ? &ctxt.para_style : nil);
        ctxt.need_paragraph = false;
    }

    ctxt.field->importblock(ctxt.paragraphs->prev(), ctxt.char_style,
                             ctxt.bytes, ctxt.byte_count, ctxt.is_unicode);
    ctxt.byte_count = 0;
    ctxt.is_unicode = false;
}

static void import_md_end_paragraph(import_md_t& ctxt)
{
    import_md_flush(ctxt);
    // Ensure a paragraph exists even if we flushed nothing (empty lines).
    if (!ctxt.need_paragraph)
        ctxt.need_paragraph = true;
}

static void import_md_begin_paragraph(import_md_t& ctxt,
                                       const MCFieldParagraphStyle *p_para,
                                       const MCFieldCharacterStyle *p_char)
{
    import_md_end_paragraph(ctxt);

    if (p_para != nil)
    {
        ctxt.para_style     = *p_para;
        ctxt.has_para_style = true;
    }
    else
    {
        memset(&ctxt.para_style, 0, sizeof(ctxt.para_style));
        ctxt.has_para_style = false;
    }

    if (p_char != nil)
        ctxt.char_style = *p_char;
    else
        memset(&ctxt.char_style, 0, sizeof(ctxt.char_style));
}

// Forward declaration – inline() is mutually recursive with itself for nested spans.
static void import_md_inline(import_md_t& ctxt,
                              const char *p, uint32_t len,
                              const MCFieldCharacterStyle& base_style);

// Emit a plain text segment, escaping nothing (raw bytes, native encoding).
static void import_md_emit_plain(import_md_t& ctxt,
                                  const char *p, uint32_t len,
                                  const MCFieldCharacterStyle& style)
{
    if (len == 0)
        return;
    if (ctxt.need_paragraph)
    {
        ctxt.field->importparagraph(ctxt.paragraphs, ctxt.has_para_style ? &ctxt.para_style : nil);
        ctxt.need_paragraph = false;
    }
    import_md_flush(ctxt);
    ctxt.char_style = style;
    import_md_append_utf8(ctxt, p, len);
    import_md_flush(ctxt);
}

// Count repeated occurrences of ch at *p (up to limit).
static uint32_t count_repeat(const char *p, const char *end, char ch)
{
    uint32_t n = 0;
    while (p < end && *p == ch) { n++; p++; }
    return n;
}

// Find the closing delimiter sequence (p_delim, p_delim_len) starting from
// p_search, returning a pointer to its start or nil if not found.
static const char *find_closing(const char *p_search, const char *p_end,
                                 const char *p_delim, uint32_t p_delim_len)
{
    while (p_search + p_delim_len <= p_end)
    {
        if (memcmp(p_search, p_delim, p_delim_len) == 0)
            return p_search;
        p_search++;
    }
    return nil;
}

static void import_md_inline(import_md_t& ctxt,
                              const char *p, uint32_t len,
                              const MCFieldCharacterStyle& base_style)
{
    const char *end = p + len;

    while (p < end)
    {
        char ch = *p;

        // ── Backslash escape ─────────────────────────────────────────────────
        if (ch == '\\' && p + 1 < end)
        {
            char next = p[1];
            if (next == '*' || next == '_' || next == '`' || next == '\\' ||
                next == '[' || next == ']' || next == '(' || next == ')' ||
                next == '#' || next == '+' || next == '-' || next == '.' ||
                next == '!' || next == '{' || next == '}' || next == '<' ||
                next == '>' || next == '~')
            {
                import_md_emit_plain(ctxt, &next, 1, base_style);
                p += 2;
                continue;
            }
        }

        // ── Code span: `...` or ``...`` etc. ────────────────────────────────
        if (ch == '`')
        {
            uint32_t tick_count = count_repeat(p, end, '`');
            const char *content_start = p + tick_count;

            // Search for closing run of same tick count.
            const char *close = nil;
            const char *search = content_start;
            while (search + tick_count <= end)
            {
                if (*search == '`' && count_repeat(search, end, '`') == tick_count)
                {
                    close = search;
                    break;
                }
                search++;
            }

            if (close != nil)
            {
                // Emit code span in Courier.
                MCFieldCharacterStyle code_style = base_style;
                code_style.has_text_font = true;
                { MCAutoStringRef t_font; /* UNCHECKED */ MCStringCreateWithCString("Courier", &t_font); /* UNCHECKED */ MCNameCreate(*t_font, code_style.text_font); }

                // Strip a single leading/trailing space (CommonMark rule).
                uint32_t content_len = (uint32_t)(close - content_start);
                const char *content = content_start;
                if (content_len >= 2 && content[0] == ' ' && content[content_len - 1] == ' ')
                {
                    content++;
                    content_len -= 2;
                }

                import_md_emit_plain(ctxt, content, content_len, code_style);
                MCValueRelease(code_style.text_font);
                p = close + tick_count;
                continue;
            }
            else
            {
                // Not a valid code span; treat backticks as literals.
                import_md_emit_plain(ctxt, p, tick_count, base_style);
                p += tick_count;
                continue;
            }
        }

        // ── Emphasis: *, _ ───────────────────────────────────────────────────
        if (ch == '*' || ch == '_')
        {
            // Determine delimiter run length (up to 3).
            uint32_t delim_len = count_repeat(p, end, ch);
            if (delim_len > 3) delim_len = 3;

            // Try to find a matching closing run, longest first.
            // NOTE: must use signed int so decrement from 1 reaches 0, not UINT32_MAX.
            bool matched = false;
            for (int32_t try_len = (int32_t)delim_len; try_len >= 1 && !matched; try_len--)
            {
                char delim_buf[3] = { ch, ch, ch };
                const char *close = find_closing(p + try_len, end, delim_buf, (uint32_t)try_len);
                if (close == nil)
                    continue;

                // Build span style.
                MCFieldCharacterStyle span_style = base_style;
                span_style.has_text_style = true;
                uint16_t extra = 0;
                if (try_len >= 2) extra |= FA_BOLD;
                if (try_len == 1 || try_len == 3) extra |= FA_ITALIC;
                span_style.text_style = (base_style.has_text_style ? base_style.text_style : 0) | extra;

                // Recurse into span content.
                import_md_inline(ctxt, p + try_len, (uint32_t)(close - p - try_len), span_style);
                p = close + try_len;
                matched = true;
            }

            if (!matched)
            {
                // No closing delimiter found; emit as literal characters.
                uint32_t raw = count_repeat(p, end, ch);
                import_md_emit_plain(ctxt, p, raw, base_style);
                p += raw;
            }
            continue;
        }

        // ── Inline link: [text](url) ─────────────────────────────────────────
        if (ch == '[')
        {
            const char *text_end = p + 1;
            while (text_end < end && *text_end != ']') text_end++;
            if (text_end < end && text_end + 1 < end && text_end[1] == '(')
            {
                const char *url_start = text_end + 2;
                const char *url_end   = url_start;
                while (url_end < end && *url_end != ')') url_end++;
                if (url_end < end)
                {
                    MCFieldCharacterStyle link_style = base_style;
                    link_style.has_link_text = true;
                    /* UNCHECKED */ MCStringCreateWithBytes(
                        (const byte_t *)url_start, (uint32_t)(url_end - url_start),
                        kMCStringEncodingUTF8, false, link_style.link_text);

                    import_md_inline(ctxt, p + 1, (uint32_t)(text_end - p - 1), link_style);
                    MCValueRelease(link_style.link_text);
                    p = url_end + 1;
                    continue;
                }
            }
            // Not a valid link (e.g. "[ ]", "[Topic]" with no following "(url)").
            // Emit '[' as a literal character and advance, otherwise p never moves.
            import_md_emit_plain(ctxt, p, 1, base_style);
            p++;
            continue;
        }

        // ── Regular character(s) ─────────────────────────────────────────────
        // Collect a run of plain characters to batch the emit call.
        const char *run_start = p;
        while (p < end && *p != '\\' && *p != '`' && *p != '*' && *p != '_' && *p != '[')
            p++;
        if (p > run_start)
            import_md_emit_plain(ctxt, run_start, (uint32_t)(p - run_start), base_style);
    }
}

// Emit a complete paragraph from a line range with the given paragraph and
// character base styles.
static void import_md_paragraph(import_md_t& ctxt,
                                  const char *content, uint32_t content_len,
                                  const MCFieldParagraphStyle *para_style,
                                  const MCFieldCharacterStyle *char_style)
{
    import_md_begin_paragraph(ctxt, para_style, char_style);
    MCFieldCharacterStyle base;
    if (char_style != nil)
        base = *char_style;
    else
        memset(&base, 0, sizeof(base));
    import_md_inline(ctxt, content, content_len, base);
    import_md_end_paragraph(ctxt);
}

// Strip trailing whitespace from a line (in-place via adjusting length).
static uint32_t rtrim(const char *p, uint32_t len)
{
    while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t'))
        len--;
    return len;
}

MCParagraph *MCField::importmarkdowntext(MCStringRef p_text)
{
    // Work in UTF-8 throughout.
    MCAutoPointer<char> t_utf8;
    uindex_t t_utf8_len;
    if (!MCStringConvertToUTF8(p_text, &t_utf8, t_utf8_len))
        return nil;

    import_md_t ctxt;
    memset(&ctxt, 0, sizeof(import_md_t));
    ctxt.field          = this;
    ctxt.need_paragraph = true;

    const char *text = *t_utf8;
    const char *end  = text + t_utf8_len;
    const char *line = text;

    // ── Line-by-line block parser ────────────────────────────────────────────
    while (line < end || (line == end && !ctxt.need_paragraph))
    {
        if (line >= end)
            break;

        // Find end of line.
        const char *line_end = line;
        while (line_end < end && *line_end != '\n' && *line_end != '\r')
            line_end++;

        const char *raw_line     = line;
        uint32_t    raw_line_len = (uint32_t)(line_end - line);
        uint32_t    trimmed_len  = rtrim(line, raw_line_len);

        // Advance line pointer past line ending.
        const char *next_line = line_end;
        if (next_line < end && *next_line == '\r') next_line++;
        if (next_line < end && *next_line == '\n') next_line++;

        // ── Fenced code block ────────────────────────────────────────────────
        if (ctxt.in_fenced_code)
        {
            // Check for closing fence.
            if (trimmed_len >= ctxt.fence_len && raw_line[0] == ctxt.fence_char)
            {
                uint32_t cnt = count_repeat(raw_line, line_end, ctxt.fence_char);
                if (cnt >= ctxt.fence_len)
                {
                    ctxt.in_fenced_code = false;
                    import_md_end_paragraph(ctxt);
                    line = next_line;
                    continue;
                }
            }

            // Inside code block: emit as monospace, no inline parsing.
            MCFieldParagraphStyle para;
            memset(&para, 0, sizeof(para));
            MCFieldCharacterStyle cs;
            memset(&cs, 0, sizeof(cs));
            cs.has_text_font = true;
            { MCAutoStringRef t_font; /* UNCHECKED */ MCStringCreateWithCString("Courier", &t_font); /* UNCHECKED */ MCNameCreate(*t_font, cs.text_font); }
            import_md_paragraph(ctxt, raw_line, raw_line_len, &para, &cs);
            MCValueRelease(cs.text_font);
            line = next_line;
            continue;
        }

        // ── Opening fence ────────────────────────────────────────────────────
        if (trimmed_len >= 3 && (raw_line[0] == '`' || raw_line[0] == '~'))
        {
            char fc = raw_line[0];
            uint32_t fc_len = count_repeat(raw_line, line_end, fc);
            if (fc_len >= 3)
            {
                ctxt.in_fenced_code = true;
                ctxt.fence_char     = fc;
                ctxt.fence_len      = fc_len;
                line = next_line;
                continue;
            }
        }

        // ── Blank line ───────────────────────────────────────────────────────
        if (trimmed_len == 0)
        {
            if (!ctxt.need_paragraph)
                import_md_end_paragraph(ctxt);
            line = next_line;
            continue;
        }

        const char *p = raw_line;

        // ── ATX heading: # through ###### ────────────────────────────────────
        if (p[0] == '#')
        {
            uint32_t level = count_repeat(p, line_end, '#');
            if (level <= 6 && (uint32_t)(line_end - p) > level && p[level] == ' ')
            {
                const char *content     = p + level + 1;
                uint32_t    content_len = trimmed_len - level - 1;
                // Strip trailing # markers.
                while (content_len > 0 && content[content_len - 1] == '#') content_len--;
                content_len = rtrim(content, content_len);

                MCFieldParagraphStyle para;
                memset(&para, 0, sizeof(para));

                MCFieldCharacterStyle cs;
                memset(&cs, 0, sizeof(cs));
                cs.has_text_style = true;
                cs.text_style     = FA_BOLD;
                if (level == 5) cs.text_style |= FA_ITALIC;
                cs.has_text_size  = true;
                cs.text_size      = s_md_heading_sizes[level - 1];

                import_md_paragraph(ctxt, content, content_len, &para, &cs);
                line = next_line;
                continue;
            }
        }

        // ── Blockquote: > ────────────────────────────────────────────────────
        if (p[0] == '>')
        {
            const char *content = p + 1;
            if (content < line_end && *content == ' ') content++;
            uint32_t content_len = (uint32_t)(line_end - content);

            MCFieldParagraphStyle para;
            memset(&para, 0, sizeof(para));
            para.has_left_indent = true;
            para.left_indent     = 30;

            import_md_paragraph(ctxt, content, content_len, &para, nil);
            line = next_line;
            continue;
        }

        // ── Unordered list: -, *, + ──────────────────────────────────────────
        if ((p[0] == '-' || p[0] == '*' || p[0] == '+') &&
            (uint32_t)(line_end - p) > 1 && p[1] == ' ')
        {
            // Distinguish from horizontal rules (--- etc.)
            bool is_hr = false;
            if (p[0] == '-' || p[0] == '*')
            {
                uint32_t hr_count = 0;
                bool only_delim = true;
                for (const char *q = p; q < line_end; q++)
                {
                    if (*q == p[0]) hr_count++;
                    else if (*q != ' ') { only_delim = false; break; }
                }
                if (only_delim && hr_count >= 3 && p[1] != ' ') is_hr = true;
            }

            if (!is_hr)
            {
                const char *content     = p + 2;
                uint32_t    content_len = (uint32_t)(line_end - content);

                MCFieldParagraphStyle para;
                memset(&para, 0, sizeof(para));
                para.has_list_style = true;
                para.list_style     = kMCParagraphListStyleDisc;
                para.list_depth     = 0;

                import_md_paragraph(ctxt, content, content_len, &para, nil);
                line = next_line;
                continue;
            }
        }

        // ── Ordered list: N. ─────────────────────────────────────────────────
        if (p[0] >= '1' && p[0] <= '9')
        {
            const char *np = p;
            while (np < line_end && *np >= '0' && *np <= '9') np++;
            if (np < line_end && *np == '.' && np + 1 < line_end && np[1] == ' ')
            {
                uint32_t idx = (uint32_t)strtoul(p, nil, 10);
                const char *content     = np + 2;
                uint32_t    content_len = (uint32_t)(line_end - content);

                MCFieldParagraphStyle para;
                memset(&para, 0, sizeof(para));
                para.has_list_style = true;
                para.list_style     = kMCParagraphListStyleNumeric;
                para.list_depth     = 0;
                para.has_list_index = true;
                para.list_index     = (uint16_t)idx;

                import_md_paragraph(ctxt, content, content_len, &para, nil);
                line = next_line;
                continue;
            }
        }

        // ── Indented code block (4 spaces or 1 tab) ──────────────────────────
        if ((raw_line_len >= 4 && p[0] == ' ' && p[1] == ' ' && p[2] == ' ' && p[3] == ' ') ||
            (raw_line_len >= 1 && p[0] == '\t'))
        {
            const char *content     = (p[0] == '\t') ? p + 1 : p + 4;
            uint32_t    content_len = (uint32_t)(line_end - content);

            MCFieldParagraphStyle para;
            memset(&para, 0, sizeof(para));
            MCFieldCharacterStyle cs;
            memset(&cs, 0, sizeof(cs));
            cs.has_text_font = true;
            { MCAutoStringRef t_font; /* UNCHECKED */ MCStringCreateWithCString("Courier", &t_font); /* UNCHECKED */ MCNameCreate(*t_font, cs.text_font); }
            import_md_paragraph(ctxt, content, content_len, &para, &cs);
            MCValueRelease(cs.text_font);
            line = next_line;
            continue;
        }

        // ── Horizontal rule: ---, ***, ___ ───────────────────────────────────
        {
            char hc = p[0];
            if (trimmed_len >= 3 && (hc == '-' || hc == '*' || hc == '_'))
            {
                uint32_t hr_count = 0;
                bool only_hr = true;
                for (const char *q = p; q < p + trimmed_len; q++)
                {
                    if (*q == hc) hr_count++;
                    else if (*q != ' ') { only_hr = false; break; }
                }
                if (only_hr && hr_count >= 3)
                {
                    // Emit an empty paragraph as a visual separator.
                    MCFieldParagraphStyle para;
                    memset(&para, 0, sizeof(para));
                    import_md_begin_paragraph(ctxt, &para, nil);
                    import_md_end_paragraph(ctxt);
                    line = next_line;
                    continue;
                }
            }
        }

        // ── Setext heading: underline with == or -- ───────────────────────────
        // Peek at the next line to detect "text\n====" or "text\n----".
        if (next_line < end)
        {
            const char *ul     = next_line;
            const char *ul_end = ul;
            while (ul_end < end && *ul_end != '\n' && *ul_end != '\r') ul_end++;
            uint32_t ul_len = rtrim(ul, (uint32_t)(ul_end - ul));

            if (ul_len >= 1 && (ul[0] == '=' || ul[0] == '-'))
            {
                bool all_same = true;
                char uc = ul[0];
                for (uint32_t i = 0; i < ul_len; i++)
                    if (ul[i] != uc) { all_same = false; break; }

                if (all_same)
                {
                    uint32_t level = (uc == '=') ? 1 : 2;

                    MCFieldParagraphStyle para;
                    memset(&para, 0, sizeof(para));

                    MCFieldCharacterStyle cs;
                    memset(&cs, 0, sizeof(cs));
                    cs.has_text_style = true;
                    cs.text_style     = FA_BOLD;
                    cs.has_text_size  = true;
                    cs.text_size      = s_md_heading_sizes[level - 1];

                    import_md_paragraph(ctxt, p, trimmed_len, &para, &cs);

                    // Advance past the underline line too.
                    const char *after_ul = ul_end;
                    if (after_ul < end && *after_ul == '\r') after_ul++;
                    if (after_ul < end && *after_ul == '\n') after_ul++;
                    line = after_ul;
                    continue;
                }
            }
        }

        // ── Regular paragraph ─────────────────────────────────────────────────
        // Each source line becomes its own field paragraph.  Blank lines are
        // already handled above (they set need_paragraph = true, which is a
        // no-op if it's already true after the previous line ended).
        {
            if (ctxt.need_paragraph)
            {
                MCFieldParagraphStyle para;
                memset(&para, 0, sizeof(para));
                ctxt.para_style     = para;
                ctxt.has_para_style = false;
                memset(&ctxt.char_style, 0, sizeof(ctxt.char_style));
            }

            MCFieldCharacterStyle base;
            memset(&base, 0, sizeof(base));
            import_md_inline(ctxt, p, trimmed_len, base);
            import_md_end_paragraph(ctxt);
        }

        line = next_line;
    }

    // Flush any trailing content.
    if (!ctxt.need_paragraph)
        import_md_end_paragraph(ctxt);

    // Guarantee at least one paragraph.
    if (ctxt.paragraphs == nil)
    {
        MCFieldParagraphStyle para;
        memset(&para, 0, sizeof(para));
        importparagraph(ctxt.paragraphs, &para);
    }

    MCMemoryDeleteArray(ctxt.bytes, ctxt.byte_capacity);
    return ctxt.paragraphs;
}

////////////////////////////////////////////////////////////////////////////////
// EXPORTER
////////////////////////////////////////////////////////////////////////////////

struct export_md_t
{
    MCStringRef m_text;         // output buffer
    uint32_t    ordered_index;  // running counter for ordered list items
    bool        at_line_start;  // whether we are at the start of a line
    bool        is_first_para;  // whether this is the first paragraph emitted

    // Inline state — which markers are currently open.
    bool bold_open;
    bool italic_open;
    bool code_open;
};

// Append a C-string literal to the output.
static void export_md_append(export_md_t& ctxt, const char *p_cstr)
{
    /* UNCHECKED */ MCStringAppendFormat(ctxt.m_text, "%s", p_cstr);
}

// Emit a substring of the field text, escaping Markdown special characters.
static void export_md_emit_text(export_md_t& ctxt,
                                 MCStringRef p_text, MCRange p_range,
                                 bool p_in_code)
{
    for (uindex_t i = p_range.offset; i < p_range.offset + p_range.length; i++)
    {
        unichar_t ch = MCStringGetCharAtIndex(p_text, i);

        if (!p_in_code)
        {
            // Escape characters that would be interpreted as Markdown syntax.
            switch (ch)
            {
                case '\\': case '*': case '_': case '`':
                case '[':  case ']': case '(':  case ')':
                    /* UNCHECKED */ MCStringAppendFormat(ctxt.m_text, "\\%c", (char)ch);
                    continue;
                default:
                    break;
            }
        }

        /* UNCHECKED */ MCStringAppendChar(ctxt.m_text, ch);
    }
}

// Simpler emit that appends the substring directly (for code spans where we
// already decided no escaping is needed inside backticks).
static void export_md_emit_code_text(export_md_t& ctxt,
                                      MCStringRef p_text, MCRange p_range)
{
    MCAutoStringRef t_sub;
    if (MCStringCopySubstring(p_text, p_range, &t_sub))
        /* UNCHECKED */ MCStringAppend(ctxt.m_text, *t_sub);
}

// Close any open inline markers.
static void export_md_close_inline(export_md_t& ctxt)
{
    if (ctxt.code_open)   { export_md_append(ctxt, "`");  ctxt.code_open   = false; }
    if (ctxt.bold_open && ctxt.italic_open)
    {
        export_md_append(ctxt, "***");
        ctxt.bold_open = ctxt.italic_open = false;
    }
    else
    {
        if (ctxt.bold_open)   { export_md_append(ctxt, "**"); ctxt.bold_open   = false; }
        if (ctxt.italic_open) { export_md_append(ctxt, "*");  ctxt.italic_open = false; }
    }
}

static bool export_md_emit_paragraphs(void *p_context,
                                       MCFieldExportEventType p_event,
                                       const MCFieldExportEventData& p_data)
{
    export_md_t& ctxt = *(export_md_t *)p_context;

    if (p_event == kMCFieldExportEventBeginParagraph)
    {
        // Separate paragraphs with a blank line (except before the first).
        if (!ctxt.is_first_para)
            export_md_append(ctxt, "\n");
        ctxt.is_first_para  = false;
        ctxt.bold_open      = false;
        ctxt.italic_open    = false;
        ctxt.code_open      = false;
        ctxt.at_line_start  = true;

        if (!p_data.has_paragraph_style)
            return true;

        const MCFieldParagraphStyle& ps = p_data.paragraph_style;

        // List items.
        if (ps.has_list_style)
        {
            if (ps.list_style == kMCParagraphListStyleSkip)
                return true; // continuation paragraph inside a list — no prefix
            if (ps.list_style == kMCParagraphListStyleNumeric ||
                ps.list_style == kMCParagraphListStyleLowerCase ||
                ps.list_style == kMCParagraphListStyleUpperCase ||
                ps.list_style == kMCParagraphListStyleLowerRoman ||
                ps.list_style == kMCParagraphListStyleUpperRoman)
            {
                uint32_t idx = ps.has_list_index ? ps.list_index : ++ctxt.ordered_index;
                /* UNCHECKED */ MCStringAppendFormat(ctxt.m_text, "%u. ", idx);
            }
            else // disc, circle, square → unordered
            {
                export_md_append(ctxt, "- ");
                ctxt.ordered_index = 0;
            }
            ctxt.at_line_start = false;
            return true;
        }

        ctxt.ordered_index = 0;

        // Blockquote heuristic: paragraph with a left indent.
        if (ps.has_left_indent && ps.left_indent > 0)
        {
            export_md_append(ctxt, "> ");
            ctxt.at_line_start = false;
        }
    }
    else if (p_event == kMCFieldExportEventEndParagraph)
    {
        export_md_close_inline(ctxt);
        export_md_append(ctxt, "\n");
        ctxt.at_line_start = true;
    }
    else if (p_event == kMCFieldExportEventNativeRun ||
             p_event == kMCFieldExportEventUnicodeRun)
    {
        if (!p_data.has_character_style)
        {
            // Plain run — emit text directly (no inline markers needed).
            export_md_close_inline(ctxt);
            export_md_emit_text(ctxt, p_data.m_text, p_data.m_range, false);
            return true;
        }

        const MCFieldCharacterStyle& cs = p_data.character_style;

        bool want_bold   = cs.has_text_style && (cs.text_style & FA_WEIGHT) == (FA_BOLD & FA_WEIGHT);
        bool want_italic = cs.has_text_style && (cs.text_style & (FA_ITALIC | FA_OBLIQUE)) != 0;
        bool want_code   = cs.has_text_font  && MCNameIsEqualToCaseless(cs.text_font, MCNAME("Courier"));
        bool want_link   = cs.has_link_text;

        // Heading detection: emit the ATX prefix before the first run of a
        // paragraph when the font size matches a heading size and style is bold.
        if (ctxt.at_line_start && cs.has_text_size && want_bold)
        {
            uint32_t level = md_size_to_heading_level(cs.text_size);
            if (level > 0)
            {
                for (uint32_t i = 0; i < level; i++)
                    MCStringAppendChar(ctxt.m_text, '#');
                MCStringAppendChar(ctxt.m_text, ' ');
                // Headings don't get bold markers — the # prefix conveys it.
                want_bold = false;
            }
        }
        ctxt.at_line_start = false;

        if (want_link)
        {
            // Links wrap the entire run: [text](url)
            export_md_close_inline(ctxt);
            export_md_append(ctxt, "[");
            export_md_emit_text(ctxt, p_data.m_text, p_data.m_range, false);
            export_md_append(ctxt, "](");
            /* UNCHECKED */ MCStringAppend(ctxt.m_text, cs.link_text);
            export_md_append(ctxt, ")");
            return true;
        }

        if (want_code)
        {
            export_md_close_inline(ctxt);
            export_md_append(ctxt, "`");
            export_md_emit_code_text(ctxt, p_data.m_text, p_data.m_range);
            export_md_append(ctxt, "`");
            return true;
        }

        // Open/close bold and italic markers as needed.
        bool need_change = (want_bold != ctxt.bold_open || want_italic != ctxt.italic_open);
        if (need_change)
        {
            export_md_close_inline(ctxt);
            if (want_bold && want_italic) { export_md_append(ctxt, "***"); ctxt.bold_open = ctxt.italic_open = true; }
            else if (want_bold)           { export_md_append(ctxt, "**");  ctxt.bold_open   = true; }
            else if (want_italic)         { export_md_append(ctxt, "*");   ctxt.italic_open = true; }
        }

        export_md_emit_text(ctxt, p_data.m_text, p_data.m_range, false);
    }

    return true;
}

bool MCField::exportasmarkdowntext(uint32_t p_part_id,
                                    int32_t p_start, int32_t p_finish,
                                    MCStringRef& r_text)
{
    return exportasmarkdowntext(resolveparagraphs(p_part_id), p_start, p_finish, r_text);
}

bool MCField::exportasmarkdowntext(MCParagraph *p_paragraphs,
                                    int32_t p_start, int32_t p_finish,
                                    MCStringRef& r_text)
{
    export_md_t ctxt;
    memset(&ctxt, 0, sizeof(export_md_t));
    /* UNCHECKED */ MCStringCreateMutable(0, ctxt.m_text);
    ctxt.is_first_para = true;

    uint32_t t_flags = kMCFieldExportParagraphs | kMCFieldExportRuns |
                       kMCFieldExportParagraphStyles | kMCFieldExportCharacterStyles;

    doexport(t_flags, p_paragraphs, p_start, p_finish,
             export_md_emit_paragraphs, &ctxt);

    r_text = ctxt.m_text; // transfer ownership
    return true;
}
