/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000-2002 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 */

#include "fileio.h"
#include "apr_file_io.h"
#include "apr_general.h"
#include "apr_strings.h"
#include "apr_errno.h"
#if APR_HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

static apr_filetype_e filetype_from_mode(mode_t mode)
{
    apr_filetype_e type = APR_NOFILE;

    if (S_ISREG(mode))
        type = APR_REG;
    if (S_ISDIR(mode))
        type = APR_DIR;
    if (S_ISCHR(mode))
        type = APR_CHR;
    if (S_ISBLK(mode))
        type = APR_BLK;
    if (S_ISFIFO(mode))
        type = APR_PIPE;
    if (S_ISLNK(mode))
        type = APR_LNK;
#ifndef BEOS
    if (S_ISSOCK(mode))
        type = APR_SOCK;
#endif
    return type;
}

static void fill_out_finfo(apr_finfo_t *finfo, struct stat *info,
                           apr_int32_t wanted)
{ 
    finfo->valid = APR_FINFO_MIN | APR_FINFO_IDENT | APR_FINFO_NLINK
                 | APR_FINFO_OWNER | APR_FINFO_PROT;
    finfo->protection = apr_unix_mode2perms(info->st_mode);
    finfo->filetype = filetype_from_mode(info->st_mode);
    finfo->user = info->st_uid;
    finfo->group = info->st_gid;
    finfo->size = info->st_size;
    finfo->inode = info->st_ino;
    finfo->device = info->st_dev;
    finfo->nlink = info->st_nlink;
    apr_time_ansi_put(&finfo->atime, info->st_atime);
    apr_time_ansi_put(&finfo->mtime, info->st_mtime);
    apr_time_ansi_put(&finfo->ctime, info->st_ctime);
    /* ### needs to be revisited  
     * if (wanted & APR_FINFO_CSIZE) {
     *   finfo->csize = info->st_blocks * 512;
     *   finfo->valid |= APR_FINFO_CSIZE;
     * }
     */
}

APR_DECLARE(apr_status_t) apr_file_info_get(apr_finfo_t *finfo, 
                                            apr_int32_t wanted,
                                            apr_file_t *thefile)
{
    struct stat info;

    if (fstat(thefile->filedes, &info) == 0) {
        finfo->pool = thefile->pool;
        finfo->fname = thefile->fname;
        fill_out_finfo(finfo, &info, wanted);
        return (wanted & ~finfo->valid) ? APR_INCOMPLETE : APR_SUCCESS;
    }
    else {
        return errno;
    }
}

APR_DECLARE(apr_status_t) apr_file_perms_set(const char *fname, 
                                             apr_fileperms_t perms)
{
    mode_t mode = apr_unix_perms2mode(perms);

    if (chmod(fname, mode) == -1)
        return errno;
    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_file_attrs_set(const char *fname,
                                             apr_fileattrs_t attributes,
                                             apr_fileattrs_t attr_mask,
                                             apr_pool_t *pool)
{
    apr_status_t status;
    apr_finfo_t finfo;

    status = apr_stat(&finfo, fname, APR_FINFO_PROT, pool);
    if (!APR_STATUS_IS_SUCCESS(status))
        return status;

    /* ### TODO: should added bits be umask'd? */
    if (attr_mask & APR_FILE_ATTR_READONLY)
    {
        if (attributes & APR_FILE_ATTR_READONLY)
        {
            finfo.protection &= ~APR_UWRITE;
            finfo.protection &= ~APR_GWRITE;
            finfo.protection &= ~APR_WWRITE;
        }
        else
        {
            /* ### umask this! */
            finfo.protection |= APR_UWRITE;
            finfo.protection |= APR_GWRITE;
            finfo.protection |= APR_WWRITE;
        }
    }

    if (attr_mask & APR_FILE_ATTR_EXECUTABLE)
    {
        if (attributes & APR_FILE_ATTR_EXECUTABLE)
        {
            /* ### umask this! */
            finfo.protection |= APR_UEXECUTE;
            finfo.protection |= APR_GEXECUTE;
            finfo.protection |= APR_WEXECUTE;
        }
        else
        {
            finfo.protection &= ~APR_UEXECUTE;
            finfo.protection &= ~APR_GEXECUTE;
            finfo.protection &= ~APR_WEXECUTE;
        }
    }

    return apr_file_perms_set(fname, finfo.protection);
}

APR_DECLARE(apr_status_t) apr_stat(apr_finfo_t *finfo, 
                                   const char *fname, 
                                   apr_int32_t wanted, apr_pool_t *pool)
{
    struct stat info;
    int srv;

    if (wanted & APR_FINFO_LINK)
        srv = lstat(fname, &info);
    else
        srv = stat(fname, &info);

    if (srv == 0) {
        finfo->pool = pool;
        finfo->fname = fname;
        fill_out_finfo(finfo, &info, wanted);
        if (wanted & APR_FINFO_LINK)
            wanted &= ~APR_FINFO_LINK;
        return (wanted & ~finfo->valid) ? APR_INCOMPLETE : APR_SUCCESS;
    }
    else {
#if !defined(ENOENT) || !defined(ENOTDIR)
#error ENOENT || ENOTDIR not defined; please see the
#error comments at this line in the source for a workaround.
        /*
         * If ENOENT || ENOTDIR is not defined in one of the your OS's
         * include files, APR cannot report a good reason why the stat()
         * of the file failed; there are cases where it can fail even though
         * the file exists.  This opens holes in Apache, for example, because
         * it becomes possible for someone to get a directory listing of a 
         * directory even though there is an index (eg. index.html) file in 
         * it.  If you do not have a problem with this, delete the above 
         * #error lines and start the compile again.  If you need to do this,
         * please submit a bug report to http://www.apache.org/bug_report.html
         * letting us know that you needed to do this.  Please be sure to 
         * include the operating system you are using.
         */
        /* WARNING: All errors will be handled as not found
         */
#if !defined(ENOENT) 
        return APR_ENOENT;
#else
        /* WARNING: All errors but not found will be handled as not directory
         */
        if (errno != ENOENT)
            return APR_ENOENT;
        else
            return errno;
#endif
#else /* All was defined well, report the usual: */
        return errno;
#endif
    }
}

/* Perhaps this becomes nothing but a macro?
 */
APR_DECLARE(apr_status_t) apr_lstat(apr_finfo_t *finfo, const char *fname,
                      apr_int32_t wanted, apr_pool_t *pool)
{
    return apr_stat(finfo, fname, wanted | APR_FINFO_LINK, pool);
}

