#pragma once

// SPDX-License-Identifier: ISC
// Shared markdown parser for GMPI rich text rendering.
// Produces plain text with formatting runs from a markdown string.

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace gmpi
{
namespace drawing
{

struct MarkdownRun
{
    uint32_t startPosition;
    uint32_t length;
    bool bold = false;
    bool italic = false;
    bool strikethrough = false;
    bool monospace = false;
    float fontSizeScale = 0.0f; // 0 = no change, >0 = multiplier relative to base font size
};

struct MarkdownParseResult
{
    std::string plainText;
    std::vector<MarkdownRun> runs;
};

// Parse inline formatting within a single block of text.
// Appends to result.plainText and result.runs.
inline void parseInlineFormatting(std::string_view input, MarkdownParseResult& result)
{
    size_t i = 0;
    while (i < input.size())
    {
        // Inline code: `code`
        if (input[i] == '`')
        {
            const auto closing = input.find('`', i + 1);
            if (closing != std::string_view::npos)
            {
                MarkdownRun run;
                run.startPosition = static_cast<uint32_t>(result.plainText.size());
                run.length = static_cast<uint32_t>(closing - i - 1);
                run.monospace = true;
                result.plainText.append(input.data() + i + 1, closing - i - 1);
                result.runs.push_back(run);
                i = closing + 1;
                continue;
            }
        }

        // Bold/italic emphasis: *, **, ***, _, __, ___
        if (input[i] == '*' || input[i] == '_')
        {
            const char marker = input[i];
            size_t markerCount = 0;
            size_t j = i;
            while (j < input.size() && input[j] == marker) { ++markerCount; ++j; }

            if (markerCount >= 1 && markerCount <= 3)
            {
                const auto closing = input.find(std::string(markerCount, marker), j);
                if (closing != std::string_view::npos)
                {
                    MarkdownRun run;
                    run.startPosition = static_cast<uint32_t>(result.plainText.size());
                    run.length = static_cast<uint32_t>(closing - j);
                    run.bold = markerCount >= 2;
                    run.italic = (markerCount == 1 || markerCount == 3);
                    result.plainText.append(input.data() + j, closing - j);
                    result.runs.push_back(run);
                    i = closing + markerCount;
                    continue;
                }
            }
        }

        // Strikethrough: ~~text~~
        if (i + 1 < input.size() && input[i] == '~' && input[i + 1] == '~')
        {
            const auto closing = input.find("~~", i + 2);
            if (closing != std::string_view::npos)
            {
                MarkdownRun run;
                run.startPosition = static_cast<uint32_t>(result.plainText.size());
                run.length = static_cast<uint32_t>(closing - i - 2);
                run.strikethrough = true;
                result.plainText.append(input.data() + i + 2, closing - i - 2);
                result.runs.push_back(run);
                i = closing + 2;
                continue;
            }
        }

        // Link: [text](url) — display text only
        if (input[i] == '[')
        {
            const auto closeBracket = input.find(']', i + 1);
            if (closeBracket != std::string_view::npos &&
                closeBracket + 1 < input.size() && input[closeBracket + 1] == '(')
            {
                const auto closeParen = input.find(')', closeBracket + 2);
                if (closeParen != std::string_view::npos)
                {
                    result.plainText.append(input.data() + i + 1, closeBracket - i - 1);
                    i = closeParen + 1;
                    continue;
                }
            }
        }

        result.plainText.push_back(input[i]);
        ++i;
    }
}

inline MarkdownParseResult parseMarkdown(const char* markdownText)
{
    MarkdownParseResult result;
    const std::string_view input(markdownText);

    // split into lines
    std::vector<std::string_view> lines;
    size_t pos = 0;
    while (pos <= input.size())
    {
        auto nl = input.find('\n', pos);
        if (nl == std::string_view::npos)
        {
            lines.push_back(input.substr(pos));
            break;
        }
        auto line = input.substr(pos, nl - pos);
        if (!line.empty() && line.back() == '\r')
            line = line.substr(0, line.size() - 1);
        lines.push_back(line);
        pos = nl + 1;
    }

    auto isBlank = [](std::string_view s)
    {
        for (auto c : s)
            if (c != ' ' && c != '\t') return false;
        return true;
    };

    // check if a line is a horizontal rule: 3+ of the same char (-, *, _) with optional spaces
    auto isHorizontalRule = [](std::string_view s) -> bool
    {
        if (s.size() < 3) return false;
        const char rc = s[0];
        if (rc != '-' && rc != '*' && rc != '_') return false;
        int count = 0;
        for (auto c : s)
        {
            if (c == rc) ++count;
            else if (c != ' ') return false;
        }
        return count >= 3;
    };

    size_t i = 0;
    bool inParagraph = false; // are we accumulating flowing paragraph text?

    auto endParagraph = [&]()
    {
        if (inParagraph)
        {
            result.plainText += '\n';
            inParagraph = false;
        }
    };

    while (i < lines.size())
    {
        const auto line = lines[i];

        // fenced code block
        if (line.size() >= 3 && line[0] == '`' && line[1] == '`' && line[2] == '`')
        {
            endParagraph();
            ++i;
            while (i < lines.size())
            {
                if (lines[i].size() >= 3 && lines[i][0] == '`' && lines[i][1] == '`' && lines[i][2] == '`')
                {
                    ++i;
                    break;
                }
                MarkdownRun run;
                run.startPosition = static_cast<uint32_t>(result.plainText.size());
                run.length = static_cast<uint32_t>(lines[i].size());
                run.monospace = true;
                result.plainText.append(lines[i]);
                result.plainText += '\n';
                result.runs.push_back(run);
                ++i;
            }
            result.plainText += '\n';
            continue;
        }

        // heading: # through ######
        {
            int level = 0;
            size_t p = 0;
            while (p < line.size() && line[p] == '#') { ++level; ++p; }
            if (level >= 1 && level <= 6 && p < line.size() && line[p] == ' ')
            {
                endParagraph();
                const auto content = line.substr(p + 1);
                const uint32_t headingStart = static_cast<uint32_t>(result.plainText.size());
                parseInlineFormatting(content, result);
                const uint32_t headingLen = static_cast<uint32_t>(result.plainText.size()) - headingStart;

                static constexpr float scales[] = {2.0f, 1.5f, 1.25f, 1.0f, 0.875f, 0.83f};
                MarkdownRun run;
                run.startPosition = headingStart;
                run.length = headingLen;
                run.bold = true;
                run.fontSizeScale = scales[level - 1];
                result.runs.push_back(run);

                result.plainText += "\n\n";
                ++i;
                continue;
            }
        }

        // horizontal rule (must be checked before bullet list)
        if (isHorizontalRule(line))
        {
            endParagraph();
            // U+2500 BOX DRAWINGS LIGHT HORIZONTAL, repeated 20 times
            for (int r = 0; r < 20; ++r)
                result.plainText += "\xe2\x94\x80";
            result.plainText += "\n\n";
            ++i;
            continue;
        }

        // bullet list: - item or * item
        if (line.size() >= 2 && (line[0] == '-' || line[0] == '*') && line[1] == ' ')
        {
            endParagraph();
            const auto content = line.substr(2);
            // U+2022 BULLET
            result.plainText += "  \xe2\x80\xa2 ";
            parseInlineFormatting(content, result);
            result.plainText += '\n';
            ++i;
            continue;
        }

        // numbered list: 1. item
        {
            size_t numEnd = 0;
            while (numEnd < line.size() && line[numEnd] >= '0' && line[numEnd] <= '9') ++numEnd;
            if (numEnd > 0 && numEnd + 1 < line.size() && line[numEnd] == '.' && line[numEnd + 1] == ' ')
            {
                endParagraph();
                const auto content = line.substr(numEnd + 2);
                result.plainText += "  ";
                result.plainText.append(line.data(), numEnd);
                result.plainText += ". ";
                parseInlineFormatting(content, result);
                result.plainText += '\n';
                ++i;
                continue;
            }
        }

        // blockquote: > text
        if (!line.empty() && line[0] == '>')
        {
            endParagraph();
            auto content = line.substr(1);
            if (!content.empty() && content[0] == ' ')
                content = content.substr(1);
            result.plainText += "  ";
            parseInlineFormatting(content, result);
            result.plainText += '\n';
            ++i;
            continue;
        }

        // blank line — paragraph break
        if (line.empty() || isBlank(line))
        {
            endParagraph();
            if (!result.plainText.empty() && result.plainText.back() != '\n')
                result.plainText += '\n';
            ++i;
            continue;
        }

        // regular paragraph text — consecutive lines flow together
        if (inParagraph)
        {
            result.plainText += ' '; // join with space, not newline
        }
        parseInlineFormatting(line, result);
        inParagraph = true;
        ++i;
    }

    endParagraph();

    // trim trailing whitespace
    while (!result.plainText.empty() && (result.plainText.back() == '\n' || result.plainText.back() == ' '))
        result.plainText.pop_back();

    return result;
}

} // namespace drawing
} // namespace gmpi
