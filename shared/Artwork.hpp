//
//  Artwork.hpp
//  plugin_taglib
//
//  Created by Danny on 04/06/2020.
//

#ifndef Artwork_hpp
#define Artwork_hpp

#include <stdio.h>
#include <string>
#include "utfString.hpp"
#include "taglib/taglib.h"
#include "taglib/fileref.h"
#include "taglib/mpegfile.h"
#include "taglib/id3v2tag.h"
#include "taglib/attachedpictureframe.h"
#include "taglib/mp4file.h"
#include "taglib/mp4tag.h"
#include "taglib/mp4coverart.h"
#include "taglib/commentsframe.h"
#include "taglib/popularimeterframe.h"
#include "taglib/flacfile.h"
#include "taglib/xiphcomment.h"

using namespace std;

class Artwork
{
	public:
	class ImageFile : public TagLib::File
	{
	public:
		#ifdef _WIN32
		ImageFile(wstring file) : TagLib::File(file.c_str())
		{

		}
		#else
		ImageFile(string file) : TagLib::File(file.c_str())
		{

		}
		#endif

		TagLib::ByteVector data()
		{
			return readBlock(length());
		}

	private:
		virtual TagLib::Tag* tag() const { return 0; }
		virtual TagLib::AudioProperties* audioProperties() const { return 0; }
		virtual bool save() { return false; }
	};
	
	public:
	#ifdef _WIN32
	static bool WriteCoverToFile(const TagLib::ByteVector& data, const wstring& target);
	static bool GetMp3Artwork(wstring filePath, TagLib::String pictureFilePath);
	static bool SetMp3Artwork(wstring filePath, wstring pictureFilePath);
	static bool GetFlacArtwork(wstring filePath, TagLib::String pictureFilePath);
	static bool SetFlacArtwork(wstring filePath, wstring pictureFilePath);
	static bool GetMP4Artwork(wstring filePath, TagLib::String pictureFilePath);
	static bool SetMP4Artwork(wstring filePath, wstring pictureFilePath);
	#else
	static bool WriteCoverToFile(const TagLib::ByteVector& data, const string& target);
	static bool GetMp3Artwork(string filePath, TagLib::String pictureFilePath);
	static bool SetMp3Artwork(string filePath, string pictureFilePath);
	static bool GetFlacArtwork(string filePath, TagLib::String pictureFilePath);
	static bool SetFlacArtwork(string filePath, string pictureFilePath);
	static bool GetMP4Artwork(string filePath, TagLib::String pictureFilePath);
	static bool SetMP4Artwork(string filePath, string pictureFilePath);
	#endif
};

#endif /* Artwork_hpp */
