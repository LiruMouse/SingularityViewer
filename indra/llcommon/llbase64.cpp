/** 
 * @file llbase64.cpp
 * @brief Wrapper for apr base64 encoding that returns a std::string
 * @author James Cook
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llbase64.h"

#include <string>
#include "apr_base64.h"

// static
std::string LLBase64::encode(const U8* input, size_t input_size)
{
	if (!(input && input_size > 0)) return LLStringUtil::null;

	// Yes, it returns int.
	int b64_buffer_length = apr_base64_encode_len(input_size);
	char* b64_buffer = new char[b64_buffer_length];
	
	// This is faster than apr_base64_encode() if you know
	// you're not on an EBCDIC machine.  Also, the output is
	// null terminated, even though the documentation doesn't
	// specify.  See apr_base64.c for details. JC
	b64_buffer_length = apr_base64_encode_binary(
		b64_buffer,
		input,
		input_size);
	std::string result;
	result.assign(b64_buffer);
	delete[] b64_buffer;

	return result;
}

// static
std::string LLBase64::encode(const std::string& in_str)
{
	size_t data_size = in_str.size();
	std::vector<U8> data;
	data.resize(data_size);
	memcpy(&data[0], in_str.c_str(), data_size);
	return encode(&data[0], data_size);
}

// static
size_t LLBase64::decode(const std::string& input, U8 * buffer, size_t buffer_size)
{
	if (input.empty()) return 0;

	size_t bytes_written = apr_base64_decode_binary(buffer, input.data());

	return bytes_written;
}

std::string LLBase64::decode(const std::string& input)
{
	U32 buffer_len = LLBase64::requiredDecryptionSpace(input);
	std::vector<U8> buffer(buffer_len);
	buffer_len = LLBase64::decode(input, &buffer[0], buffer_len);
	buffer.resize(buffer_len);
	return std::string(reinterpret_cast<const char*>(&buffer[0]), buffer_len);
}

// static
size_t LLBase64::requiredDecryptionSpace(const std::string& str)
{
	size_t len = str.length();
	U32 padding = 0;
 
	if (str[len - 1] == '=' && str[len - 2] == '=') //last two chars are =
		padding = 2;
	else if (str[len - 1] == '=') //last char is =
		padding = 1;

	return len * 0.75 - padding;
}

