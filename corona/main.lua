-----------------------------------------------------------------------------------------
--
-- main.lua
--
-----------------------------------------------------------------------------------------
local tag = require("plugin.taglib")
local docsPath = system.pathForFile(nil, system.DocumentsDirectory)
local resourcePath = system.pathForFile(nil, system.ResourceDirectory)

print("resource:", resourcePath)
print("docs:", docsPath)

-- MP3 TESTS
local testMp3MetadataGet = true
local testMp3MetadataSet = false
local testMp3MetadataSetRating = false
local testMP3ArtworkGet = false
local testMP3ArtworkSet = false
-- FLAC TESTS
local testFlacArtworkGet = false
local testFlacArtworkSet = false
-- MP4 TESTS
local testMP4ArtworkGet = false
local testMP4ArtworkSet = false

if (testMp3MetadataGet) then
	local testTag = tag.get({fileName = "Above & Beyond - Can't Sleep.mp3", filePath = resourcePath})

	for k, v in pairs(testTag) do
		print(k .. " : " .. v)
	end
end

if (testMp3MetadataSet) then
	print("")
	print("changing track title")
	print("")

	tag.set(
		{
			fileName = "Above & Beyond - Can't Sleep.mp3",
			filePath = resourcePath,
			tags = {title = "Can't Sleep", trackNumber = 16}
		}
	)

	local newTag = tag.get({fileName = "Above & Beyond - Can't Sleep.mp3", filePath = resourcePath})

	for k, v in pairs(newTag) do
		print(k .. " : " .. v)
	end
end

if (testMp3MetadataSetRating) then
	print("testing adding a rating to MP3 file")

	tag.set(
		{
			fileName = "Above & Beyond - Can't Sleep.mp3",
			filePath = resourcePath,
			tags = {rating = 242}
		}
	)

	local newTag = tag.get({fileName = "Above & Beyond - Can't Sleep.mp3", filePath = resourcePath})

	for k, v in pairs(newTag) do
		print(k .. " : " .. v)
	end
end

--- ### MP3 ### ---
if (testMP3ArtworkGet) then
	print("testing getting mp3 artwork")

	tag.getArtwork(
		{
			fileName = "Above & Beyond - Can't Sleep.mp3",
			filePath = resourcePath,
			imageFileName = "testMp3",
			imageFilePath = docsPath
		}
	)
end

if (testMP3ArtworkSet) then
	print("testing setting mp3 artwork")

	tag.setArtwork(
		{
			fileName = "Above & Beyond - Can't Sleep.mp3",
			filePath = resourcePath,
			--imageFileName = "gears.jpg",
			imageFileName = "origMP3.jpg",
			imageFilePath = resourcePath
		}
	)

	tag.getArtwork(
		{
			fileName = "Above & Beyond - Can't Sleep.mp3",
			filePath = resourcePath,
			imageFileName = "testMp3",
			imageFilePath = docsPath
		}
	)
end

--- ### FLAC ### ---
if (testFlacArtworkGet) then
	print("testing getting flac artwork")

	tag.getArtwork(
		{
			fileName = "Various - My Friend Pedro OST - 17 - Navie D - Stop and Watch.flac",
			filePath = resourcePath,
			imageFileName = "testFlac",
			imageFilePath = docsPath
		}
	)
end

if (testFlacArtworkSet) then
	print("testing setting flac artwork")

	tag.setArtwork(
		{
			fileName = "Various - My Friend Pedro OST - 17 - Navie D - Stop and Watch.flac",
			filePath = resourcePath,
			--imageFileName = "gears.jpg",
			imageFileName = "origFLAC.jpg",
			imageFilePath = resourcePath
		}
	)

	tag.getArtwork(
		{
			fileName = "Various - My Friend Pedro OST - 17 - Navie D - Stop and Watch.flac",
			filePath = resourcePath,
			imageFileName = "testFlac",
			imageFilePath = docsPath
		}
	)
end

--- ### MP4 ### ---
if (testMP4ArtworkGet) then
	print("testing getting mp4 artwork")

	tag.getArtwork(
		{
			fileName = "3 - Britney Spears.m4a",
			filePath = resourcePath,
			imageFileName = "testMp4",
			imageFilePath = docsPath
		}
	)
end

if (testMP4ArtworkSet) then
	print("testing setting mp4 artwork")

	tag.setArtwork(
		{
			fileName = "3 - Britney Spears.m4a",
			filePath = resourcePath,
			--imageFileName = "gears.jpg",
			imageFileName = "origMP4.jpg",
			imageFilePath = resourcePath
		}
	)

	tag.getArtwork(
		{
			fileName = "3 - Britney Spears.m4a",
			filePath = resourcePath,
			imageFileName = "testMp4",
			imageFilePath = docsPath
		}
	)
end
