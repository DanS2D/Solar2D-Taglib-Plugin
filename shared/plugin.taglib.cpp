#include "CoronaAssert.h"
#include "CoronaEvent.h"
#include "CoronaLua.h"
#include "CoronaLibrary.h"
#include <string.h>
#include <stdio.h>
#include <fstream>
#define TAGLIB_STATIC
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

// ----------------------------------------------------------------------------

namespace Corona
{
	// ----------------------------------------------------------------------------

	class TagLibLibrary
	{
	public:
		typedef TagLibLibrary Self;

	public:
		static const char kName[];

	public:
		static int Open(lua_State* L);
		static int Finalizer(lua_State* L);
		static Self* ToLibrary(lua_State* L);

	protected:
		TagLibLibrary();
		bool Initialize(void* platformContext);

	public:
		static int get(lua_State* L);
		static int getArtwork(lua_State* L);
		static int set(lua_State* L);
		static int setArtwork(lua_State* L);

	public:
		class ImageFile : public TagLib::File
		{
		public:
			ImageFile(std::wstring file) : TagLib::File(file.c_str())
			{

			}

			TagLib::ByteVector data()
			{
				return readBlock(length());
			}

		private:
			virtual TagLib::Tag* tag() const { return 0; }
			virtual TagLib::AudioProperties* audioProperties() const { return 0; }
			virtual bool save() { return false; }
		};
	};

	// ----------------------------------------------------------------------------

	// This corresponds to the name of the library, e.g. [Lua] require "plugin.library"
	const char TagLibLibrary::kName[] = "plugin.taglib";

	int TagLibLibrary::Open(lua_State* L)
	{
		// Register __gc callback
		const char kMetatableName[] = __FILE__; // Globally unique string to prevent collision
		CoronaLuaInitializeGCMetatable(L, kMetatableName, Finalizer);

		//CoronaLuaInitializeGCMetatable( L, kMetatableName, Finalizer );
		void* platformContext = CoronaLuaGetContext(L);

		// Set library as upvalue for each library function
		Self* library = new Self;

		if (library->Initialize(platformContext))
		{
			// Functions in library
			static const luaL_Reg kFunctions[] =
			{
				{ "get", get },
				{ "getArtwork", getArtwork },
				{ "set", set },
				{ "setArtwork", setArtwork },
				{ NULL, NULL }
			};

			// Register functions as closures, giving each access to the
			// 'library' instance via ToLibrary()
			{
				CoronaLuaPushUserdata(L, library, kMetatableName);
				luaL_openlib(L, kName, kFunctions, 1); // leave "library" on top of stack
			}
		}

		return 1;
	}

	int TagLibLibrary::Finalizer(lua_State* L)
	{
		Self* library = (Self*)CoronaLuaToUserdata(L, 1);
		if (library)
		{
			delete library;
		}

		return 0;
	}

	TagLibLibrary* TagLibLibrary::ToLibrary(lua_State* L)
	{
		// library is pushed as part of the closure
		Self* library = (Self*)CoronaLuaToUserdata(L, lua_upvalueindex(1));
		return library;
	}

	TagLibLibrary::TagLibLibrary()
	{
	}

	bool TagLibLibrary::Initialize(void* platformContext)
	{
		return 1;
	}

	// ----------------------------------------------------------------------------

	static std::wstring utf8_to_utf16(const std::string& utf8)
	{
		std::vector<unsigned long> unicode;
		size_t i = 0;
		while (i < utf8.size())
		{
			unsigned long uni;
			size_t todo;
			bool error = false;
			unsigned char ch = utf8[i++];
			if (ch <= 0x7F)
			{
				uni = ch;
				todo = 0;
			}
			else if (ch <= 0xBF)
			{
				throw std::logic_error("not a UTF-8 string");
			}
			else if (ch <= 0xDF)
			{
				uni = ch & 0x1F;
				todo = 1;
			}
			else if (ch <= 0xEF)
			{
				uni = ch & 0x0F;
				todo = 2;
			}
			else if (ch <= 0xF7)
			{
				uni = ch & 0x07;
				todo = 3;
			}
			else
			{
				throw std::logic_error("not a UTF-8 string");
			}
			for (size_t j = 0; j < todo; ++j)
			{
				if (i == utf8.size())
					throw std::logic_error("not a UTF-8 string");
				unsigned char ch = utf8[i++];
				if (ch < 0x80 || ch > 0xBF)
					throw std::logic_error("not a UTF-8 string");
				uni <<= 6;
				uni += ch & 0x3F;
			}
			if (uni >= 0xD800 && uni <= 0xDFFF)
				throw std::logic_error("not a UTF-8 string");
			if (uni > 0x10FFFF)
				throw std::logic_error("not a UTF-8 string");
			unicode.push_back(uni);
		}
		std::wstring utf16;
		for (size_t i = 0; i < unicode.size(); ++i)
		{
			unsigned long uni = unicode[i];
			if (uni <= 0xFFFF)
			{
				utf16 += (wchar_t)uni;
			}
			else
			{
				uni -= 0x10000;
				utf16 += (wchar_t)((uni >> 10) + 0xD800);
				utf16 += (wchar_t)((uni & 0x3FF) + 0xDC00);
			}
		}
		return utf16;
	}

