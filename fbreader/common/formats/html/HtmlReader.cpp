/*
 * FBReader -- electronic book reader
 * Copyright (C) 2005 Nikolay Pultsin <geometer@mawhrin.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <abstract/ZLInputStream.h>
#include <abstract/ZLStringInputStream.h>

#include <abstract/ZLXMLReader.h>
#include <abstract/ZLFSDir.h>
#include <abstract/ZLStringUtil.h>

#include "../docbook/DocBookReader.h"
#include "HtmlReader.h"

static const ZLXMLReader::Tag TAGS[] = {
	{0, 0}
};

const ZLXMLReader::Tag *HtmlTextConverter::tags() const {
	return TAGS;
}

void HtmlTextConverter::convertBuffer(std::vector<std::string> &buffer) {
	myBuffer = &buffer;
	std::string str = "<t>";
	ZLStringUtil::append(str, buffer);
	str += "</t>";
	buffer.clear();
	ZLStringInputStream stream(str);
	readDocument(stream);
}

void HtmlTextConverter::convertString(std::string &str) {
	std::vector<std::string> buffer;
	myBuffer = &buffer;
	str = "<t>" + str + "</t>";
	ZLStringInputStream stream(str);
	readDocument(stream);
	str.erase();
	ZLStringUtil::append(str, buffer);
}

void HtmlTextConverter::startElementHandler(int, const char **) {
}

void HtmlTextConverter::endElementHandler(int) {
}

void HtmlTextConverter::characterDataHandler(const char *text, int len) {
	if (myBuffer != 0) {
		myBuffer->push_back(std::string());
		myBuffer->back().append(text, len);
	}
}

static std::vector<std::string> EXTERNAL_DTDs;

const std::vector<std::string> &HtmlTextConverter::externalDTDs() const {
	if (EXTERNAL_DTDs.empty()) {
		ZLFSDir dtdPath(DocBookReader::DTDDirectory);
		if (dtdPath.open()) {
			std::vector<std::string> files;
			dtdPath.collectRegularFiles(files);
			dtdPath.close();
			for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); it++) {
				if (ZLStringUtil::stringEndsWith(*it, ".ent")) {
					EXTERNAL_DTDs.push_back(dtdPath.name() + "/" + *it);
				}
			}
		}
	}

	return EXTERNAL_DTDs;
}

HtmlReader::HtmlReader() : myConverter("windows-1251") {
	std::string str = "<t>";
	ZLStringInputStream stream(str);
	myConverter.readDocument(stream);
}

static struct {
	const char *tagName;
	HtmlReader::TagCode tagCode;
} HTML_TAGS[] = {
	{ "body", HtmlReader::_BODY },
	{ "p", HtmlReader::_P },
	{ "br", HtmlReader::_BR },
	{ "li", HtmlReader::_LI },
	{ "title", HtmlReader::_TITLE },
	{ "h1", HtmlReader::_H1 },
	{ "h2", HtmlReader::_H2 },
	{ "h3", HtmlReader::_H3 },
	{ "h4", HtmlReader::_H4 },
	{ "h5", HtmlReader::_H5 },
	{ "h6", HtmlReader::_H6 },
	{ "tt", HtmlReader::_TT },
	{ "b", HtmlReader::_B },
	{ "i", HtmlReader::_I },
	{ "strong", HtmlReader::_STRONG },
	{ "sup", HtmlReader::_SUP },
	{ "sub", HtmlReader::_SUB },
	{ "cite", HtmlReader::_CITE },
	{ "style", HtmlReader::_STYLE },
	{ 0, HtmlReader::_UNKNOWN }
};

HtmlReader::HtmlTag HtmlReader::tag(std::string &name) {
	if (name.length() == 0) {
		return HtmlTag(_UNKNOWN, true);
	}

	bool start = name[0] != '/';
	if (!start) {
		name = name.substr(1);
	}

	if (name.length() > 6) {
		return HtmlTag(_UNKNOWN, start);
	}

	for (unsigned int i = 0; i < name.length(); i++) {
		name[i] = tolower(name[i]);
	}

	for (unsigned int i = 0; ; i++) {
		if ((HTML_TAGS[i].tagName == 0) || HTML_TAGS[i].tagName == name) {
			return HtmlTag(HTML_TAGS[i].tagCode, start);
		}
	}
}

void HtmlReader::readDocument(ZLInputStream &stream) {
	if (!stream.open()) {
		return;
	}

	startDocumentHandler();

	bool insideTag = false;
	bool insideTagName = false;
	std::string tagName;
	
	const size_t BUFSIZE = 2048;
	char buffer[BUFSIZE];
	size_t length;
	bool doBreak = false;
	do {
		length = stream.read(buffer, BUFSIZE);
		char *start = buffer;
		char *endOfBuffer = buffer + length;
		for (char *ptr = buffer; ptr < endOfBuffer; ptr++) {
			if (insideTag) {
				if (*ptr == '>') {
					insideTag = false;
					if (insideTagName) {
						tagName.append(start, ptr - start);
					}
					if (!tagHandler(tag(tagName))) {
						doBreak = true;
						break;
					}
					tagName.erase();
					start = ptr + 1;
				} if (insideTagName && isspace(*ptr)) {
					tagName.append(start, ptr - start);
					insideTagName = false;
				}
			} else {
				if (*ptr == '<') {
					insideTag = true;
					insideTagName = true;
					if (!characterDataHandler(start, ptr - start)) {
						doBreak = true;
						break;
					}
					start = ptr + 1;
				}
			}
		}
		if (doBreak) {
			break;
		}
		if (start != endOfBuffer) {
			if (!insideTag) {
				if (!characterDataHandler(start, endOfBuffer - start)) {
					break;
				}
			} else if (insideTagName) {
				tagName.append(start, endOfBuffer - start);
			}
		}
  } while (length == BUFSIZE);

	endDocumentHandler();

	stream.close();
}
