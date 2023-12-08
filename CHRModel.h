#pragma once

#include <array>
#include <windows.h>
#include <fstream>
#include <vector>

#include "CHRModel.h"
#define NOT_FOUND -2


using NESPalette = std::array<std::array<uint8_t, 3>, 64>;

//this is an RGB approximation of the NES' native color palette
//provided by b0b_d0e
const NESPalette basePaletteColors = { {
	{ 84,  84,  84}, {  0,  30, 116}, {  8,  16, 144}, { 48,   0, 136}, { 68,   0, 100}, { 92,   0,  48}, { 84,   4,   0}, { 60,  24,   0}, { 32,  42,   0}, {  8,  58,   0}, {  0,  64,   0}, {  0,  60,   0}, {  0,  50,  60}, {  0,   0,   0}, {  0,   0,   0}, {  0,   0,   0},
	{152, 150, 152}, {  8,  76, 196}, { 48,  50, 236}, { 92,  30, 228}, {136,  20, 176}, {160,  20, 100}, {152,  34,  32}, {120,  60,   0}, { 84,  90,   0}, { 40, 114,   0}, {  8, 124,   0}, {  0, 118,  40}, {  0, 102, 120}, {  0,   0,   0}, {  0,   0,   0}, {  0,   0,   0},
	{236, 238, 236}, { 76, 14, 236}, {120, 124, 236}, {176,  98, 236}, {228,  84, 236}, {236,  88, 180}, {236, 106, 100}, {212, 136,  32}, {160, 170,   0}, {116, 196,   0}, { 76, 208,  32}, { 56, 204, 108}, { 56, 180, 204}, { 60,  60,  60}, {  0,   0,   0}, {  0,   0,   0},
	{236, 238, 236}, {168, 204, 236}, {188, 188, 236}, {212, 178, 236}, {236, 174, 236}, {236, 174, 212}, {236, 180, 176}, {228, 196, 144}, {204, 210, 120}, {180, 222, 120}, {168, 226, 144}, {152, 226, 180}, {160, 214, 228}, {160, 162, 160}, {  0,   0,   0}, {  0,   0,   0},
} };

class CHRModel
{
public:

	std::vector<uint8_t> IndexArr;
	int prgPad = 0;
	int size;
	
	CHRModel()
	{
		//default constructor makes blank 4kb CHR
		IndexArr.resize(16384);
		size = 4096;
	}

	CHRModel(const std::string& filename)
	{
		//this constructor accepts an std::string and is the main
		//use for this library

		//open file in binary mode
		std::ifstream file(filename, std::ios::binary | std::ios::ate);

		//check file extension
		bool isROM = (filename.find(".nes") + (filename.find(".NES")) != NOT_FOUND);

		if (isROM)
		{
			//read PRG size from INES header
			file.seekg(4);
			//set pad to skip over PRG
			prgPad = 16 + (file.get() * 16384);
		}

		//copy file contents to std::vector
		std::vector<uint8_t> chr_data = std::vector<BYTE>((std::istreambuf_iterator<char>(file.seekg(prgPad))),
			std::istreambuf_iterator<char>());

		//set global size variable (needed for saving file)
		size = chr_data.size();
		//resize x4 because 2 bytes is 8 pixels in CHR (8 bits / 2bpp)
		IndexArr.resize(size * 4);
		LoadCHR(chr_data);
		file.close();
	}

	void LoadCHR(const std::vector<uint8_t>& chr_data)
	{
		int x = 0;
		int y = 0;
		//loop through all of the raw bytes in different sections
		//sections are as follows
		for (int tileRow = 0; tileRow < size / 256; tileRow++)
		{
			//row of 16 8px*8px tiles
			for (int rowOfPixels = 0; rowOfPixels < 8; rowOfPixels++)
			{
				//row of 128 pixels
				for (int row_in_tile = 0; row_in_tile < 16; row_in_tile++)
				{
					//row of 8 pixels within each tile
					for (int bits = 7; bits >= 0; bits--)
					{
						//this is where the 2 plane bytes are reduced
						//into 8 seperate pixels
						//1 loop nets 1 pixel stored to the array

						uint8_t mask_value = 1 << bits;
						//the planes are 8 bytes away, so we add 8 to the index for the 2nd plane
						uint8_t plane1 = chr_data[row_in_tile * 16 + rowOfPixels + tileRow * 256] & mask_value;
						uint8_t plane2 = chr_data[row_in_tile * 16 + rowOfPixels + tileRow * 256 + 8] & mask_value;
						plane1 = plane1 >> bits;
						plane2 = plane2 >> bits;
						//the index tells you which of the 4 colors is stored at that pixels location
						uint8_t index = (plane1 + 2 * plane2);
						//and the IndexArr is where all of the decoded indices are stored
						IndexArr[y * 128 + x] = index;
						//this is a rather ugly way of keeping track of where to store the 
						//index, since I couldn't figure out the maths to decode it from the
						//other available values
						x++;
						if (x >= 128)
						{
							x = 0;
							y++;
						}
					}
				}
			}
		}
	}

	void WritePixel(int x, int y, int index)
	{
		IndexArr[x + y * 128] = index;
	}

	void SaveCHR(std::string filename)
	{
		char* CHROutput = new char[size * 8];

		//this is almost exactly the same loop from the constructor
		for (int TileRow = 0; TileRow < size / 256; TileRow++)
		{
			for (int Tiles = 0; Tiles < 16; Tiles++)
			{
				for (int PixRow = 0; PixRow < 8; PixRow++)
				{
					//GetPlane() condenses sets of 8 pixels/indices into sets of 2 bytes
					std::vector<uint8_t> ByteSet = GetPlane(Tiles + (PixRow * 16) + (TileRow * 128));
					//and then the 2 bytes from the returned vector are placed in the
					//output array 8 bytes apart. Just how we found them
					CHROutput[Tiles * 16 + PixRow + (TileRow * 256)] = ByteSet[0];
					CHROutput[((Tiles * 16) + PixRow + (TileRow * 256)) + 8] = ByteSet[1];
				}
			}
		}

		//if we don't use an fstream to read/write, and just try to write
		//it would delete all the PRG data from a rom
		std::fstream Output(filename, std::ios::binary | std::ios::in | std::ios::out);

		//bypass the PRG ROM
		Output.seekp(prgPad);
		//and finally. . .output the CHR data!!!
		Output.write(CHROutput, size);
		Output.close();
		//should probably have used a vector, but I'm not too worried about it
		delete[] CHROutput;
	}

	std::vector<uint8_t> GetPlane(int offset)
	{
		//all this does is run through 8 pixels from an offset
		//and separete out the 2 bits per pixel and combine them into 2 planes/bytes
		uint8_t plane1 = 0;
		uint8_t plane2 = 0;
		for (int Pix = 0; Pix < 8; Pix++)
		{
			plane1 = (plane1 << 1) + (IndexArr[Pix + (offset * 8)]) % 2;
			plane2 = (plane2 << 1) + (((IndexArr[Pix + (offset * 8)] >> 1)) % 2);
		}
		//and returns a vector
		std::vector<uint8_t> ByteSet = { plane1, plane2 };
		return ByteSet;
	}
};