	static bool WriteCoverToFile(const TagLib::ByteVector& data, const std::wstring& target)
	{
		FILE* f = _wfopen(target.c_str(), L"wb");

		if (f)
		{
			const bool written = fwrite(data.data(), 1, data.size(), f) == data.size();
			fclose(f);
			return written;
		}

		return false;
	}

	// ################################### MP3 ARTWORK ################################### //
	static bool GetMp3Artwork(std::wstring filePath, TagLib::String pictureFilePath)
	{
		bool created = false;
		TagLib::MPEG::File mp3File(filePath.c_str());

		if (mp3File.isValid())
		{
			const TagLib::ID3v2::FrameList& frameList = mp3File.ID3v2Tag()->frameList("APIC");

			if (!frameList.isEmpty())
			{
				// Just grab the first image.
				const auto* pictureFrame = (TagLib::ID3v2::AttachedPictureFrame*)frameList.front();

				if (pictureFrame != NULL)
				{
					TagLib::String mimeType = pictureFrame->mimeType();
					TagLib::String fullPath = pictureFilePath;

					if (mimeType == "image/png")
					{
						fullPath.append(".png");
					}
					else
					{
						fullPath.append(".jpg");
					}

					created = WriteCoverToFile(pictureFrame->picture(), utf8_to_utf16(fullPath.toCString()));
				}
			}
		}

		return created;
	}

	static bool SetMp3Artwork(std::wstring filePath, std::wstring pictureFilePath)
	{
		bool saved = false;
		TagLib::MPEG::File mp3File(filePath.c_str());

		if (mp3File.isValid())
		{
			TagLib::ID3v2::Tag* mp3Tag = mp3File.ID3v2Tag();
			auto framelist = mp3Tag->frameListMap()["APIC"];

			if (!framelist.isEmpty())
			{
				for (auto it = mp3Tag->frameList().begin(); it != mp3Tag->frameList().end(); ++it)
				{
					auto frameID = (*it)->frameID();
					std::string framestr{ frameID.data(), frameID.size() };

					if (framestr.compare("APIC") == 0)
					{
						mp3Tag->removeFrame((*it));
						it = mp3Tag->frameList().begin();
					}
				}
			}

			TagLibLibrary::ImageFile img(pictureFilePath);
			auto picframe = new TagLib::ID3v2::AttachedPictureFrame;
			picframe->setPicture(img.data());
			picframe->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
			mp3Tag->addFrame(picframe);
			mp3File.save();
			saved = true;
		}

		return saved;
	}

	// ################################### FLAC ARTWORK ################################### //
	static bool GetFlacArtwork(std::wstring filePath, TagLib::String pictureFilePath)
	{
		bool created = false;
		TagLib::FLAC::File flacFile(filePath.c_str());

		if (flacFile.isValid())
		{
			const TagLib::List<TagLib::FLAC::Picture*>& picList = flacFile.pictureList();

			if (!picList.isEmpty())
			{
				const TagLib::FLAC::Picture* picture = picList[0];

				if (picture != NULL)
				{
					TagLib::String mimeType = picture->mimeType();
					TagLib::String fullPath = pictureFilePath;

					if (mimeType == "image/png")
					{
						fullPath.append(".png");
					}
					else
					{
						fullPath.append(".jpg");
					}

					created = WriteCoverToFile(picture->data(), utf8_to_utf16(fullPath.toCString()));
				}
			}
		}

		return created;
	}

