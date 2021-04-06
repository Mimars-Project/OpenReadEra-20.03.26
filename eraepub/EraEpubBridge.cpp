/*
 * Copyright (C) 2013-2020 READERA LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Developers: ReadEra Team (2013-2020), Playful Curiosity (2013-2020),
 * Tarasus (2018-2020).
 */

#include "EraEpubBridge.h"
#include "StSocket.h"
#include <cstdlib>
// Should go as last include to not trigger rebuild of other files on changes
#include "openreadera_version.h"

static int CeilToEvenInt(int n)
{
    return (n + 1) & ~1;
}

static int FloorToEvenInt(int n)
{
    return n & ~1;
}

uint32_t CreBridge::ExportPagesCount(int columns, int pages)
{
	if (columns == 2) {
		return (uint32_t) (CeilToEvenInt(pages) / columns);
	}
	return (uint32_t) pages;
}

int CreBridge::ExportPage(int columns, int page)
{
    if (columns == 2) {
        return FloorToEvenInt(page) / columns;
    }
    return page;
}

int CreBridge::ImportPage(int page, int columns)
{
    return page * columns;
}

void CreBridge::responseAddLinkUnknown(CmdResponse& response, const lString16& href,
        float l, float t, float r, float b)
{
    response.addWords(LINK_TARGET_UNKNOWN, 0);
    responseAddString(response, href);
    response.addFloat(l);
    response.addFloat(t);
    response.addFloat(r);
    response.addFloat(b);
}

void CreBridge::responseAddString(CmdResponse& response, const lString16& str16)
{
    lString8 str8 = UnicodeToUtf8(str16);
    auto size = (uint32_t) str8.size();
    // We will place null-terminator at the string end
    size++;
    auto cmd_data = new CmdData();
    unsigned char* str_buffer = cmd_data->newByteArray(size);
    memcpy(str_buffer, str8.c_str(), (size - 1));
    str_buffer[size - 1] = 0;
    response.addData(cmd_data);
}

void CreBridge::convertBitmap(LVColorDrawBuf* bitmap)
{
    if (bitmap->GetBitsPerPixel() == 32) {
        // Convert Cre colors to Android
        int size = bitmap->GetWidth() * bitmap->GetHeight();
        for (lUInt8* p = bitmap->GetData(); --size >= 0; p+=4) {
            // Invert A
            p[3] ^= 0xFF;
            // Swap R and B
            lUInt8 t = p[0];
            p[0] = p[2];
            p[2] = t;
        }
    }
}

void CreBridge::processFonts(CmdRequest& request, CmdResponse& response)
{
    response.cmd = CMD_RES_PDF_FONTS;
    CmdDataIterator iter(request.first);
    while (iter.hasNext()) {
        uint32_t font_family;
        if (!iter.getInt(&font_family).isValid()) {
            CRLog::error("No font family found");
            response.result = RES_BAD_REQ_DATA;
            return;
        }
        uint8_t* fonts[4];
        iter.getByteArray(&fonts[0])
                .getByteArray(&fonts[1])
                .getByteArray(&fonts[2])
                .getByteArray(&fonts[3]);
        if (!iter.isValid()) {
            CRLog::error("processFonts no fonts");
            response.result = RES_BAD_REQ_DATA;
            return;
        }
        for (auto & i : fonts) {
            const char* const_font = reinterpret_cast<const char*>(i);
            if (strlen(const_font) == 0) {
                continue;
            }
            lString16 font = lString16(const_font);
            fontMan->RegisterFont(UnicodeToUtf8(font));
        }
    }
}

void CreBridge::processOpen(CmdRequest& request, CmdResponse& response)
{
    response.cmd = CMD_RES_OPEN;
    CmdDataIterator iter(request.first);
    uint32_t doc_format = 0;
    uint8_t* socket_name = nullptr;
    uint8_t* absolute_path_arg = nullptr;
    uint32_t compressed_size = 0;
    uint32_t smart_archive_arg = 0;
    iter.getInt(&doc_format)
            .getByteArray(&socket_name)
            .getByteArray(&absolute_path_arg)
            .getInt(&compressed_size)
            .getInt(&smart_archive_arg);
    if (!iter.isValid() || !socket_name || !absolute_path_arg) {
        response.result = RES_BAD_REQ_DATA;
        return;
    }
#if SEND_FD_VIA_SOCKET == 1
    StSocketConnection connection((const char*) socket_name);
    if (!connection.isValid()) {
        response.result = RES_BAD_REQ_DATA;
        return;
    }
    int fd;
    bool received = connection.receiveFileDescriptor(fd);
    if (!received) {
        response.result = RES_BAD_REQ_DATA;
        return;
    }
#endif
    const char* absolute_path = reinterpret_cast<const char*>(absolute_path_arg);
    bool smart_archive = (bool) smart_archive_arg;
	bool result = doc_view_->LoadDoc(doc_format, absolute_path, compressed_size, smart_archive);
    if (result) {
        doc_view_->RenderIfDirty();
        response.addInt(ExportPagesCount(doc_view_->GetColumns(), doc_view_->GetPagesCount()));
    } else if (compressed_size > 0) {
        response.result = RES_ARCHIVE_COLLISION;
    } else {
        response.result = RES_INTERNAL_ERROR;
    }
}

