#pragma once

namespace Image
{
	enum
	{
		Unknown = 0,
		G		= 1,
		GA		= 2,
		RGB		= 3,
		RGBA	= 4
	};
	unsigned char* Load(char const* FileName, int* Width, int* Height, int* ChannelsInFile, int DesiredChannels);
	unsigned char* LoadFromMemory(unsigned char const* Buffer, int Len, int* Width, int* Height, int* ChannelsInFile, int DesiredChannels);
	void Free( void * Data );
}