	static bool SetFlacArtwork(std::wstring filePath, std::wstring pictureFilePath)
	{
		bool saved = false;
		TagLib::FLAC::File flacFile(filePath.c_str());

		if (flacFile.isValid())
		{
			TagLib::FLAC::Picture* picture = new TagLib::FLAC::Picture();
			TagLibLibrary::ImageFile img(pictureFilePath);
			picture->setData(img.data());
			picture->setType(TagLib::FLAC::Picture::FrontCover);
			picture->setDescription("Front Cover");
			auto framelist = flacFile.pictureList();

			for (auto it = framelist.begin(); it != framelist.end(); ++it)
			{
				TagLib::FLAC::Picture* currentPicture = *it;

				if (currentPicture->type() == TagLib::FLAC::Picture::FrontCover)
				{
					flacFile.removePicture(currentPicture);
					break;
				}
			}

			flacFile.addPicture(picture);
			flacFile.save();
			saved = true;
		}

		return saved;
	}

	// ################################### MP4 ARTWORK ################################### //
	static bool GetMP4Artwork(std::wstring filePath, TagLib::String pictureFilePath)
	{
		bool created = false;

		TagLib::MP4::File mp4File(filePath.c_str());

		if (mp4File.isValid())
		{
			const TagLib::MP4::ItemListMap& itemListMap = mp4File.tag()->itemListMap();

			if (itemListMap.contains("covr"))
			{
				const TagLib::MP4::CoverArtList& coverArtList = itemListMap["covr"].toCoverArtList();

				if (!coverArtList.isEmpty())
				{
					const TagLib::MP4::CoverArt* picture = &(coverArtList.front());
					TagLib::MP4::CoverArt::Format mimeType = picture->format();
					TagLib::String fullPath = pictureFilePath;

					if (mimeType == TagLib::MP4::CoverArt::PNG)
					{
						fullPath.append(".png");
					}
					else if (mimeType == TagLib::MP4::CoverArt::JPEG)
					{
						fullPath.append(".jpg");
					}

					created = WriteCoverToFile(picture->data(), utf8_to_utf16(fullPath.toCString()));
				}
			}
		}

		return created;
	}

	static bool SetMP4Artwork(std::wstring filePath, std::wstring pictureFilePath)
	{
		bool saved = false;

		TagLib::MP4::File mp4File(filePath.c_str());

		if (mp4File.isValid())
		{
			TagLib::MP4::CoverArtList coverArtList = mp4File.tag()->item("covr").toCoverArtList();
			TagLibLibrary::ImageFile img(pictureFilePath);
			TagLib::String fullPath = pictureFilePath;
			coverArtList.clear();

			if (fullPath.substr(fullPath.size() - 3).upper() == "JPG" || fullPath.substr(fullPath.size() - 3).upper() == "JPEG")
			{
				coverArtList.append(TagLib::MP4::CoverArt(TagLib::MP4::CoverArt::JPEG, img.data()));
			}
			else if (fullPath.substr(fullPath.size() - 3).upper() == "PNG")
			{
				coverArtList.append(TagLib::MP4::CoverArt(TagLib::MP4::CoverArt::PNG, img.data()));
			}

			mp4File.tag()->setItem("covr", coverArtList);
			mp4File.save();
			saved = true;
		}

		return saved;
	}

	// ----------------------------------------------------------------------------

