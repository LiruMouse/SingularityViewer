/** 
 * @file lllogchat.cpp
 * @brief LLLogChat class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include <ctime>
#include "lllogchat.h"
#include "llappviewer.h"
#include "llfloaterchat.h"

#include "boost/algorithm/string.hpp"

extern bool xantispam_check(const std::string&, const std::string&, const std::string&);


//static
std::string LLLogChat::makeLogFileName(std::string filename)
{
	if (gSavedPerAccountSettings.getBOOL("LogFileNamewithDate"))
	{
		time_t now; 
		time(&now); 
		char dbuffer[100];               /* Flawfinder: ignore */
		if (filename == "chat") 
		{ 
			static const LLCachedControl<std::string> local_chat_date_format(gSavedPerAccountSettings, "LogFileLocalChatDateFormat", "-%Y-%m-%d");
			strftime(dbuffer, 100, local_chat_date_format().c_str(), localtime(&now));
		} 
		else 
		{ 
			static const LLCachedControl<std::string> ims_date_format(gSavedPerAccountSettings, "LogFileIMsDateFormat", "-%Y-%m");
			strftime(dbuffer, 100, ims_date_format().c_str(), localtime(&now));
		} 
		filename += dbuffer; 
	}
	filename = cleanFileName(filename);
	filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_ACCOUNT_CHAT_LOGS,filename);
	filename += ".txt";
	return filename;
}

std::string LLLogChat::cleanFileName(std::string filename)
{
	// [Ratany: When this isn't trimmed, there will be (at least) two different
	// file names, one with space/underscore and one without. /Ratany]
	boost::algorithm::trim(filename);
	std::string invalidChars = "\"\'\\/?*:<>|[]{}~"; // Cannot match glob or illegal filename chars
	S32 position = filename.find_first_of(invalidChars);
	while (position != filename.npos)
	{
		filename[position] = '_';
		position = filename.find_first_of(invalidChars, position);
	}
	return filename;
}

std::string LLLogChat::timestamp(bool withdate)
{
	time_t utc_time;
	utc_time = time_corrected();

	// There's only one internal tm buffer.
	struct tm* timep;

	// Convert to Pacific, based on server's opinion of whether
	// it's daylight savings time there.
	timep = utc_to_pacific_time(utc_time, gPacificDaylightTime);

	static LLCachedControl<bool> withseconds("SecondsInLog");
	std::string text;
	if (withdate)
		if (withseconds)
			text = llformat("[%d/%02d/%02d %02d:%02d:%02d]  ", (timep->tm_year-100)+2000, timep->tm_mon+1, timep->tm_mday, timep->tm_hour, timep->tm_min, timep->tm_sec);
		else
			text = llformat("[%d/%02d/%02d %02d:%02d]  ", (timep->tm_year-100)+2000, timep->tm_mon+1, timep->tm_mday, timep->tm_hour, timep->tm_min);
	else
		if (withseconds)
			text = llformat("[%02d:%02d:%02d]  ", timep->tm_hour, timep->tm_min, timep->tm_sec);
		else
			text = llformat("[%02d:%02d]  ", timep->tm_hour, timep->tm_min);

	return text;
}


//static
void LLLogChat::saveHistory(std::string const& filename, std::string line)
{
	if(!filename.size())
	{
		llinfos << "Filename is Empty!" << llendl;
		return;
	}

	LLFILE* fp = LLFile::fopen(LLLogChat::makeLogFileName(filename), "a"); 		/*Flawfinder: ignore*/
	if (!fp)
	{
		llinfos << "Couldn't open chat history log!" << llendl;
	}
	else
	{
		fprintf(fp, "%s\n", line.c_str());
		
		fclose (fp);
	}
}

static long const LOG_RECALL_BUFSIZ = 2048;

long LLLogChat::computeFileposition(LLFILE *fptr, U32 lines)
{
	// Set pos to point to the last byte of the file, if any.
	if(fseek(fptr, 0, SEEK_END))
	{
		return -1;
	}
	long pos = ftell(fptr);
	if(pos == -1)
	{
		return -1;
	}

	llinfos << "starting at end-pos: " << pos << llendl;

	char buffer[LOG_RECALL_BUFSIZ];
	std::size_t nlines = 0;
	std::size_t linepos = 0;
	while((pos > 0) && (nlines < lines))
	{
		// reposition file pointer to before pos
		size_t size = llmin(LOG_RECALL_BUFSIZ, pos);
		pos -= size;

		fseek(fptr, pos, SEEK_SET);
		if(fread(buffer, 1, size, fptr) != size)
		{
			return -1;
		}

		// Count the number of newlines in the buffer and set
		// pos to the beginning of the first line to return
		// when we found enough.
		for (char const* p = buffer + size - 1; p >= buffer; --p)
		{
			if (*p == '\n')
			{
				nlines++;
				if(nlines > lines)
				{
					linepos = pos + p - buffer + 1;
					llinfos << "linepos: " << linepos << ", lines: " << nlines << llendl;
					return linepos;
				}
			}
		}
	}

	// maybe there aren't so many lines in the file
	return linepos;
}


void LLLogChat::loadHistory(std::string const& filename , void (*callback)(ELogLineType, std::string, void*), void* userdata)
{
	// The number of lines to read.
	U32 lines = gSavedSettings.getU32("LogShowHistoryLines");

	if(!lines || filename.empty())
	{
		callback(LOG_EMPTY, LLStringUtil::null, userdata);
		return;
	}

	// Open the log file.
	// reading in binary mode might disable newline conversions
	LLFILE* fptr = LLFile::fopen(makeLogFileName(filename), "rb");
	if (!fptr)
	{
		callback(LOG_EMPTY, LLStringUtil::null, userdata);
		return;
	}

	long pos = 0;
	if(xantispam_check(filename, "&-IM/GRLogFullHistory", filename))
	{
		pos = computeFileposition(fptr, lines);
	}

	if(pos < 0)
	{
		callback(LOG_EMPTY, LLStringUtil::null, userdata);
	}
	else
	{
		// Set the file pointer at the first line to read.
	llinfos << "setting to pos: " << pos << llendl;
		if(!fseek(fptr, pos, SEEK_SET))
		{
	llinfos << "is at pos: " << ftell(fptr) << llendl;
	std::size_t nread = 0;
			// Read lines from the file one by one until we reach the end of the file.
			char buffer[LOG_RECALL_BUFSIZ];
			while(fgets(buffer, LOG_RECALL_BUFSIZ, fptr) == buffer)
			{
				size_t len = strlen(buffer);
				if(len > 0)
				{
					// fgets() does null-terminate the buffer on success
					// overwrite a possible EOF
					buffer[len - 1] = '\n';
				}

				callback(LOG_LINE, buffer, userdata);
				nread++;
				llinfos << "line: " << buffer << llendl;
			}
			callback(LOG_END, LLStringUtil::null, userdata);
	llinfos << "lines read: " << nread << llendl;
		}
		else
		{
			callback(LOG_EMPTY, LLStringUtil::null, userdata);
		}
	}

	clearerr(fptr);
	fclose(fptr);
}
