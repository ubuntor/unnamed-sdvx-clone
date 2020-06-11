#include "stdafx.h"
#include "TextStream.hpp"

bool TextStream::ReadLine(BinaryStream& stream, String& out, const String& lineEnding /*= "\r\n"*/)
{
	out.clear();
	size_t max = stream.GetSize();
	size_t pos = stream.Tell();
	while(pos < max)
	{
		char c;
		stream << c;
		out.push_back(c);
		if(out.size() >= lineEnding.size())
		{
			// Compare end of output with line ending
			auto liStart = out.end() - lineEnding.size();
			for(size_t i = 0; i < lineEnding.size(); i++)
			{
				if(lineEnding[i] != *(liStart + i))
					goto _continue;
			}
			out.erase(liStart, out.end());
			return true;
		}
		_continue:
		pos++;
	}
	return out.size() > 0;
}
void TextStream::Write(BinaryStream& stream, const String& out)
{
	for(char c : out)
	{
		stream << c;
	}
}
void TextStream::WriteLine(BinaryStream& stream, const String& out, const String& lineEnding /*= "\r\n"*/)
{
	Write(stream, out);
	Write(stream, lineEnding);
}
bool TextStream::ReadAll(BinaryStream& stream, String& out) {
	//TODO: This is slow but binarystream doesn't have enough options
	out.clear();
	size_t max = stream.GetSize();
	size_t pos = stream.Tell();
	while (pos < max)
	{
		char c;
		stream << c;
		out.push_back(c);
		pos++;
	}
	return out.size() > 0;
}