	// 
	int TagLibLibrary::get(lua_State* L)
	{
		const char* fileName;
		const char* filePath;
		int rating = 0;

		if (lua_istable(L, 1))
		{
			lua_getfield(L, 1, "fileName");
			if (lua_isstring(L, -1))
			{
				fileName = lua_tostring(L, -1);
			}
			else
			{
				CoronaLuaError(L, "taglib.get() options.filename (string) expected, got: %s", lua_typename(L, -1));
				lua_pushnil(L);
			}
			lua_pop(L, 1);

			lua_getfield(L, 1, "filePath");
			if (lua_isstring(L, -1))
			{
				filePath = lua_tostring(L, -1);
			}
			else
			{
				CoronaLuaError(L, "taglib.get() options.filePath (string) expected, got: %s", lua_typename(L, -1));
				lua_pushnil(L);
			}
			lua_pop(L, 1);
		}
		else
		{
			CoronaLuaError(L, "taglib.get() options (table) expected, got: %s", lua_typename(L, 1));
			lua_pushnil(L);
			return 1;
		}

		if (fileName != NULL && filePath != NULL)
		{
			std::string fullPath = filePath;
			fullPath.append("\\");
			fullPath.append(fileName);
			std::wstring utf16Path = utf8_to_utf16(fullPath);
			TagLib::String fName = fileName;
			TagLib::String fileType = fName.substr(fName.size() - 3).upper();

			if (fileType == "MP3")
			{
				TagLib::MPEG::File mp3File(utf16Path.c_str());

				if (mp3File.isValid() && mp3File.hasID3v2Tag())
				{
					if (!mp3File.ID3v2Tag()->frameList("POPM").isEmpty() && mp3File.ID3v2Tag()->frameList("POPM").front())
					{
						rating = dynamic_cast<TagLib::ID3v2::PopularimeterFrame*>(mp3File.ID3v2Tag()->frameList("POPM").front())->rating();
					}
				}
			}

			TagLib::FileRef file(utf16Path.c_str());

			if (!file.isNull() && file.tag())
			{
				TagLib::AudioProperties* properties = file.audioProperties();
				int seconds = properties->length() % 60;
				int minutes = (properties->length() - seconds) / 60;

				lua_newtable(L);
				// title
				lua_pushstring(L, file.tag()->title().toCString(true));
				lua_setfield(L, -2, "title");
				// artist
				lua_pushstring(L, file.tag()->artist().toCString(true));
				lua_setfield(L, -2, "artist");
				// album
				lua_pushstring(L, file.tag()->album().toCString(true));
				lua_setfield(L, -2, "album");
				// genre
				lua_pushstring(L, file.tag()->genre().toCString(true));
				lua_setfield(L, -2, "genre");
				// comment
				lua_pushstring(L, file.tag()->comment().toCString(true));
				lua_setfield(L, -2, "comment");
				// year
				lua_pushnumber(L, file.tag()->year());
				lua_setfield(L, -2, "year");
				// track number
				lua_pushnumber(L, file.tag()->track());
				lua_setfield(L, -2, "trackNumber");
				// rating
				lua_pushnumber(L, rating);
				lua_setfield(L, -2, "rating");
				// duration seconds
				lua_pushnumber(L, seconds);
				lua_setfield(L, -2, "durationSeconds");
				// duration minutes
				lua_pushnumber(L, minutes);
				lua_setfield(L, -2, "durationMinutes");
				// bitrate
				lua_pushnumber(L, properties->bitrate());
				lua_setfield(L, -2, "bitrate");
				// sample rate
				lua_pushnumber(L, properties->sampleRate());
				lua_setfield(L, -2, "sampleRate");
			}
			else
			{
				lua_newtable(L);
				// title
				lua_pushstring(L, "");
				lua_setfield(L, -2, "title");
				// artist
				lua_pushstring(L, "");
				lua_setfield(L, -2, "artist");
				// album
				lua_pushstring(L, "");
				lua_setfield(L, -2, "album");
				// genre
				lua_pushstring(L, "");
				lua_setfield(L, -2, "genre");
				// comment
				lua_pushstring(L, "");
				lua_setfield(L, -2, "comment");
				// year
				lua_pushnumber(L, 0);
				lua_setfield(L, -2, "year");
				// track number
				lua_pushnumber(L, 0);
				lua_setfield(L, -2, "trackNumber");
				// rating
				lua_pushnumber(L, rating);
				lua_setfield(L, -2, "rating");
				// duration seconds
				lua_pushnumber(L, 0);
				lua_setfield(L, -2, "durationSeconds");
				// duration minutes
				lua_pushnumber(L, 0);
				lua_setfield(L, -2, "durationMinutes");
				// bitrate
				lua_pushnumber(L, 0);
				lua_setfield(L, -2, "bitrate");
				// sample rate
				lua_pushnumber(L, 0);
				lua_setfield(L, -2, "sampleRate");
			}
		}

		return 1;
	}

