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
#include "include/epubfmt.h"
#include "include/fb3fmt.h"

void CreBridge::processMeta(CmdRequest &request, CmdResponse &response) {
    response.cmd = CMD_RES_CRE_METADATA;
    CmdDataIterator iter(request.first);
    uint32_t format = 0;
    uint8_t *path_arg;
    uint32_t direct_archive = 0;
    uint32_t smart_archive_arg = 0;
    iter.getInt(&format)
            .getByteArray(&path_arg)
            .getInt(&direct_archive)
            .getInt(&smart_archive_arg);
    if (!iter.isValid()) {
        CRLog::error("processMeta: iterator invalid data");
        response.result = RES_BAD_REQ_DATA;
        return;
    }
    const char *path = reinterpret_cast<const char *>(path_arg);
    bool smart_archive = (bool) smart_archive_arg;
    LVStreamRef stream = LVDocView::ThResolveStream(format, path, direct_archive, smart_archive);
    if (!stream) {
        if (direct_archive > 0) {
            response.result = RES_ARCHIVE_COLLISION;
        } else {
            response.result = RES_INTERNAL_ERROR;
        }
        return;
    }
    LVStreamRef thumb_stream;
    lString16 title;
    lString16 authors;
    lString16 series;
    int series_number = 0;
    lString16 lang;
    if (format == DOC_FORMAT_FB3) {
        CRLog::error("processMeta: FB3  meta extraction");
        LVContainerRef container = LVOpenArchive(stream);
        if (container.isNull()) {
            CRLog::error("processMeta: FB3 is not in ZIP");
            response.result = RES_INTERNAL_ERROR;
            return;
        }
        EncryptedDataContainer *decryptor = new EncryptedDataContainer(container);
        if (decryptor->open()) {
            CRLog::debug("processMeta: FB3 encrypted items detected");
        }
        container = LVContainerRef(decryptor);
        lString16 root_file_path = L"fb3/description.xml";
        LVStreamRef content_stream = container->OpenStream(root_file_path.c_str(), LVOM_READ);

        if (content_stream.isNull()) {
            CRLog::error("processMeta: malformed FB3 (1)");
            response.result = RES_INTERNAL_ERROR;
            return;
        }
        CrDom *dom = LVParseXMLStream(content_stream);
        if (!dom) {
            CRLog::error("processMeta: malformed FB3 (2)");
            response.result = RES_INTERNAL_ERROR;
            return;
        }
        thumb_stream = GetFb3CoverImage(container);

        GetFb3Metadata(dom, &title, &authors, &lang, &series, &series_number);
        delete dom;
    } else if (format == DOC_FORMAT_EPUB) {
        LVContainerRef container = LVOpenArchive(stream);
        if (container.isNull()) {
            CRLog::error("processMeta: EPUB is not in ZIP");
            response.result = RES_INTERNAL_ERROR;
            return;
        }
        // Check root media type
        lString16 root_file_path = EpubGetRootFilePath(container);
        if (root_file_path.empty()) {
            CRLog::error("processMeta: malformed EPUB (0)");
            response.result = RES_INTERNAL_ERROR;
            return;
        }
        EncryptedDataContainer *decryptor = new EncryptedDataContainer(container);
        if (decryptor->open()) {
            CRLog::debug("processMeta: EPUB encrypted items detected");
        }
        container = LVContainerRef(decryptor);
        lString16 code_base = LVExtractPath(root_file_path, false);
        LVStreamRef content_stream = container->OpenStream(root_file_path.c_str(), LVOM_READ);
        if (content_stream.isNull()) {
            CRLog::error("processMeta: malformed EPUB (1)");
            response.result = RES_INTERNAL_ERROR;
            return;
        }
        CrDom *dom = LVParseXMLStream(content_stream);
        if (!dom) {
            CRLog::error("processMeta: malformed EPUB (2)");
            response.result = RES_INTERNAL_ERROR;
            return;
        }
        thumb_stream = GetEpubCoverImage(dom, container, code_base);
        GetEpubMetadata(dom, &title, &authors, &lang, &series, &series_number);
        delete dom;
    } else if (format == DOC_FORMAT_FB2) {
        thumb_stream = GetFB2Coverpage(stream);
        CrDom dom;
        LvDomWriter writer(&dom, true);
        dom.setNodeTypes(fb2_elem_table);
        dom.setAttributeTypes(fb2_attr_table);
        dom.setNameSpaceTypes(fb2_ns_table);
        LvXmlParser parser(stream, &writer);
        if (parser.CheckFormat() && parser.Parse()) {
            authors = ExtractDocAuthors(&dom, lString16("|"));
            title = ExtractDocTitle(&dom);
            lang = ExtractDocLanguage(&dom);
            series = ExtractDocSeries(&dom, &series_number);
            //coverimage = ExtractDocThumbImageName(&dom);
            //CRLog::error("coverimage extracted == %s", LCSTR(coverimage));
        } else {
            CRLog::error("processMeta: !parser.CheckFormat() || !parser.Parse()");
            response.result = RES_INTERNAL_ERROR;
            return;
        }
#ifdef OREDEBUG
#if 0
        LVStreamRef out = LVOpenFileStream("/data/data/org.readera/files/metatemp.xml", LVOM_WRITE);
        dom.saveToStream(out, NULL, true);
#endif
#endif
    }
    auto doc_thumb = new CmdData();
    int thumb_width = 0;
    int thumb_height = 0;
    doc_thumb->type = TYPE_ARRAY_POINTER;
    if (!thumb_stream.isNull()) {
        LVImageSourceRef thumb_image = LVCreateStreamCopyImageSource(thumb_stream);
        if (!thumb_image.isNull() && thumb_image->GetWidth() > 0 && thumb_image->GetHeight() > 0) {
            if (thumb_image->GetWidth() * thumb_image->GetHeight() * 4 <= META_THUMB_MAX_SIZE) {
                thumb_width = thumb_image->GetWidth();
                thumb_height = thumb_image->GetHeight();
                unsigned char *pixels = doc_thumb->newByteArray(thumb_width * thumb_height * 4);
                auto buf = new LVColorDrawBuf(thumb_width, thumb_height, pixels, 32);
                buf->Draw(thumb_image, 0, 0, thumb_width, thumb_height, false);
                convertBitmap(buf);
                delete buf;
                thumb_image.Clear();
            } else {
                CRLog::warn("Ignoring large doc thumb");
            }
        }
    }
    if (title.length() > META_STRING_MAX_LENGTH) {
        title = title.substr(0, META_STRING_MAX_LENGTH);
    }
    if (authors.length() > META_STRING_MAX_LENGTH) {
        authors = authors.substr(0, META_STRING_MAX_LENGTH);
    }
    if (series.length() > META_STRING_MAX_LENGTH) {
        series = series.substr(0, META_STRING_MAX_LENGTH);
    }
    if (lang.length() > META_STRING_MAX_LENGTH) {
        lang = lang.substr(0, META_STRING_MAX_LENGTH);
    }
    response.addData(doc_thumb);
    response.addInt((uint32_t) thumb_width);
    response.addInt((uint32_t) thumb_height);
    responseAddString(response, title);
    responseAddString(response, authors);
    responseAddString(response, series);
    response.addInt((uint32_t) series_number);
    responseAddString(response, lang);
}
