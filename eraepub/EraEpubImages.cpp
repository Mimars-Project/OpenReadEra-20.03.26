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

void CreBridge::processImagesXpaths(CmdRequest &request, CmdResponse &response)
{
    response.cmd = CMD_RES_CRE_IMG_XPATHS;
    lString16Map map;
    for (int page = 0; page < doc_view_->GetPagesCount(); page++)
    {
        LVArray<ImgRect> images_array = doc_view_->GetPageImages(page);
        for (int i = 0; i < images_array.length(); i++)
        {
            ImgRect curr = images_array.get(i);
            ldomXPointer xPointer(curr.getNode(),0);
            lString16 path = xPointer.toString();
            std::string strp = std::string(LCSTR(path),path.length());
            if(map.find(strp) != map.end())
            {
                continue;
            }
            else
            {
                map.insert(lString16Map::value_type(LCSTR(path), 1));
                CRLog::debug("processImagesXpaths: {%d} = [%s]", page,LCSTR(path));
                //response.addInt(page);
                responseAddString(response,path);
            }
        }
    }
}

void CreBridge::processImageByXpath(CmdRequest &request, CmdResponse &response)
{
    response.cmd = CMD_RES_CRE_IMG_BLOB;
    CmdDataIterator iter(request.first);
    uint8_t *xpath_string;

    iter.getByteArray(&xpath_string);
    if (!iter.isValid())
    {
        CRLog::error("processImageByXpath invalid iterator");
        response.result = RES_BAD_REQ_DATA;
        return;
    }

    lString16 xpath(reinterpret_cast<const char *>(xpath_string));

    ldomXPointer bm = doc_view_->GetCrDom()->createXPointer(xpath);
    if (bm.isNull())
    {
        CRLog::error("processImageByXpath bad xpath bm.isNull()");
        response.result = RES_BAD_REQ_DATA;
        return;
    }

    auto imgData = new CmdData();
    ldomNode* node = bm.getNode();
    int thumb_width = 0;
    int thumb_height = 0;
    imgData->type = TYPE_ARRAY_POINTER;

    LVImageSourceRef img = node->getObjectImageSource();
    if (!img.isNull() && img->GetWidth() > 0 && img->GetHeight() > 0)
    {
        if (img->GetWidth() * img->GetHeight() * 4 <= 100e6) // 100 mb
        {
            thumb_width = img->GetWidth();
            thumb_height = img->GetHeight();
            unsigned char *pixels = imgData->newByteArray(thumb_width * thumb_height * 4);
            auto buf = new LVColorDrawBuf(thumb_width, thumb_height, pixels, 32);
            buf->Clear(0xffffffff);
            buf->Draw(img, 0, 0, thumb_width, thumb_height, false);
            convertBitmap(buf);
            delete buf;
            img.Clear();
        }
        else
        {
            CRLog::warn("Ignoring large image");
        }
    }
    response.addData(imgData);
    response.addInt((uint32_t) thumb_width);
    response.addInt((uint32_t) thumb_height);
}

void CreBridge::processImageHitbox(CmdRequest &request, CmdResponse &response)
{
    response.cmd = CMD_RES_CRE_IMG_HITBOXES;
    CmdDataIterator iter(request.first);
    uint32_t external_page = 0;
    iter.getInt(&external_page);
    if (!iter.isValid())
    {
        CRLog::error("processImageHitbox bad request data");
        response.result = RES_BAD_REQ_DATA;
        return;
    }
    auto page = (uint32_t) ImportPage(external_page, doc_view_->GetColumns());
    doc_view_->GoToPage(page);

    LVArray<ImgRect> hitboxes = doc_view_->GetPageImages();
    for (int i = 0; i < hitboxes.length(); i++)
    {
        ImgRect currHitbox = hitboxes.get(i);
        lvRect rect = currHitbox.getRect();

        doc_view_->DocToWindowRect(rect,true);

        lString16 xpath = ldomXPointer(currHitbox.getNode(),0).toString();
        //CRLog::error("processImageHitbox {%d} [%d],[%d:%d][%d:%d] = %s",
        //        i,doc_view_->GetCurrPage(),rect.left,rect.right,rect.top,rect.bottom,LCSTR(xpath));

        float page_width    = doc_view_->GetWidth();
        float page_height   = doc_view_->GetHeight();

        response.addFloat(rect.left   / page_width);
        response.addFloat(rect.top    / page_height);
        response.addFloat(rect.right  / page_width);
        response.addFloat(rect.bottom / page_height);
        responseAddString(response, xpath);
    }
}