	int TagLibLibrary::getArtwork(lua_State* L)
	{
		const char* fileName;
		const char* filePath;
		const char* pictureFileName;
		const char* pictureFilePath;

		if (lua_istable(L, 1))
		{
			lua_getfield(L, 1, "fileName");
			if (lua_isstring(L, -1))
			{
				fileName = lua_tostring(L, -1);
			}
			else
			{
				CoronaLuaError(L, "taglib.getArtwork() options.filename (string) expected, got: %s", lua_typename(L, -1));
				lua_pushnil(L);
			}
			lua_pop(L, 1);

			lua_getfield(L, 1, "filePath");
			if (lua_isstring(L, -1))
			{
				filePath = lua_tostring(L, -1);
			}
			else
			{
				CoronaLuaError(L, "taglib.getArtwork() options.filePath (string) expected, got: %s", lua_typename(L, -1));
				lua_pushnil(L);
			}
			lua_pop(L, 1);

			lua_getfield(L, 1, "imageFileName");
			if (lua_isstring(L, -1))
			{
				pictureFileName = lua_tostring(L, -1);
			}
			else
			{
				CoronaLuaError(L, "taglib.getArtwork() options.imageFileName (string) expected, got: %s", lua_typename(L, -1));
				lua_pushnil(L);
			}
			lua_pop(L, 1);

			lua_getfield(L, 1, "imageFilePath");
			if (lua_isstring(L, -1))
			{
				pictureFilePath = lua_tostring(L, -1);
			}
			else
			{
				CoronaLuaError(L, "taglib.getArtwork() options.imageFilePath (string) expected, got: %s", lua_typename(L, -1));
				lua_pushnil(L);
			}
			lua_pop(L, 1);
		}
		else
		{
			CoronaLuaError(L, "taglib.getArtwork() options (table) expected, got: %s", lua_typename(L, 1));
			lua_pushnil(L);
			return 1;
		}

		if (fileName != NULL && filePath != NULL && pictureFileName != NULL && pictureFilePath != NULL)
		{
			std::string fullPath = filePath;
			fullPath.append("\\");
			fullPath.append(fileName);

			std::string pictureFullPath = pictureFilePath;
			pictureFullPath.append("\\");
			pictureFullPath.append(pictureFileName);

			std::wstring utf16FilePath = utf8_to_utf16(fullPath);
			std::wstring utf16PictureFilePath = utf8_to_utf16(pictureFullPath);
			TagLib::String fName = fileName;

			if (fName.substr(fName.size() - 3).upper() == "MP3")
			{
				lua_pushboolean(L, GetMp3Artwork(utf16FilePath, utf16PictureFilePath));
				return 1;
			}
			else if (fName.substr(fName.size() - 4).upper() == "FLAC")
			{
				lua_pushboolean(L, GetFlacArtwork(utf16FilePath, utf16PictureFilePath));
				return 1;
			}
			else if (fName.substr(fName.size() - 3).upper() == "M4V" || fName.substr(fName.size() - 3).upper() == "M4A")
			{
				lua_pushboolean(L, GetMP4Artwork(utf16FilePath, utf16PictureFilePath));
				return 1;
			}
		}

		lua_pushboolean(L, false);
		return 1;
	}

	int TagLibLibrary::set(lua_State* L)
	{
		const char* fileName;
		const char* filePath;

		if (lua_istable(L, 1))
		{
			lua_getfield(L, 1, "fileName");
			if (lua_isstring(L, -1))
			{
				fileName = lua_tostring(L, -1);
			}
			else
			{
				CoronaLuaError(L, "taglib.get() options.filename (string) expected, got: %s", lua_typename(L, -1));
				lua_pushnil(L);
			}
			lua_pop(L, 1);

			lua_getfield(L, 1, "filePath");
			if (lua_isstring(L, -1))
			{
				filePath = lua_tostring(L, -1);
			}
			else
			{
				CoronaLuaError(L, "taglib.get() options.filePath (string) expected, got: %s", lua_typename(L, -1));
				lua_pushnil(L);
			}
			lua_pop(L, 1);

			lua_getfield(L, 1, "tags");
			if (lua_istable(L, -1))
			{
				std::string fullPath = filePath;
				fullPath.append("\\");
				fullPath.append(fileName);
				std::wstring utf16Path = utf8_to_utf16(fullPath);
				TagLib::FileRef file(utf16Path.c_str());
				bool hasChangedTag = false;

				lua_getfield(L, -1, "title");
				if (lua_isstring(L, -1))
				{
					file.tag()->setTitle(lua_tostring(L, -1));
					hasChangedTag = true;
				}
				lua_pop(L, 1);

				lua_getfield(L, -1, "artist");
				if (lua_isstring(L, -1))
				{
					file.tag()->setArtist(lua_tostring(L, -1));
					hasChangedTag = true;
				}
				lua_pop(L, 1);

				lua_getfield(L, -1, "album");
				if (lua_isstring(L, -1))
				{
					file.tag()->setAlbum(lua_tostring(L, -1));
					hasChangedTag = true;
				}
				lua_pop(L, 1);

				lua_getfield(L, -1, "genre");
				if (lua_isstring(L, -1))
				{
					file.tag()->setGenre(lua_tostring(L, -1));
					hasChangedTag = true;
				}
				lua_pop(L, 1);

				lua_getfield(L, -1, "comment");
				if (lua_isstring(L, -1))
				{
					file.tag()->setComment(lua_tostring(L, -1));
					hasChangedTag = true;
				}
				lua_pop(L, 1);

				lua_getfield(L, -1, "year");
				if (lua_isnumber(L, -1))
				{
					file.tag()->setYear(lua_tonumber(L, -1));
					hasChangedTag = true;
				}
				lua_pop(L, 1);

				lua_getfield(L, -1, "trackNumber");
				if (lua_isnumber(L, -1))
				{
					file.tag()->setTrack(lua_tonumber(L, -1));
					hasChangedTag = true;
				}
				lua_pop(L, 2); // pop the table too

				if (hasChangedTag)
				{
					file.save();
				}
			}
			else
			{
				CoronaLuaError(L, "taglib.set() options (table) expected, got: %s", lua_typename(L, 3));
				lua_pushnil(L);
			}
		}
		else
		{
			CoronaLuaError(L, "taglib.get() options (table) expected, got: %s", lua_typename(L, 1));
			lua_pushnil(L);
		}

		return 1;
	}

