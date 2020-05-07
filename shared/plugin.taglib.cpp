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
#include "taglib/mp4tag.h"
#include "taglib/mp4coverart.h"

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
			ImageFile(const char* file) : TagLib::File(file)
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

	static bool GetMp3Artwork(const char* filePath, const char* pictureFilePath)
	{
		bool created = false;

		if (filePath != NULL && pictureFilePath != NULL)
		{
			TagLib::MPEG::File mp3File(filePath);
			TagLib::ID3v2::Tag* mp3Tag = mp3File.ID3v2Tag();

			if (mp3Tag)
			{
				TagLib::ID3v2::FrameList frameList = mp3Tag->frameListMap()["APIC"]; //look for picture frames only

				if (!frameList.isEmpty())
				{
					TagLib::ID3v2::AttachedPictureFrame* pictureFrame;
					TagLib::ID3v2::FrameList::ConstIterator it = frameList.begin();

					for (; it != frameList.end(); it++)
					{
						pictureFrame = static_cast<TagLib::ID3v2::AttachedPictureFrame*> (*it);//cast Frame * to AttachedPictureFrame*
					}

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

						FILE* fout;
						fopen_s(&fout, fullPath.toCString(), "wb");
						fwrite(pictureFrame->picture().data(), pictureFrame->picture().size(), 1, fout);
						fclose(fout);
						created = true;
					}
				}
			}
		}

		return created;
	}

	static bool SetMp3Artwork(const char* filePath, const char* pictureFilePath)
	{
		bool saved = false;

		if (filePath != NULL && pictureFilePath != NULL)
		{
			TagLib::MPEG::File mp3File(filePath);
			TagLib::ID3v2::Tag* mp3Tag = mp3File.ID3v2Tag();

			if (mp3Tag)
			{
				TagLib::ID3v2::FrameList frames = mp3Tag->frameList("APIC");
				TagLib::ID3v2::AttachedPictureFrame* frame = new TagLib::ID3v2::AttachedPictureFrame;
				TagLibLibrary::ImageFile img(pictureFilePath);

				mp3File.ID3v2Tag()->removeFrame(frames.front());
				frame->setMimeType("image/jpeg");
				frame->setDescription("Cover");
				frame->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
				frame->setPicture(img.data());
				mp3File.ID3v2Tag()->addFrame(frame);
				mp3File.save();
				saved = true;
			}
		}

		return saved;
	}


	// ----------------------------------------------------------------------------

	// 
	int TagLibLibrary::get(lua_State* L)
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

			TagLib::FileRef file(fullPath.c_str());

			if (!file.isNull() && file.tag())
			{
				TagLib::AudioProperties* properties = file.audioProperties();
				int seconds = properties->length() % 60;
				int minutes = (properties->length() - seconds) / 60;

				//TagLib::

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
			TagLib::String fName = fileName;
			TagLib::String fileType = fName.substr(fName.size() - 3).upper();

			if (fileType == "MP3")
			{
				lua_pushboolean(L, GetMp3Artwork(fullPath.c_str(), pictureFullPath.c_str()));
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
				TagLib::FileRef file(fullPath.c_str());
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
			//CoronaLuaWarning(L, "set() picture path: %s\n", pictureFullPath.c_str());
			TagLib::String fName = fileName;
			TagLib::String fileType = fName.substr(fName.size() - 3).upper();

			if (fileType == "MP3")
			{
				lua_pushboolean(L, SetMp3Artwork(fullPath.c_str(), pictureFullPath.c_str()));
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


