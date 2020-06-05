//
//  Artwork.cpp
//  plugin_taglib
//
//  Created by Danny on 04/06/2020.
//

#include "Artwork.hpp"

////////////// ### WIN 32 ### //////////////

#ifdef _WIN32
bool Artwork::WriteCoverToFile(const TagLib::ByteVector& data, const wstring& target)
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

bool Artwork::GetMp3Artwork(wstring filePath, TagLib::String pictureFilePath)
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

				created = WriteCoverToFile(pictureFrame->picture(), UTFString::Convert(fullPath.toCString()));
			}
		}
	}

	return created;
}

bool Artwork::SetMp3Artwork(wstring filePath, wstring pictureFilePath)
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
				string framestr{ frameID.data(), frameID.size() };

				if (framestr.compare("APIC") == 0)
				{
					mp3Tag->removeFrame((*it));
					it = mp3Tag->frameList().begin();
				}
			}
		}

		ImageFile img(pictureFilePath);
		auto picframe = new TagLib::ID3v2::AttachedPictureFrame;
		picframe->setPicture(img.data());
		picframe->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
		mp3Tag->addFrame(picframe);
		mp3File.save();
		saved = true;
	}

	return saved;
}

bool Artwork::GetFlacArtwork(wstring filePath, TagLib::String pictureFilePath)
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

				created = WriteCoverToFile(picture->data(), UTFString::Convert(fullPath.toCString()));
			}
		}
	}

	return created;
}

bool Artwork::SetFlacArtwork(wstring filePath, wstring pictureFilePath)
{
	bool saved = false;
	TagLib::FLAC::File flacFile(filePath.c_str());

	if (flacFile.isValid())
	{
		TagLib::FLAC::Picture* picture = new TagLib::FLAC::Picture();
		ImageFile img(pictureFilePath);
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

bool Artwork::GetMP4Artwork(wstring filePath, TagLib::String pictureFilePath)
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

				created = WriteCoverToFile(picture->data(), UTFString::Convert(fullPath.toCString()));
			}
		}
	}

	return created;
}

bool Artwork::SetMP4Artwork(wstring filePath, wstring pictureFilePath)
{
	bool saved = false;

	TagLib::MP4::File mp4File(filePath.c_str());

	if (mp4File.isValid())
	{
		TagLib::MP4::CoverArtList coverArtList = mp4File.tag()->item("covr").toCoverArtList();
		ImageFile img(pictureFilePath);
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

////////////// ### MAC/LINUX ### //////////////

#else

bool Artwork::WriteCoverToFile(const TagLib::ByteVector& data, const string& target)
{
	FILE* f = fopen(target.c_str(), "wb");

	if (f)
	{
		const bool written = fwrite(data.data(), 1, data.size(), f) == data.size();
		fclose(f);
		return written;
	}

	return false;
}

bool Artwork::GetMp3Artwork(string filePath, TagLib::String pictureFilePath)
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

				created = WriteCoverToFile(pictureFrame->picture(), fullPath.toCString());
			}
		}
	}

	return created;
}

bool Artwork::SetMp3Artwork(string filePath, string pictureFilePath)
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
				string framestr{ frameID.data(), frameID.size() };

				if (framestr.compare("APIC") == 0)
				{
					mp3Tag->removeFrame((*it));
					it = mp3Tag->frameList().begin();
				}
			}
		}

		ImageFile img(pictureFilePath);
		auto picframe = new TagLib::ID3v2::AttachedPictureFrame;
		picframe->setPicture(img.data());
		picframe->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
		mp3Tag->addFrame(picframe);
		mp3File.save();
		saved = true;
	}

	return saved;
}

bool Artwork::GetFlacArtwork(string filePath, TagLib::String pictureFilePath)
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

				created = WriteCoverToFile(picture->data(), fullPath.toCString());
			}
		}
	}

	return created;
}

bool Artwork::SetFlacArtwork(string filePath, string pictureFilePath)
{
	bool saved = false;
	TagLib::FLAC::File flacFile(filePath.c_str());

	if (flacFile.isValid())
	{
		TagLib::FLAC::Picture* picture = new TagLib::FLAC::Picture();
		ImageFile img(pictureFilePath);
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

bool Artwork::GetMP4Artwork(string filePath, TagLib::String pictureFilePath)
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

				created = WriteCoverToFile(picture->data(), fullPath.toCString());
			}
		}
	}

	return created;
}

bool Artwork::SetMP4Artwork(string filePath, string pictureFilePath)
{
	bool saved = false;

	TagLib::MP4::File mp4File(filePath.c_str());

	if (mp4File.isValid())
	{
		TagLib::MP4::CoverArtList coverArtList = mp4File.tag()->item("covr").toCoverArtList();
		ImageFile img(pictureFilePath);
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
#endif
