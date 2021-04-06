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

#ifndef CRCONFIG_H
#define CRCONFIG_H

#define TXT_SMART_HEADERS false
#define TXT_SMART_DESCRIPTION false
#define FIRSTPAGE_BLOCKS_MAX 250            //html + xml
#define FIRSTPAGE_BLOCKS_MAX_DOCX 500        //html + xml
#define FIRSTPAGE_BLOCKS_MAX_WORD 4000
#define FIRSTPAGE_BLOCKS_MAX_RTF 2000
#define FIRSTPAGE_BLOCKS_MAX_CHM 5

#define FALLBACK_FONT_ARRAY_SIZE 500
#define FALLBACK_CYCLE_MAX 5
#define FONT_FOLDER "/system/fonts/"
#define SYSTEM_FALLBACK_FONTS_ENABLE 1
#define RTL_DISPLAY_ENABLE           1
#define CSS_EMBEDDED_STYLES_LEVEL    1 // 0 == OFF; 1 = +filtering; 2 = no filtering (leads to crash or slow performance);

#define FALLBACK_FACE_DEFAULT lString8("Roboto") // lString8("NONE") to switch it off

#define CHAR_HEIGHT_MIN 5
#define PARAEND_REPEAT_MAX 2
#define NOTES_HIDDEN_ID L"__notes_hidden__"
#define NOTES_HIDDEN_MAX_LEN 500
#define TOC_ITEM_LENGTH_MAX 150

extern int gDocumentRTL;
extern int gTextLeftShift;
extern int gDocumentFormat;

#ifdef OREDEBUG
#define DUMP_DOMTREE 0
#define DEBUG_TREE_DRAW 0 // define to non-zero (1..5) to see block bounds
#define DEBUG_CRE_PARA_END_BLOCKS 0
#define DEBUG_DRAW_IMAGE_HITBOXES 0
#define DEBUG_GETRECT_LOGS 0
#define DEBUG_NOTES_HIDDEN_SHOW 0
#define DEBUG_DRAW_CLIP_REGION 0
#else
#define DUMP_DOMTREE 0
#define DEBUG_TREE_DRAW 0
#define DEBUG_CRE_PARA_END_BLOCKS 0
#define DEBUG_DRAW_IMAGE_HITBOXES 0
#define DEBUG_GETRECT_LOGS 0
#define DEBUG_NOTES_HIDDEN_SHOW 0
#define DEBUG_DRAW_CLIP_REGION 0
#endif // OREDEBUG

#endif //CRCONFIG_H