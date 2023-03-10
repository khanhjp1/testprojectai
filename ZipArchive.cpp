#include "ZipArchive.h"
#include "stdio.h"
#include "sys/stat.h"

#ifdef MINIZIP_FROM_SYSTEM
#include <minizip/unzip.h>
#else // from our embedded sources
#include "unzip.h"
#endif

NS_CC_EXT_BEGIN

#define BUFFER_SIZE    20480
#define MAX_FILENAME   512

ZipArchive::ZipArchive()
{
	/*m_zipName = "";
	m_isStart = false;
	m_curProgress = 0;*/
	_fileUtils = FileUtils::getInstance();
}
ZipArchive::~ZipArchive()
{
}


void ZipArchive::startDecompression(const std::string &zip)
{
	if (m_isStart)
		return;

	m_isStart = true;
	m_zipName = zip;
	std::thread t(&ZipArchive::decompress, this);
	t.detach();
}

bool ZipArchive::decompress()
{
	// Find root path for zip file
	string zip = m_zipName.c_str();
	size_t pos = zip.find_last_of("/\\");
	if (pos == std::string::npos)
	{
		log("AssetsManagerEx : no root path specified for zip file %s\n", zip.c_str());
		return false;
	}
	const std::string rootPath = FileUtils::getInstance()->getWritablePath();// zip.substr(0, pos + 1);

	string zipPath = FileUtils::getInstance()->fullPathForFilename(zip);
	if (zipPath == "")
	{
		m_curProgress = 100;
		return false;
	}

	// Open the zip file
	ssize_t size = 0;
	unsigned char *zipFileData = FileUtils::getInstance()->getFileData(zipPath, "rb", &size);
	unzFile zipfile = unzOpenBuffer(zipFileData,size);
	if (!zipfile)
	{
		log("AssetsManagerEx : can not open downloaded zip file %s\n", zipPath.c_str());
		return false;
	}

	// Get info about the zip file
	unz_global_info global_info;
	if (unzGetGlobalInfo(zipfile, &global_info) != UNZ_OK)
	{
		log("AssetsManagerEx : can not read file global info of %s\n", zipPath.c_str());
		unzClose(zipfile);
		return false;
	}

	// Buffer to hold data read from the zip file
	char readBuffer[BUFFER_SIZE] = "";
	// Loop to extract all files.
	uLong i;
	for (i = 0; i < global_info.number_entry; ++i)
	{
		// Get info about current file.
		unz_file_info fileInfo;
		char fileName[MAX_FILENAME];
		if (unzGetCurrentFileInfo(zipfile,
			&fileInfo,
			fileName,
			MAX_FILENAME,
			NULL,
			0,
			NULL,
			0) != UNZ_OK)
		{
			log("AssetsManagerEx : can not read compressed file info\n");
			unzClose(zipfile);
			return false;
		}
		const std::string fullPath = rootPath + fileName;

		// Check if this entry is a directory or a file.
		const size_t filenameLength = strlen(fileName);
		if (fileName[filenameLength - 1] == '/')
		{
			//There are not directory entry in some case.
			//So we need to create directory when decompressing file entry
			if (!_fileUtils->createDirectory(basename(fullPath)))
			{
				// Failed to create directory
				log("AssetsManagerEx : can not create directory %s\n", fullPath.c_str());
				unzClose(zipfile);
				return false;
			}
		}
		else
		{
			// Create all directories in advance to avoid issue
			std::string dir = basename(fullPath);
			if (!_fileUtils->isDirectoryExist(dir)) {
				if (!_fileUtils->createDirectory(dir)) {
					// Failed to create directory
					log("AssetsManagerEx : can not create directory %s\n", fullPath.c_str());
					unzClose(zipfile);
					return false;
				}
			}
			// Entry is a file, so extract it.
			// Open current file.
			if (unzOpenCurrentFile(zipfile) != UNZ_OK)
			{
				log("AssetsManagerEx : can not extract file %s\n", fileName);
				unzClose(zipfile);
				return false;
			}

			// Create a file to store current file.
			FILE *out = fopen(FileUtils::getInstance()->getSuitableFOpen(fullPath).c_str(), "wb");
			if (!out)
			{
				log("AssetsManagerEx : can not create decompress destination file %s (errno: %d)\n", fullPath.c_str(), errno);
				unzCloseCurrentFile(zipfile);
				unzClose(zipfile);
				return false;
			}

			// Write current file content to destinate file.
			int error = UNZ_OK;
			do
			{
				error = unzReadCurrentFile(zipfile, readBuffer, BUFFER_SIZE);
				if (error < 0)
				{
					log("AssetsManagerEx : can not read zip file %s, error code is %d\n", fileName, error);
					fclose(out);
					unzCloseCurrentFile(zipfile);
					unzClose(zipfile);
					return false;
				}

				if (error > 0)
				{
					fwrite(readBuffer, error, 1, out);
				}
			} while (error > 0);

			fclose(out);
		}

		unzCloseCurrentFile(zipfile);

		// Goto next entry listed in the zip file.
		if ((i + 1) < global_info.number_entry)
		{
			if (unzGoToNextFile(zipfile) != UNZ_OK)
			{
				log("AssetsManagerEx : can not read next file for decompressing\n");
				unzClose(zipfile);
				return false;
			}
		}

		m_mutex.lock();
		m_curProgress = ((float)(i + 1) / (float)global_info.number_entry) * 100.0f;
		m_mutex.unlock();
	}

	unzClose(zipfile);
	delete[] zipFileData;
	//_fileUtils->removeFile(zipPath);
	m_isStart = false;

	//const std::string rootPath = FileUtils::getInstance()->getWritablePath();// zip.substr(0, pos + 1);
	std::string filePath = rootPath + "zipOver";

	FILE* file = fopen(filePath.c_str(), "wb");

	if (file) 
		fclose(file);
	else
		log("save file error.");

	return true;
}


int ZipArchive::getCurProgress()
{
	//返回当前进度
	m_mutex.lock();
	int res = m_curProgress;
	m_mutex.unlock();
	return res;
}

std::string ZipArchive::basename(const std::string& path) const
{
	size_t found = path.find_last_of("/\\");

	if (std::string::npos != found)
	{
		return path.substr(0, found);
	}
	else
	{
		return path;
	}
}

NS_CC_EXT_END