void CreBridge::processPageRender(CmdRequest& request, CmdResponse& response)
{
    response.cmd = CMD_RES_PAGE_RENDER;
    CmdDataIterator iter(request.first);
    uint32_t page;
    uint32_t width;
    uint32_t height;
    iter.getInt(&page).getInt(&width).getInt(&height);
    if (!iter.isValid()) {
        CRLog::error("processPageRender bad request data");
        response.result = RES_BAD_REQ_DATA;
        return;
    }
    if (doc_view_ == nullptr) {
        CRLog::error("processPageRender doc not opened");
        response.result = RES_BAD_REQ_DATA;
        return;
    }
    if (doc_view_->height_ != height || doc_view_->width_ != width) {
        CRLog::error("processPageRender wanted page size mismatch");
        response.result = RES_BAD_REQ_DATA;
        return;
    }
    doc_view_->GoToPage(ImportPage(page, doc_view_->GetColumns()));
    auto resp = new CmdData();
    unsigned char* pixels = resp->newByteArray(width * height * 4);
    auto buf = new LVColorDrawBuf(width, height, pixels, 32);
    doc_view_->Draw(*buf);
    convertBitmap(buf);
    delete buf;
    response.addData(resp);
}

void CreBridge::processQuit(CmdRequest& request, CmdResponse& response)
{
    response.cmd = CMD_RES_QUIT;
}

CreBridge::CreBridge() : StBridge(ORE_LOG_TAG)
{
    doc_view_ = nullptr;
#ifdef OREDEBUG
    CRLog::setLevel(CRLog::TRACE);
#else
    CRLog::setLevel(CRLog::FATAL);
#endif
    InitFontManager(lString8::empty_str);
    // 0 - disabled, 1 - bytecode, 2 - auto
    fontMan->SetHintingMode(HINTING_MODE_BYTECODE_INTERPRETOR);
    fontMan->setKerning(true);
    HyphMan::init();
}

CreBridge::~CreBridge()
{
    delete doc_view_;
    HyphMan::uninit();
    ShutdownFontManager();
}

void CreBridge::process(CmdRequest& request, CmdResponse& response)
{
    response.reset();
    request.print("EraEpubBridge");
    switch (request.cmd)
    {
        case CMD_REQ_PDF_FONTS:
            processFonts(request, response);
            break;
        case CMD_REQ_SET_CONFIG:
            processConfig(request, response);
            break;
        case CMD_REQ_OPEN:
            processOpen(request, response);
            break;
        case CMD_REQ_PAGE_RENDER:
            processPageRender(request, response);
            break;
        case CMD_REQ_LINKS:
            processPageLinks(request, response);
            break;
        case CMD_REQ_PAGE_TEXT:
            processPageText(request, response);
            break;
        case CMD_REQ_XPATH_BY_HITBOX:
            processXPathByHitbox(request, response);
            break;
        case CMD_REQ_XPATH_BY_RECT_ID:
            processXPathByRectId(request, response);
            break;
        case CMD_REQ_RANGE_HITBOX:
            processPageRangeText(request, response);
            break;
        case CMD_REQ_OUTLINE:
            processOutline(request, response);
            break;
        case CMD_REQ_CRE_PAGE_BY_XPATH:
            processPageByXPath(request, response);
            break;
        case CMD_REQ_CRE_PAGE_BY_XPATH_MULT:
            processPageByXPathMultiple(request, response);
            break;
        case CMD_REQ_CRE_PAGE_XPATH:
            processPageXPath(request, response);
            break;
        case CMD_REQ_CRE_METADATA:
            processMeta(request, response);
            break;
        case CMD_REQ_QUIT:
            processQuit(request, response);
            break;
        case CMD_REQ_VERSION:
            OreVerResporse(OPENREADERA_BASE_VERSION, response);
            break;
        case CMD_REQ_SEARCH_PREVIEWS:
            processTextSearchPreviews(request, response);
            break;
        case CMD_REQ_SEARCH_HITBOXES:
            processTextSearchHitboxes(request, response);
            break;
        case CMD_REQ_INDEXER:
            processTextSearchGetSuggestionsIndex(request, response);
            break;
        case CMD_REQ_CRE_IMG_XPATHS:
            processImagesXpaths(request, response);
            break;
        case CMD_REQ_CRE_IMG_BLOB:
            processImageByXpath(request, response);
            break;
        case CMD_REQ_CRE_IMG_HITBOXES:
            processImageHitbox(request, response);
            break;
        case CMD_REQ_RTL_TEXT:
            processRtlText(request, response);
            break;
        default:
            CRLog::error("Unknown request: %d", request.cmd);
            response.result = RES_UNKNOWN_CMD;
            break;
    }
    response.print("EraEpubBridge");
}