	int TagLibLibrary::setArtwork(lua_State* L)
	{
		const char* fileName;
		const char* filePath;
		const char* pictureFileName;
		const char* pictureFilePath;

		if (lua_istable(L, 1))
		{
			lua_getfield(L, 1, "fileName");
			if (lua_isstring(L, -1))
			{
				fileName = lua_tostring(L, -1);
			}
			else
			{
				CoronaLuaError(L, "taglib.setArtwork() options.filename (string) expected, got: %s", lua_typename(L, -1));
				lua_pushnil(L);
			}
			lua_pop(L, 1);

			lua_getfield(L, 1, "filePath");
			if (lua_isstring(L, -1))
			{
				filePath = lua_tostring(L, -1);
			}
			else
			{
				CoronaLuaError(L, "taglib.setArtwork() options.filePath (string) expected, got: %s", lua_typename(L, -1));
				lua_pushnil(L);
			}
			lua_pop(L, 1);

			lua_getfield(L, 1, "imageFileName");
			if (lua_isstring(L, -1))
			{
				pictureFileName = lua_tostring(L, -1);
			}
			else
			{
				CoronaLuaError(L, "taglib.setArtwork() options.imageFileName (string) expected, got: %s", lua_typename(L, -1));
				lua_pushnil(L);
			}
			lua_pop(L, 1);

			lua_getfield(L, 1, "imageFilePath");
			if (lua_isstring(L, -1))
			{
				pictureFilePath = lua_tostring(L, -1);
			}
			else
			{
				CoronaLuaError(L, "taglib.setArtwork() options.imageFilePath (string) expected, got: %s", lua_typename(L, -1));
				lua_pushnil(L);
			}
			lua_pop(L, 1);
		}
		else
		{
			CoronaLuaError(L, "taglib.setArtwork() options (table) expected, got: %s", lua_typename(L, 1));
			lua_pushnil(L);
			return 1;
		}

		if (fileName != NULL && filePath != NULL && pictureFileName != NULL && pictureFilePath != NULL)
		{
			std::string fullPath = filePath;
			fullPath.append("\\");
			fullPath.append(fileName);

			std::string pictureFullPath = pictureFilePath;
			pictureFullPath.append("\\");
			pictureFullPath.append(pictureFileName);
			std::wstring utf16FilePath = utf8_to_utf16(fullPath);
			std::wstring utf16PictureFilePath = utf8_to_utf16(pictureFullPath);
			TagLib::String fName = fileName;

			if (fName.substr(fName.size() - 3).upper() == "MP3")
			{
				lua_pushboolean(L, SetMp3Artwork(utf16FilePath, utf16PictureFilePath));
				return 1;
			}
			else if (fName.substr(fName.size() - 4).upper() == "FLAC")
			{
				lua_pushboolean(L, SetFlacArtwork(utf16FilePath, utf16PictureFilePath));
				return 1;
			}
			else if (fName.substr(fName.size() - 3).upper() == "M4V" || fName.substr(fName.size() - 3).upper() == "M4A")
			{
				lua_pushboolean(L, SetMP4Artwork(utf16FilePath, utf16PictureFilePath));
				return 1;
			}
		}

		lua_pushboolean(L, false);
		return 1;
	}

	// ----------------------------------------------------------------------------

} // namespace Corona

// ----------------------------------------------------------------------------

CORONA_EXPORT
int luaopen_plugin_taglib(lua_State* L)
{
	return Corona::TagLibLibrary::Open(L);
}


