#include "CoronaAssert.h"
#include "CoronaEvent.h"
#include "CoronaLua.h"
#include "CoronaLibrary.h"
#include <string.h>
#include <string>
#include <stdio.h>
#include <fstream>
#include "utfString.hpp"
#include "Artwork.hpp"
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
using namespace std;

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

	// 
	int TagLibLibrary::get(lua_State* L)
	{
		const char* fileName = NULL;
		const char* filePath = NULL;
		int rating = 0;

		if (lua_istable(L, 1))
		{
			lua_getfield(L, 1, "fileName");
			if (lua_isstring(L, -1))
			{
				fileName = lua_tostring(L, -1);
				CoronaLog("Filename is: %s", fileName);
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
				CoronaLog("FilePath is: %s", filePath);
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
			string fullPath(filePath);

			#ifdef _WIN32
			fullPath.append("\\");
			#else
			fullPath.append("/");
			#endif

			fullPath.append(fileName);

			#ifdef _WIN32
			wstring utf16Path = UTFString::Convert(fullPath);
			#else
			string utf16Path(fullPath);
			#endif

			TagLib::String fName(fileName);

			if (fName.substr(fName.size() - 3).upper() == "MP3")
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
		const char* fileName = NULL;
		const char* filePath = NULL;
		const char* pictureFileName = NULL;
		const char* pictureFilePath = NULL;

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
			string fullPath(filePath);

			#ifdef _WIN32
			fullPath.append("\\");
			#else
			fullPath.append("/");
			#endif

			fullPath.append(fileName);

			string pictureFullPath(pictureFilePath);

			#ifdef _WIN32
			pictureFullPath.append("\\");
			#else
			pictureFullPath.append("/");
			#endif

			pictureFullPath.append(pictureFileName);

			#ifdef _WIN32
			wstring utf16FilePath = UTFString::Convert(fullPath);
			wstring utf16PictureFilePath = UTFString::Convert(pictureFullPath);
			#else
			string utf16FilePath(fullPath);
			string utf16PictureFilePath(pictureFullPath);
			#endif

			TagLib::String fName(fileName);

			if (fName.substr(fName.size() - 3).upper() == "MP3")
			{
				lua_pushboolean(L, Artwork::GetMp3Artwork(utf16FilePath, utf16PictureFilePath));
				return 1;
			}
			else if (fName.substr(fName.size() - 4).upper() == "FLAC")
			{
				lua_pushboolean(L, Artwork::GetFlacArtwork(utf16FilePath, utf16PictureFilePath));
				return 1;
			}
			else if (fName.substr(fName.size() - 3).upper() == "M4V" || fName.substr(fName.size() - 3).upper() == "M4A")
			{
				lua_pushboolean(L, Artwork::GetMP4Artwork(utf16FilePath, utf16PictureFilePath));
				return 1;
			}
		}

		lua_pushboolean(L, false);
		return 1;
	}

	int TagLibLibrary::set(lua_State* L)
	{
		const char* fileName = NULL;
		const char* filePath = NULL;

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
				string fullPath(filePath);

				#ifdef _WIN32
				fullPath.append("\\");
				#else
				fullPath.append("/");
				#endif

				fullPath.append(fileName);

				#ifdef _WIN32
				wstring utf16Path = UTFString::Convert(fullPath);
				#else
				string utf16Path(fullPath);
				#endif

				TagLib::String fName(fileName);

				if (fName.substr(fName.size() - 3).upper() == "MP3")
				{
					TagLib::MPEG::File mp3File(utf16Path.c_str());

					if (mp3File.isValid() && mp3File.hasID3v2Tag())
					{
						int rating = 0;

						lua_getfield(L, -1, "rating");
						if (lua_isnumber(L, -1))
						{
							rating = lua_tonumber(L, -1);
						}
						lua_pop(L, 1);

						TagLib::ID3v2::PopularimeterFrame* popularimeterFrame = new TagLib::ID3v2::PopularimeterFrame();

						if (!mp3File.ID3v2Tag()->frameList("POPM").isEmpty() && mp3File.ID3v2Tag()->frameList("POPM").front())
						{
							popularimeterFrame->setEmail(dynamic_cast<TagLib::ID3v2::PopularimeterFrame*>(mp3File.ID3v2Tag()->frameList("POPM").front())->email());
							popularimeterFrame->setCounter(dynamic_cast<TagLib::ID3v2::PopularimeterFrame*>(mp3File.ID3v2Tag()->frameList("POPM").front())->counter());
							mp3File.ID3v2Tag()->removeFrame(mp3File.ID3v2Tag()->frameList("POPM").front());
							mp3File.save();
						}

						popularimeterFrame->setRating(rating);
						mp3File.ID3v2Tag()->addFrame(popularimeterFrame);
						mp3File.save();
					}
				}

				TagLib::FileRef file(utf16Path.c_str());

				if (!file.isNull())
				{
					bool hasChangedTag = false;

					lua_getfield(L, -1, "title");
					if (lua_isstring(L, -1))
					{
						file.tag()->setTitle(UTFString::Convert(lua_tostring(L, -1)));
						hasChangedTag = true;
					}
					lua_pop(L, 1);

					lua_getfield(L, -1, "artist");
					if (lua_isstring(L, -1))
					{
						file.tag()->setArtist(UTFString::Convert(lua_tostring(L, -1)));
						hasChangedTag = true;
					}
					lua_pop(L, 1);

					lua_getfield(L, -1, "album");
					if (lua_isstring(L, -1))
					{
						file.tag()->setAlbum(UTFString::Convert(lua_tostring(L, -1)));
						hasChangedTag = true;
					}
					lua_pop(L, 1);

					lua_getfield(L, -1, "genre");
					if (lua_isstring(L, -1))
					{
						file.tag()->setGenre(UTFString::Convert(lua_tostring(L, -1)));
						hasChangedTag = true;
					}
					lua_pop(L, 1);

					lua_getfield(L, -1, "comment");
					if (lua_isstring(L, -1))
					{
						file.tag()->setComment(UTFString::Convert(lua_tostring(L, -1)));
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
					lua_pop(L, 1);

					if (hasChangedTag)
					{
						file.save();
					}
				}
				else
				{
					#ifdef _WIN32
					CoronaLuaError(L, "taglib.set() couldn't find file at path: %ls", utf16Path);
					#elseif
					CoronaLuaError(L, "taglib.set() couldn't find file at path: %s", utf16Path);
					#endif
					lua_pushnil(L);
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
		const char* fileName = NULL;
		const char* filePath = NULL;
		const char* pictureFileName = NULL;
		const char* pictureFilePath = NULL;

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
			string fullPath(filePath);

			#ifdef _WIN32
			fullPath.append("\\");
			#else
			fullPath.append("/");
			#endif

			fullPath.append(fileName);

			string pictureFullPath(pictureFilePath);

			#ifdef _WIN32
			pictureFullPath.append("\\");
			#else
			pictureFullPath.append("/");
			#endif

			pictureFullPath.append(pictureFileName);

			#ifdef _WIN32
			wstring utf16FilePath = UTFString::Convert(fullPath);
			wstring utf16PictureFilePath = UTFString::Convert(pictureFullPath);
			#else
			string utf16FilePath(fullPath);
			string utf16PictureFilePath(pictureFullPath);
			#endif

			TagLib::String fName(fileName);

			if (fName.substr(fName.size() - 3).upper() == "MP3")
			{
				lua_pushboolean(L, Artwork::SetMp3Artwork(utf16FilePath, utf16PictureFilePath));
				return 1;
			}
			else if (fName.substr(fName.size() - 4).upper() == "FLAC")
			{
				lua_pushboolean(L, Artwork::SetFlacArtwork(utf16FilePath, utf16PictureFilePath));
				return 1;
			}
			else if (fName.substr(fName.size() - 3).upper() == "M4V" || fName.substr(fName.size() - 3).upper() == "M4A")
			{
				lua_pushboolean(L, Artwork::SetMP4Artwork(utf16FilePath, utf16PictureFilePath));
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


