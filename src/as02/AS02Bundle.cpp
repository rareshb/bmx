/*
 * Copyright (C) 2010, British Broadcasting Corporation
 * All Rights Reserved.
 *
 * Author: Philip de Nier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the British Broadcasting Corporation nor the names
 *       of its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstring>
#include <cerrno>
#include <cstdio>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#if defined(_MSC_VER)
#include <direct.h> // mkdir
#endif

#include <im/as02/AS02Bundle.h>
#include <im/URI.h>
#include <im/Utils.h>
#include <im/Version.h>
#include <im/IMException.h>
#include <im/Logging.h>

using namespace std;
using namespace im;



static const char MANIFEST_NAME[]       = "manifest.xml";
static const char SHIM_NAME[]           = "shim.xml";

static const char MEDIA_SUBDIR_NAME[]   = "media";



static bool check_is_dir(string name)
{
    struct stat buf;
    if (stat(name.c_str(), &buf) != 0)
        return false;

#if defined(_MSC_VER)
    return ((buf.st_mode & _S_IFMT) == _S_IFDIR);
#else
    return S_ISDIR(buf.st_mode);
#endif
}



AS02Bundle* AS02Bundle::OpenNew(string root_directory, bool create_directory)
{
    string root_filepath;
    if (root_directory.empty() || root_directory[0] != '/') {
        root_filepath = get_abs_cwd();
        if (!root_directory.empty())
            root_filepath.append("/").append(root_directory);
    } else {
        root_filepath = root_directory;
    }

    if (root_filepath[root_filepath.size() - 1] != '/')
        root_filepath.append("/");

    if (create_directory) {
#if defined(_MSC_VER)
        if (mkdir(root_filepath.c_str()) != 0) {
#else
        if (mkdir(root_filepath.c_str(), 0777) != 0) {
#endif
            if (errno != EEXIST)
                throw IMException("Failed to create bundle directory '%s': %s", root_filepath.c_str(), strerror(errno));
            if (!check_is_dir(root_filepath))
                throw IMException("Bundle directory name '%s' clashes with non-directory file", root_filepath.c_str());
        }
    } else if (!check_is_dir(root_filepath)) {
        throw IMException("Bundle root directory '%s' does not exist", root_filepath.c_str());
    }

    string sub_dir;
    sub_dir.reserve(root_filepath.size() + sizeof(MEDIA_SUBDIR_NAME));
    sub_dir.append(root_filepath).append(MEDIA_SUBDIR_NAME);
#if defined(_MSC_VER)
    if (mkdir(sub_dir.c_str()) != 0) {
#else
    if (mkdir(sub_dir.c_str(), 0777) != 0) {
#endif
        if (errno != EEXIST)
            throw IMException("Failed to create bundle media sub-directory '%s': %s", sub_dir.c_str(), strerror(errno));
        if (!check_is_dir(sub_dir))
            throw IMException("Media sub-directory '%s' clashes with non-directory file", sub_dir.c_str());
    }

    return new AS02Bundle(root_filepath);
}


AS02Bundle::AS02Bundle(string root_filepath)
{
    mRootFilepath = root_filepath;

    IM_ASSERT(!root_filepath.empty() && root_filepath[root_filepath.size() - 1] == '/');
    mBundleName = strip_path(root_filepath.substr(0, root_filepath.size() - 1));
    IM_CHECK_M(!mBundleName.empty(), ("Empty bundle name"));

    mManifest.SetBundleName(mBundleName);
    mManifest.SetBundleId(generate_uuid());
    mManifest.SetCreator(get_im_library_name());
}

AS02Bundle::~AS02Bundle()
{
}

string AS02Bundle::CreatePrimaryVersionFilepath(string *rel_uri_out)
{
    IM_ASSERT(!mBundleName.empty());

    // 'primary' version has same name as bundle
    string result;
    result.reserve(mRootFilepath.size() + mBundleName.size() + 4);
    result.append(mRootFilepath).append(mBundleName).append(".mxf");
    if (rel_uri_out) {
        URI rel_uri;
        rel_uri.ParseFilename(mBundleName + ".mxf");
        *rel_uri_out = rel_uri.ToString();
    }

    return result;
}

string AS02Bundle::CreateVersionFilepath(string name, string *rel_uri_out)
{
    string result;
    result.reserve(mRootFilepath.size() + name.size() + 4);
    result.append(mRootFilepath).append(name).append(".mxf");
    if (rel_uri_out) {
        URI rel_uri;
        rel_uri.ParseFilename(name + ".mxf");
        *rel_uri_out = rel_uri.ToString();
    }

    return result;
}

string AS02Bundle::CreateEssenceComponentFilepath(string version_filename, bool is_video, uint32_t track_number,
                                                  string *rel_uri_out)
{
    IM_CHECK(track_number > 0);

    char suffix[32];
    sprintf(suffix, "%s%u.mxf", (is_video ? "_v" : "_a"), track_number - 1);

    string version_name = strip_suffix(version_filename);

    string result;
    result.reserve(mRootFilepath.size() + sizeof(MEDIA_SUBDIR_NAME) + 1 + version_name.size() + sizeof(suffix));
    result.append(mRootFilepath).append(MEDIA_SUBDIR_NAME).append("/").append(version_name).append(suffix);
    if (rel_uri_out) {
        URI rel_uri;
        rel_uri.ParseFilename(string(MEDIA_SUBDIR_NAME).append("/").append(version_name).append(suffix));
        *rel_uri_out = rel_uri.ToString();
    }

    return result;
}

string AS02Bundle::CompleteFilepath(string rel_uri_in)
{
    URI rel_uri(rel_uri_in);
    IM_CHECK(rel_uri.IsRelative());

    string filename = rel_uri.ToFilename();

    string comp_filepath;
    comp_filepath.reserve(mRootFilepath.size() + filename.size());
    comp_filepath.append(mRootFilepath).append(filename);

    return comp_filepath;
}

void AS02Bundle::FinalizeBundle()
{
    mShim.Write(mRootFilepath + SHIM_NAME);
    mManifest.RegisterFile(SHIM_NAME, SHIM_FILE_ROLE);

    mManifest.Write(this, mRootFilepath + MANIFEST_NAME);
}
