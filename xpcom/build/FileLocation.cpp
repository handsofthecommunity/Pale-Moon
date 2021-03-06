/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileLocation.h"
#include "nsZipArchive.h"
#include "nsURLHelper.h"

namespace mozilla {

FileLocation::FileLocation(const FileLocation &file, const char *path)
{
  if (file.IsZip()) {
    if (file.mBaseFile) {
      Init(file.mBaseFile, file.mPath.get());
    } else {
      Init(file.mBaseZip, file.mPath.get());
    }
    if (path) {
      int32_t i = mPath.RFindChar('/');
      if (kNotFound == i) {
        mPath.Truncate(0);
      } else {
        mPath.Truncate(i + 1);
      }
      mPath += path;
    }
  } else {
    if (path) {
      nsCOMPtr<nsIFile> cfile;
      file.mBaseFile->GetParent(getter_AddRefs(cfile));

#if defined(XP_WIN)
      nsAutoCString pathStr(path);
      char *p;
      uint32_t len = pathStr.GetMutableData(&p);
      for (; len; ++p, --len) {
        if ('/' == *p) {
            *p = '\\';
        }
      }
      cfile->AppendRelativeNativePath(pathStr);
#else
      cfile->AppendRelativeNativePath(nsDependentCString(path));
#endif
      Init(cfile);
    } else {
      Init(file.mBaseFile);
    }
  }
}

void
FileLocation::GetURIString(nsACString &result) const
{
  if (mBaseFile) {
    net_GetURLSpecFromActualFile(mBaseFile, result);
  } else if (mBaseZip) {
    nsRefPtr<nsZipHandle> handler = mBaseZip->GetFD();
    handler->mFile.GetURIString(result);
  }
  if (IsZip()) {
    result.Insert("jar:", 0);
    result += "!/";
    result += mPath;
  }
}

already_AddRefed<nsIFile>
FileLocation::GetBaseFile()
{
  if (IsZip() && mBaseZip) {
    nsRefPtr<nsZipHandle> handler = mBaseZip->GetFD();
    if (handler)
      return handler->mFile.GetBaseFile();
    return nullptr;
  }

  nsCOMPtr<nsIFile> file = mBaseFile;
  return file.forget();
}

bool
FileLocation::Equals(const FileLocation &file) const
{
  if (mPath != file.mPath)
    return false;

  if (mBaseFile && file.mBaseFile) {
    bool eq;
    return NS_SUCCEEDED(mBaseFile->Equals(file.mBaseFile, &eq)) && eq;
  }

  const FileLocation *a = this, *b = &file;
  if (a->mBaseZip) {
    nsRefPtr<nsZipHandle> handler = a->mBaseZip->GetFD();
    a = &handler->mFile;
  }
  if (b->mBaseZip) {
    nsRefPtr<nsZipHandle> handler = b->mBaseZip->GetFD();
    b = &handler->mFile;
  }
  return a->Equals(*b);
}

nsresult
FileLocation::GetData(Data &data)
{
  if (!IsZip()) {
    return mBaseFile->OpenNSPRFileDesc(PR_RDONLY, 0444, &data.mFd.rwget());
  }
  data.mZip = mBaseZip;
  if (!data.mZip) {
    data.mZip = new nsZipArchive();
    data.mZip->OpenArchive(mBaseFile);
  }
  data.mItem = data.mZip->GetItem(mPath.get());
  if (data.mItem)
    return NS_OK;
  return NS_ERROR_FILE_UNRECOGNIZED_PATH;
}

nsresult
FileLocation::Data::GetSize(uint32_t *result)
{
  if (mFd) {
    PRFileInfo64 fileInfo;
    if (PR_SUCCESS != PR_GetOpenFileInfo64(mFd, &fileInfo))
      return NS_ErrorAccordingToNSPR();

    if (fileInfo.size > int64_t(UINT32_MAX))
      return NS_ERROR_FILE_TOO_BIG;

    *result = fileInfo.size;
    return NS_OK;
  } else if (mItem) {
    *result = mItem->RealSize();
    return NS_OK;
  }
  return NS_ERROR_NOT_INITIALIZED;
}

nsresult
FileLocation::Data::Copy(char *buf, uint32_t len)
{
  if (mFd) {
    for (uint32_t totalRead = 0; totalRead < len; ) {
      int32_t read = PR_Read(mFd, buf + totalRead, XPCOM_MIN(len - totalRead, uint32_t(INT32_MAX)));
      if (read < 0)
        return NS_ErrorAccordingToNSPR();
      totalRead += read;
    }
    return NS_OK;
  } else if (mItem) {
    nsZipCursor cursor(mItem, mZip, reinterpret_cast<uint8_t *>(buf), len, true);
    uint32_t readLen;
    cursor.Copy(&readLen);
    return (readLen == len) ? NS_OK : NS_ERROR_FILE_CORRUPTED;
  }
  return NS_ERROR_NOT_INITIALIZED;
}

} /* namespace mozilla */
