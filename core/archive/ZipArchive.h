/*
    Created on: Nov 23, 2018

	Copyright 2018 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "archive.h"
#include <zip.h>

class ZipArchive : public Archive
{
public:
	~ZipArchive() override;

	ArchiveFile* OpenFile(const char* name) override;
	ArchiveFile* OpenFileByCrc(u32 crc) override;

	bool Open(const void *data, size_t size);
	ArchiveFile *OpenFirstFile();

protected:
	bool Open(FILE *file) override;

private:
	zip_t *zip = nullptr;
};

class ZipArchiveFile : public ArchiveFile
{
public:
	ZipArchiveFile(zip_file_t *zip_file, size_t length)
		: zip_file(zip_file), _length(length) {}
	~ZipArchiveFile() override {
		zip_fclose(zip_file);
	}
	u32 Read(void* buffer, u32 length) override;
	size_t length() override {
		return _length;
	}

private:
	zip_file_t *zip_file;
	size_t _length;
};
