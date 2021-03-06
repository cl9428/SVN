/*
 * svn_wc.h :  public interface for the Subversion Working Copy Library
 *
 * ====================================================================
 * Copyright (c) 2000-2002 CollabNet.  All rights reserved.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution.  The terms
 * are also available at http://subversion.tigris.org/license-1.html.
 * If newer versions of this license are posted there, you may use a
 * newer version instead, at your option.
 *
 * This software consists of voluntary contributions made by many
 * individuals.  For exact contribution history, see the revision
 * history and logs, available at http://subversion.tigris.org/.
 * ====================================================================
 */

#ifndef SVN_WC_H
#define SVN_WC_H


/* ==================================================================== */

/* 
 * Requires:  
 *            A working copy
 * 
 * Provides: 
 *            - Ability to manipulate working copy's versioned data.
 *            - Ability to manipulate working copy's administrative files.
 *
 * Used By:   
 *            Clients.
 */

#include <apr.h>
#include <apr_pools.h>
#include <apr_tables.h>
#include <apr_hash.h>

#include "svn_types.h"
#include "svn_string.h"
#include "svn_delta.h"
#include "svn_error.h"
#include "svn_ra.h"    /* for svn_ra_reporter_t type */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*** Notification/callback handling. ***/

/* In many cases, the WC library will scan a working copy and making
   changes. The caller usually wants to know when each of these changes
   have been made, so that it can display some kind of notification to
   the user.

   These notifications have a standard callback function type, which
   takes the path of the file that was affected, and a caller-
   supplied baton.

   Note that the callback is a 'void' return -- this is a simple
   reporting mechanism, rather than an opportunity for the caller to
   alter the operation of the WC library.
*/
typedef enum svn_wc_notify_action_t
{
  svn_wc_notify_add = 0,
  svn_wc_notify_copy,
  svn_wc_notify_delete,
  svn_wc_notify_restore,
  svn_wc_notify_revert,
  svn_wc_notify_resolve,
  svn_wc_notify_update,
  svn_wc_notify_commit_modified,
  svn_wc_notify_commit_added,
  svn_wc_notify_commit_deleted,
  svn_wc_notify_commit_replaced,
  svn_wc_notify_commit_postfix_txdelta
} svn_wc_notify_action_t;

typedef void (*svn_wc_notify_func_t) (void *baton, 
                                      svn_wc_notify_action_t action,
                                      const char *path);



/*** Asking questions about a working copy. ***/

/* Set *IS_WC to true iff PATH is a valid working copy directory, else
   set it to false.  PATH must exist, either as a file or directory,
   else an error will be returned. */
svn_error_t *svn_wc_check_wc (const svn_stringbuf_t *path,
                              svn_boolean_t *is_wc,
                              apr_pool_t *pool);


/* Set *HAS_BINARY_PROP to TRUE iff PATH has been marked with a
   property indicating that it is non-text (i.e. binary.) */
svn_error_t *svn_wc_has_binary_prop (svn_boolean_t *has_binary_prop,
                                     const svn_stringbuf_t *path,
                                     apr_pool_t *pool);


/*** Detecting modification. ***/

/* Set *MODIFIED_P to non-zero if FILENAME's text is modified
   w.r.t. the base revision, else set *MODIFIED_P to zero.
   FILENAME is a path to the file, not just a basename. 

   If FILENAME does not exist, consider it unmodified.  If it exists
   but is not under revision control (not even scheduled for
   addition), return the error SVN_ERR_ENTRY_NOT_FOUND.
*/
svn_error_t *svn_wc_text_modified_p (svn_boolean_t *modified_p,
                                     svn_stringbuf_t *filename,
                                     apr_pool_t *pool);


/* Set *MODIFIED_P to non-zero if PATH's properties are modified
   w.r.t. the base revision, else set MODIFIED_P to zero. */
svn_error_t *svn_wc_props_modified_p (svn_boolean_t *modified_p,
                                      svn_stringbuf_t *path,
                                      apr_pool_t *pool);




/*** Administrative subdir. ***/

/* Ideally, this would be completely private to wc internals (in fact,
   it used to be that adm_files.c:adm_subdir() was the only function
   who knew the adm subdir's name).  However, import wants to protect
   against importing administrative subdirs, so now the name is a
   matter of public record. */

#define SVN_WC_ADM_DIR_NAME   ".svn"



/*** Entries and status. ***/

typedef enum svn_wc_schedule_t
{
  svn_wc_schedule_normal,       /* Nothing special here */
  svn_wc_schedule_add,          /* Slated for addition */
  svn_wc_schedule_delete,       /* Slated for deletion */
  svn_wc_schedule_replace       /* Slated for replacement (delete + add) */

} svn_wc_schedule_t;


/* A working copy entry -- that is, revision control information about
   one versioned entity. */
typedef struct svn_wc_entry_t
{
  /* General Attributes */
  svn_stringbuf_t *name;         /* entry's name */
  svn_revnum_t revision;         /* base revision */
  svn_stringbuf_t *url;          /* url in repository */
  svn_stringbuf_t *repos;        /* canonical repository url */
  svn_node_kind_t kind;          /* node kind (file, dir, ...) */

  /* State information */
  svn_wc_schedule_t schedule;    /* scheduling (add, delete, replace ...) */
  svn_boolean_t copied;          /* in a copied state */
  svn_stringbuf_t *copyfrom_url; /* copyfrom location */
  svn_revnum_t copyfrom_rev;     /* copyfrom revision */
  svn_stringbuf_t *conflict_old; /* old version of conflicted file */
  svn_stringbuf_t *conflict_new; /* new version of conflicted file */
  svn_stringbuf_t *conflict_wrk; /* wroking version of conflicted file */
  svn_stringbuf_t *prejfile;     /* property reject file */

  /* Timestamps (0 means no information available) */
  apr_time_t text_time;          /* last up-to-date time for text contents */
  apr_time_t prop_time;          /* last up-to-date time for properties */

  /* "Entry props" */
  svn_revnum_t cmt_rev;          /* last revision this was changed */
  apr_time_t cmt_date;           /* last date this was changed */
  svn_stringbuf_t *cmt_author;   /* last commit author of this item 
                                    ### should this be svn_string_t or
                                    ### perhaps just a const char * ? */
  
} svn_wc_entry_t;


/* How an entries file's owner dir is named in the entries file. */
#define SVN_WC_ENTRY_THIS_DIR  "svn:this_dir"


/* Set *ENTRY to an entry for PATH, allocated in POOL.
 * If PATH is not under revision control, set *ENTRY to NULL.  
 *
 * Note that it is possible for PATH to be absent from disk but still
 * under revision control; and conversely, it is possible for PATH to
 * be present, but not under revision control.
 */
svn_error_t *svn_wc_entry (svn_wc_entry_t **entry,
                           svn_stringbuf_t *path,
                           apr_pool_t *pool);


/* Parse the `entries' file for PATH and return a hash ENTRIES, whose
   keys are (const char *) entry names and values are (svn_wc_entry_t *). 
   
   Important note: only the entry structures representing files and
   SVN_WC_ENTRY_THIS_DIR contain complete information.  The entry
   structures representing subdirs have only the `kind' and `state'
   fields filled in.  If you want info on a subdir, you must use this
   routine to open its PATH and read the SVN_WC_ENTRY_THIS_DIR
   structure, or call svn_wc_get_entry on its PATH. */
svn_error_t *svn_wc_entries_read (apr_hash_t **entries,
                                  svn_stringbuf_t *path,
                                  apr_pool_t *pool);


/* Return a duplicate of ENTRY, allocated in POOL.  No part of the new
   entry will be shared with ENTRY. */
svn_wc_entry_t *svn_wc_entry_dup (svn_wc_entry_t *entry, apr_pool_t *pool);


/* Given a DIR_PATH under version control, decide if one of its
   entries (ENTRY) is in state of conflict; return the answers in
   TEXT_CONFLICTED_P and PROP_CONFLICTED_P.  

   (If the entry mentions that a .rej or .prej exist, but they are
   both removed, assume the conflict has been resolved by the user.)  */
svn_error_t *svn_wc_conflicted_p (svn_boolean_t *text_conflicted_p,
                                  svn_boolean_t *prop_conflicted_p,
                                  svn_stringbuf_t *dir_path,
                                  svn_wc_entry_t *entry,
                                  apr_pool_t *pool);

/* Set *URL and *REV to the ancestor url and revision for PATH,
   allocating in POOL. */
svn_error_t *svn_wc_get_ancestry (svn_stringbuf_t **url,
                                  svn_revnum_t *rev,
                                  svn_stringbuf_t *path,
                                  apr_pool_t *pool);


/*** Status. ***/

/* We have two functions for getting working copy status: one function
 * for getting the status of exactly one thing, and another for
 * getting the statuses of (potentially) multiple things.
 * 
 * The WebDAV concept of "depth" may be useful in understanding the
 * motivation behind this.  Suppose we're getting the status of
 * directory D.  The three depth levels would mean
 * 
 *    depth 0:         D itself (just the named directory)
 *    depth 1:         D and its immediate children (D + its entries)
 *    depth Infinity:  D and all its descendants (full recursion)
 * 
 * To offer all three levels, we could have one unified function,
 * taking a `depth' parameter.  Unfortunately, because this function
 * would have to handle multiple return values as well as the single
 * return value case, getting the status of just one entity would
 * become cumbersome: you'd have to roll through a hash to find one
 * lone status.
 * 
 * So we have svn_wc_status() for depth 0, and svn_wc_statuses() for
 * depths 1 and 2, since the latter two involve multiple return
 * values.
 *
 * NOTE:  Status structures returned by svn_wc_status() or found in
 * the hash created by svn_wc_statuses() may contain a NULL ->entry
 * field.  This indicates an item that is not versioned in the working
 * copy.
 */

enum svn_wc_status_kind
{
    svn_wc_status_none = 1,    /* does not exist */
    svn_wc_status_unversioned, /* is not a versioned thing in this wc */
    svn_wc_status_normal,      /* exists, but uninteresting. */
    svn_wc_status_added,       /* is scheduled for additon */
    svn_wc_status_absent,      /* under v.c., but is missing */
    svn_wc_status_deleted,     /* scheduled for deletion */
    svn_wc_status_replaced,    /* was deleted and then re-added */
    svn_wc_status_modified,    /* text or props have been modified */
    svn_wc_status_merged,      /* local mods received repos mods */
    svn_wc_status_conflicted   /* local mods received conflicting repos mods */
};

/* Structure for holding the "status" of a working copy item. 
   The item's entry data is in ENTRY, augmented and possibly shadowed
   by the other fields.  ENTRY is null if this item is not under
   version control. */
typedef struct svn_wc_status_t
{
  svn_wc_entry_t *entry;     /* Can be NULL if not under vc. */
  
  /* Mutually exclusive states. One of these will always be set for
     the "textual" component and one will be set for the "property"
     component.  */
  enum svn_wc_status_kind text_status;
  enum svn_wc_status_kind prop_status;

  /* Booleans: a directory can be 'locked' if a working copy update
     was interrupted, and a file or directory can be 'copied' if it's
     scheduled for addition-with-history (or part of a subtree that
     is scheduled as such.) */
  svn_boolean_t locked;
  svn_boolean_t copied;

  /* Fields that describe the status of the entry in the repository;
     in other words, these fields indicate whether text or props would
     be patched or deleted if we were to run 'svn up'. */
  enum svn_wc_status_kind repos_text_status;
  enum svn_wc_status_kind repos_prop_status;

} svn_wc_status_t;


/* Fill *STATUS for PATH, allocating in POOL, with the exception of
   the repos_rev field, which is normally filled in by the caller. 

   Here are some things to note about the returned structure.  A quick
   examination of the STATUS->text_status after a successful return of
   this function can reveal the following things:

      svn_wc_status_none : PATH is not versioned, and is either not
                           present on disk, or is ignored by the
                           svn:ignore property setting for PATH's
                           parent directory.

      svn_wc_status_absent : PATH is versioned, but is missing from
                             the working copy.

      svn_wc_status_unversioned : PATH is not versioned, but is
                                  present on disk and not being
                                  ignored (see above).  

   The other available results for the text_status field more
   straightforward in their meanings.  See the comments on the
   svn_wc_status_kind structure above for some hints.  */
svn_error_t *svn_wc_status (svn_wc_status_t **status, 
                            svn_stringbuf_t *path, 
                            apr_pool_t *pool);


/* Under PATH, fill STATUSHASH mapping paths to svn_wc_status_t
 * structures.  All fields in each struct will be filled in except for
 * repos_rev, which would presumably be filled in by the caller.
 *
 * PATH will usually be a directory, since for a regular file, you would
 * have used svn_wc_status().  However, it is no error if PATH is not
 * a directory; its status will simply be stored in STATUSHASH like
 * any other.
 *
 * If STRICT is non-zero, then if we encounter a path that is not in
 * the wc, we'll return an error. STRICT should be zero if we're
 * updating, as the update will catch any non wc path errors (and
 * properly deal with files that are in the repository but missing
 * from the wc for whatever reason).
 *
 * Assuming PATH is a directory, then:
 * 
 * If GET_ALL is zero, then only locally-modified entries will be
 * returned.  If non-zero, then all entries will be returned.
 *
 * If DESCEND is zero, statushash will contain paths for PATH and
 * its non-directory entries (subdirectories should be subjects of
 * separate status calls).  
 *
 * If DESCEND is non-zero, statushash will contain statuses for PATH
 * and everything below it, including subdirectories.  In other
 * words, a full recursion.  */
svn_error_t *svn_wc_statuses (apr_hash_t *statushash,
                              svn_stringbuf_t *path,
                              svn_boolean_t descend,
                              svn_boolean_t get_all,
                              svn_boolean_t strict,
                              apr_pool_t *pool);


/* Set  *EDITOR and *EDIT_BATON to an editor that tweaks or adds
   svn_wc_status_t structures to STATUSHASH to reflect repository
   modifications that would be received on update, and that sets
   *YOUNGEST to the youngest revision in the repository (the editor
   also sets the repos_rev field in each svn_wc_status_t structure
   to the same value).

   If DESCEND is zero, then only immediate children of PATH will be
   done.

   Allocate the editor itself in POOL, but the editor does temporary
   allocations in a subpool of POOL.  */
svn_error_t *svn_wc_get_status_editor (const svn_delta_editor_t **editor,
                                       void **edit_baton,
                                       svn_stringbuf_t *path,
                                       svn_boolean_t descend,
                                       apr_hash_t *statushash,
                                       svn_revnum_t *youngest,
                                       apr_pool_t *pool);



/* Where you see an argument like
 * 
 *   apr_array_header_t *paths
 *
 * it means an array of (svn_stringbuf_t *) types, each one of which is
 * a file or directory path.  This is so we can do atomic operations
 * on any random set of files and directories.
 */


/* Copy SRC to DST_BASENAME in DST_PARENT, and schedule DST_BASENAME
   for addition to the repository, remembering the copy history.

   SRC must be a file or directory under version control; DST_PARENT
   must be a directory under version control in the same working copy;
   DST_BASENAME will be the name of the copied item, and it must not
   exist already.

   For each file or directory copied, NOTIFY_FUNC will be called
   with its path and the NOTIFY_BATON. NOTIFY_FUNC may be NULL if
   you are not interested in this information.

   Important: this is a variant of svn_wc_add.  No changes will happen
   to the repository until a commit occurs.  This scheduling can be
   removed with svn_client_revert.  */
svn_error_t *svn_wc_copy (svn_stringbuf_t *src,
                          svn_stringbuf_t *dst_parent,
                          svn_stringbuf_t *dst_basename,
                          svn_wc_notify_func_t notify_func,
                          void *notify_baton,
                          apr_pool_t *pool);


/* Schedule PATH for deletion.  This does not actually delete PATH
   from disk nor from the repository.  It is deleted from the
   repository on commit.

   If PATH refers to a directory, then a recursive deletion will occur.

   For each path marked for deletion, NOTIFY_FUNC will be called with
   the NOTIFY_BATON and that path. The NOTIFY_FUNC callback may be
   NULL if notification is not needed.  */
svn_error_t *svn_wc_delete (svn_stringbuf_t *path,
                            svn_wc_notify_func_t notify_func,
                            void *notify_baton,
                            apr_pool_t *pool);


/* Put PATH under version control by adding an entry in its parent,
   and, if PATH is a directory, adding an administrative area.  The
   new entry and anything under it is scheduled for addition to the
   repository.

   If PATH does not exist, return SVN_ERR_WC_PATH_NOT_FOUND.

   If COPYFROM_URL is non-null, it and COPYFROM_REV are used as
   `copyfrom' args.  This is for copy operations, where one wants
   to schedule PATH for addition with a particular history.

   When the PATH has been added, then NOTIFY_FUNC will be called
   (if it is not NULL) with the NOTIFY_BATON and the path.

   ### This function currently does double duty -- it is also
   ### responsible for "switching" a working copy directory over to a
   ### new copyfrom ancestry and scheduling it for addition.  Here is
   ### the old doc string from Ben, lightly edited to bring it
   ### up-to-date, explaining the true, secret life of this function:

   Given a PATH within a working copy of type KIND, follow this algorithm:

      - if PATH is not under version control:
         - Place it under version control and schedule for addition; 
           if COPYFROM_URL is non-null, use it and COPYFROM_REV as
           'copyfrom' history

      - if PATH is already under version control:
            (This can only happen when a directory is copied, in which
             case ancestry must have been supplied as well.)

         -  Schedule the directory itself for addition with copyfrom history.
         -  Mark all its children with a 'copied' flag
         -  Rewrite all the URLs to what they will be after a commit.
         -  ### TODO:  remove old wcprops too, see the '###'below

   ### I think possibly the "switchover" functionality should be
   ### broken out into a separate function, but its all intertwined in
   ### the code right now.  Ben, thoughts?  Hard?  Easy?  Mauve? */
svn_error_t *svn_wc_add (svn_stringbuf_t *path,
                         svn_stringbuf_t *copyfrom_url,
                         svn_revnum_t copyfrom_rev,
                         svn_wc_notify_func_t notify_func,
                         void *notify_baton,
                         apr_pool_t *pool);


/* Remove entry NAME in PATH from revision control.  NAME must be
   either a file or SVN_WC_ENTRY_THIS_DIR.

   If NAME is a file, all its info will be removed from PATH's
   administrative directory.  If NAME is SVN_WC_ENTRY_THIS_DIR, then
   PATH's entrire administrative area will be deleted, along with
   *all* the administrative areas anywhere in the tree below PATH.

   Normally, only adminstrative data is removed.  However, if
   DESTROY_WF is set, then all working file(s) and dirs are deleted
   from disk as well.  When called with DESTROY_WF, any locally
   modified files will *not* be deleted, and the special error
   SVN_WC_LEFT_LOCAL_MOD might be returned.  (Callers only need to
   check for this special return value if DESTROY_WF is set.)

   WARNING:  This routine is exported for careful, measured use by
   libsvn_client.  Do *not* call this routine unless you really
   understand what the heck you're doing.  */
svn_error_t *svn_wc_remove_from_revision_control (svn_stringbuf_t *path, 
                                                  svn_stringbuf_t *name,
                                                  svn_boolean_t destroy_wf,
                                                  apr_pool_t *pool);


/* Assuming PATH is under version control and in a state of conflict,
   then take PATH *out* of this state.

   Needless to say, this function doesn't touch conflict markers or
   anything of that sort -- only a human can semantically resolve a
   conflict.  Instead, this function simply marks a file as "having
   been resolved", clearing the way for a commit.  

   The implementation details are opaque, as our "conflicted" criteria
   might change over time.  (At the moment, this routine removes the
   three fulltext 'backup' files and any .prej file created in a conflict.)

   If PATH is not under version control, return SVN_ERR_ENTRY_NOT_FOUND.  
   If PATH isn't in a state of conflict to begin with, do nothing, and
   return SVN_NO_ERROR.  If PATH was successfully taken out of a state
   of conflict, report this information to NOTIFY_FUNC (if non-NULL.)
 */
svn_error_t *svn_wc_resolve_conflict (svn_stringbuf_t *path,
                                      svn_wc_notify_func_t notify_func,
                                      void *notify_baton,
                                      apr_pool_t *pool);


/*** Commits. ***/

/* The RA layer needs 3 functions when doing a commit: */

/* Publically declared, so libsvn_client can pass it off to the RA
   layer to use with any of the next three functions. */
struct svn_wc_close_commit_baton
{
  /* The "prefix" path that must be prepended to each target that
     comes in here.  It's the original path that the user specified to
     the `svn commit' command. */
  svn_stringbuf_t *prefix_path;
};


/* This is of type `svn_ra_close_commit_func_t'.

   Bump each committed PATH to NEW_REVNUM, one at a time, after a
   commit succeeds.  REV_DATE and REV_AUTHOR are the (server-side)
   date and author of the new revision; one or both may be NULL.

   If RECURSE is set and PATH is a directory, then bump every
   versioned object at or under PATH.  This is usually done for
   copied trees.  */
svn_error_t *svn_wc_process_committed (svn_stringbuf_t *path,
                                       svn_boolean_t recurse,
                                       svn_revnum_t new_revnum,
                                       const char *rev_date,
                                       const char *rev_author,
                                       apr_pool_t *pool);



/* Update working copy PATH with NEW_REVISION after a commit has succeeded.
 * TARGETS is a hash of files/dirs that actually got committed --
 * these are the only ones who we can write log items for, and whose
 * revision numbers will get set.  todo: eventually this hash will be
 * of the sort used by svn_wc__compose_paths(), as with all entries
 * recursers.
 */
svn_error_t *svn_wc_close_commit (void *baton,
                                  svn_revnum_t new_revision,
                                  apr_hash_t *targets,
                                  apr_pool_t *pool);


/* This is a function of type svn_ra_get_wc_prop_t.  Return *VALUE for
   property NAME on PATH.  */
svn_error_t *svn_wc_get_wc_prop (const char *path,
                                 const char *name,
                                 const svn_string_t **value,
                                 apr_pool_t *pool);

/* This is a function of type svn_ra_set_wc_prop_t. Set property NAME
   to VALUE on PATH.  */
svn_error_t *svn_wc_set_wc_prop (const char *path,
                                 const char *name,
                                 const svn_string_t *value,
                                 apr_pool_t *pool);


/* Do a depth-first crawl in a working copy, beginning at PATH.
   Communicate the `state' of the working copy's revisions to
   REPORTER/REPORT_BATON.  Obviously, if PATH is a file instead of a
   directory, this depth-first crawl will be a short one.

   No locks are or logs are created, nor are any animals harmed in the
   process.  No cleanup is necessary.

   After all revisions are reported, REPORTER->finish_report() is
   called, which immediately causes the RA layer to update the working
   copy.  Thus the return value may very well reflect the result of
   the update!

   If RESTORE_FILES is set, then unexpectedly missing working files
   will be restored from the administrative directory's cache. For each
   file restored, the NOTIFY_FUNC function will be called with the
   NOTIFY_BATON and the path of the restored file. NOTIFY_FUNC may
   be NULL if this notification is not required. */
svn_error_t *
svn_wc_crawl_revisions (svn_stringbuf_t *path,
                        const svn_ra_reporter_t *reporter,
                        void *report_baton,
                        svn_boolean_t restore_files,
                        svn_boolean_t recurse,
                        svn_wc_notify_func_t notify_func,
                        void *notify_baton,
                        apr_pool_t *pool);


/*** Updates. ***/

/*
 * Set *WC_ROOT to TRUE if PATH represents a "working copy root",
 * FALSE otherwise.  Use POOL for any intermediate allocations.
 *
 * NOTE: Due to the way in which "WC-root-ness" is calculated, passing
 * a PATH of `.' to this function will always return TRUE.
 */
svn_error_t *svn_wc_is_wc_root (svn_boolean_t *wc_root,
                                svn_stringbuf_t *path,
                                apr_pool_t *pool);


/*
 * Conditionally split PATH into an ANCHOR and TARGET for the purpose
 * of updating and committing.
 *
 * ANCHOR is the directory at which the update or commit editor
 * should be rooted.
 *
 * TARGET is the actual subject (relative to the ANCHOR) of the
 * update/commit, or NULL if the ANCHOR itself is the subject.
 *
 * Do all necessary allocations in POOL.  
 */
svn_error_t *svn_wc_get_actual_target (svn_stringbuf_t *path,
                                       svn_stringbuf_t **anchor,
                                       svn_stringbuf_t **target,
                                       apr_pool_t *pool);


/* Set *EDITOR and *EDIT_BATON to an editor and baton for updating a
 * working copy.
 * 
 * ANCHOR is the local path to the working copy which will be used as
 * the root of our editor.  TARGET is the entry in ANCHOR that will
 * actually be updated, or NULL if all of ANCHOR should be updated.
 *
 * TARGET_REVISION is the repository revision that results from this set
 * of changes.
 */
svn_error_t *svn_wc_get_update_editor (svn_stringbuf_t *anchor,
                                       svn_stringbuf_t *target,
                                       svn_revnum_t target_revision,
                                       svn_boolean_t recurse,
                                       const svn_delta_editor_t **editor,
                                       void **edit_baton,
                                       apr_pool_t *pool);


/* Like svn_wc_get_update_editor(), except that:
 *
 * DEST will be created as a working copy, if it does not exist
 * already.  It is not an error for it to exist; if it does, checkout
 * just behaves like update.
 *
 * It is the caller's job to make sure that DEST is not some other
 * working copy, or that if it is, it will not be damaged by the
 * application of this delta.  The wc library tries to detect
 * such a case and do as little damage as possible, but makes no
 * promises.
 *
 * ANCESTOR_URL is the repository string to be recorded in this
 * working copy.
 */
svn_error_t *svn_wc_get_checkout_editor (svn_stringbuf_t *dest,
                                         svn_stringbuf_t *ancestor_url,
                                         svn_revnum_t target_revision,
                                         svn_boolean_t recurse,
                                         const svn_delta_editor_t **editor,
                                         void **edit_baton,
                                         apr_pool_t *pool);


/* Another variant of svn_wc_get_update_editor(): 
 *
 * Set *EDITOR and *EDIT_BATON to an editor and baton for "switching"
 * a working copy to a new SWITCH_URL.  (Right now, this URL must be
 * within the same repository that the working copy already comes
 * from.)
 * 
 * ANCHOR is the local path to the working copy which will be used as
 * the root of our editor.  TARGET is the entry in ANCHOR that will
 * actually be updated, or NULL if all of ANCHOR should be updated.
 *
 * TARGET_REVISION is the repository revision that results from this set
 * of changes.
 */
svn_error_t *svn_wc_get_switch_editor (svn_stringbuf_t *anchor,
                                       svn_stringbuf_t *target,
                                       svn_revnum_t target_revision,
                                       svn_stringbuf_t *switch_url,
                                       svn_boolean_t recurse,
                                       const svn_delta_editor_t **editor,
                                       void **edit_baton,
                                       apr_pool_t *pool);


/* Given a FILE_PATH already under version control, fully "install" a
   NEW_REVISION of the file.  

   By "install", we mean: the working copy library creates a new
   text-base and prop-base, merges any textual and property changes
   into the working file, and finally updates all metadata so that the
   working copy believes it has a new working revision of the file.
   All of this work includes being sensitive to eol translation,
   keyword substitution, and performing all actions using a journaled
   logfile.

   The caller provides a NEW_TEXT_PATH which points to a temporary
   file containing the 'new' full text of the file at revision
   NEW_REVISION.  This function automatically removes NEW_TEXT_PATH
   upon successful completion.  If there is no new text, then caller
   must set NEW_TEXT_PATH to NULL.

   The caller also provides the new properties for the file in the
   PROPS array; if there are no new props, then caller must pass NULL
   instead.  This argument is an array of svn_prop_t structures, and
   can be interpreted in one of two ways:

      - if IS_FULL_PROPLIST is set, then the array represents the
        complete list of all properties for the file.  It is the new
        'pristine' proplist.

      - if IS_FULL_PROPLIST is unset, then the array represents a set of
        *differences* against the file's existing pristine proplist.
        (A deletion is represented by setting an svn_prop_t's 'value'
        field to NULL.)  

   Note that the PROPS array is expected to contain all categories of
   props, not just 'regular' ones that the user sees.  (See 'enum
   svn_prop_kind').

   If NEW_URL is non-NULL, then this URL will be attached to the file
   in the 'entries' file.  Otherwise, the file will simply "inherit"
   it's URL from the parent dir.

   POOL is used for all bookkeeping work during the installation.
 */
svn_error_t *svn_wc_install_file (const char *file_path,
                                  svn_revnum_t new_revision,
                                  const char *new_text_path,
                                  const apr_array_header_t *props,
                                  svn_boolean_t is_full_proplist,
                                  const char *new_URL,
                                  apr_pool_t *pool);



/* A word about the implementation of working copy property storage:
 *
 * Since properties are key/val pairs, you'd think we store them in
 * some sort of Berkeley DB-ish format, and even store pending changes
 * to them that way too.
 *
 * However, we already have libsvn_subr/hashdump.c working, and it
 * uses a human-readable format.  That will be very handy when we're
 * debugging, and presumably we will not be dealing with any huge
 * properties or property lists initially.  Therefore, we will
 * continue to use hashdump as the internal mechanism for storing and
 * reading from property lists, but note that the interface here is
 * _not_ dependent on that.  We can swap in a DB-based implementation
 * at any time and users of this library will never know the
 * difference.
 */

/* Set *PROPS to a hash table mapping char * names onto
   svn_stringbuf_t * values for all the wc properties of PATH.
   Allocate the table, names, and values in POOL.  If the node has no
   properties, an empty hash is returned. */
svn_error_t *svn_wc_prop_list (apr_hash_t **props,
                               const char *path,
                               apr_pool_t *pool);


/* Set *VALUE to the value of wc property NAME for PATH, allocating
   *VALUE in POOL.  If no such prop, set *VALUE to NULL. */
svn_error_t *svn_wc_prop_get (const svn_string_t **value,
                              const char *name,
                              const char *path,
                              apr_pool_t *pool);

/* Set wc property NAME to VALUE for PATH.  Do any temporary
   allocation in POOL.

   ### todo (issue #406): name could be const char *, value
   svn_string_t instead of svn_stringbuf_t.  */
svn_error_t *svn_wc_prop_set (const char *name,
                              const svn_string_t *value,
                              const char *path,
                              apr_pool_t *pool);


/* Return true iff NAME is a 'normal' property name.  'Normal' is
   defined as a user-visible and user-tweakable property that shows up
   when you fetch a proplist.

   The function currently parses the namespace like so:

     'svn:wc:'  ==>  a wcprop, stored/accessed seperately via different API.

     'svn:entry:' ==> an "entry" prop, shunted into the 'entries' file.

   If these patterns aren't found, then the property is assumed to be
   Normal.  */
svn_boolean_t svn_wc_is_normal_prop (const char *name);



/* Return true iff NAME is a 'wc' property name.  (see above) */
svn_boolean_t svn_wc_is_wc_prop (const char *name);

/* Return true iff NAME is a 'entry' property name.  (see above) */
svn_boolean_t svn_wc_is_entry_prop (const char *name);




/*** Diffs ***/


/* Return an EDITOR/EDIT_BATON for diffing a working copy against the
 * repository.
 *
 * ANCHOR/TARGET represent the base of the hierarchy to be compared.
 *
 * DIFF_CALLBACKS/DIFF_CMD_BATON is the callback table to use when two
 * files are to be compared.
 *
 * RECURSE determines whether to descend into subdirectories when TARGET
 * is a directory.
 */
svn_error_t *svn_wc_get_diff_editor (svn_stringbuf_t *anchor,
                                     svn_stringbuf_t *target,
                                     const svn_diff_callbacks_t *diff_callbacks,
                                     void *diff_cmd_baton,
                                     svn_boolean_t recurse,
                                     const svn_delta_edit_fns_t **editor,
                                     void **edit_baton,
                                     apr_pool_t *pool);


/* Compare working copy against the text-base.
 *
 * ANCHOR/TARGET represent the base of the hierarchy to be compared.
 *
 * DIFF_CALLBACKS/DIFF_CMD_BATON is the callback table to use when two
 * files are to be compared.
 *
 * RECURSE determines whether to descend into subdirectories when TARGET
 * is a directory.
 */
svn_error_t *svn_wc_diff (svn_stringbuf_t *anchor,
                          svn_stringbuf_t *target,
                          const svn_diff_callbacks_t *diff_callbacks,
                          void *diff_cmd_baton,
                          svn_boolean_t recurse,
                          apr_pool_t *pool);

/* Given a PATH to a file or directory under version control, discover
   any local changes made to properties.  Return these changes as an
   array of svn_prop_t structures stored in *PROPCHANGES.  The
   structures and array will be allocated in POOL.

   If there are no local property modifications on PATH, then set
   *PROPCHANGES to NULL. 

   If ORIGINAL_PROPS is non-NULL, then set *ORIGINAL_PROPS to
   hashtable (const char *name -> svn_stringbuf_t *value) that
   represents the 'pristine' property list of PATH.  This hashtable is
   allocated in POOL, and can be used to compare old and new values of
   properties.
*/
svn_error_t *svn_wc_get_prop_diffs (apr_array_header_t **propchanges,
                                    apr_hash_t **original_props,
                                    const char *path,
                                    apr_pool_t *pool);



/* Given two property hashes, deduce the differences between them
   (from BASEPROPS -> LOCALPROPS).  Return these changes as a series
   of svn_prop_t structures stored in LOCAL_PROPCHANGES, allocated
   from POOL.
   
   For note, here's a quick little table describing the logic of this
   routine:

   basehash        localhash         event
   --------        ---------         -----
   value = foo     value = NULL      Deletion occurred.
   value = foo     value = bar       Set occurred (modification)
   value = NULL    value = baz       Set occurred (creation)
*/
svn_error_t *
svn_wc_get_local_propchanges (apr_array_header_t **local_propchanges,
                              apr_hash_t *localprops,
                              apr_hash_t *baseprops,
                              apr_pool_t *pool);



/* Given paths to three fulltexts, merge the differences between LEFT
   and RIGHT into MERGE_TARGET.  (It may help to know that LEFT,
   RIGHT, and MERGE_TARGET correspond to "OLDER", "YOURS", and "MINE",
   respectively, in the diff3 documentation.)  Use POOL for any
   temporary allocation.

   This function assumes that LEFT and RIGHT are in repository-normal
   form (linefeeds, with keywords contracted); if necessary,
   MERGE_TARGET is temporarily converted to this form to receive the
   changes, then translated back again.

   MERGE_TARGET must be under version control; if it is not, return
   SVN_ERR_NO_SUCH_ENTRY.

   If no conflict results from the merge, return without error.  If
   there is a conflict, then

     * Put conflict markers around the conflicting regions in
       MERGE_TARGET, labeled with LEFT_LABEL, RIGHT_LABEL, and
       TARGET_LABEL.  (If any of these labels are NULL, default values
       will be used.)
 
     * Copy LEFT, RIGHT, and the original MERGE_TARGET to unique names
       in the same directory as MERGE_TARGET, ending with the suffixes
       ".LEFT_LABEL", ".RIGHT_LABEL", and ".TARGET_LABEL"
       respectively.

     * Mark the entry for MERGE_TARGET as "conflicted", and track the
       abovementioned backup files in the entry as well.

     * Return the error SVN_ERR_WC_CONFLICT.

   Binary case:

    If all three files are binary files, then no merging is attempted.
    Instead, a variant of the conflict procedure is carried out: the
    working MERGE_TARGET is untouched, and copies of LEFT and RIGHT
    are created next to it using LEFT_LABEL and RIGHT_LABEL.
    MERGE_TARGET's entry is marked as "conflicted", and begins
    tracking the two backup files.  SVN_ERR_WC_CONFLICT is returned.

*/
svn_error_t *svn_wc_merge (const char *left,
                           const char *right,
                           const char *merge_target,
                           const char *left_label,
                           const char *right_label,
                           const char *target_label,
                           apr_pool_t *pool);


/* Given a PATH under version control, merge an array of PROPCHANGES
   into the path's existing properties.  PROPCHANGES is an array of
   svn_prop_t objects.

   If conflicts are found when merging, they are described in a
   temporary .prej file (or appended to an already-existing .prej
   file), and the entry is marked "conflicted".
*/
svn_error_t *
svn_wc_merge_prop_diffs (const char *path,
                         const apr_array_header_t *propchanges,
                         apr_pool_t *pool);



/* Given a PATH to a wc file, return a PRISTINE_PATH which points to a
   pristine version of the file.  This is needed so clients can do
   diffs.  If the WC has no text-base, return a NULL instead of a
   path. */
svn_error_t *svn_wc_get_pristine_copy_path (svn_stringbuf_t *path,
                                            svn_stringbuf_t **pristine_path,
                                            apr_pool_t *pool);


/* Recurse from PATH, cleaning up unfinished log business.  Perform
   necessary allocations in POOL.  */
svn_error_t *
svn_wc_cleanup (svn_stringbuf_t *path, apr_pool_t *pool);


/* Revert changes to PATH (perhaps in a RECURSIVE fashion).  Perform
   necessary allocations in POOL.

   For each item reverted, NOTIFY_FUNC will be called with NOTIFY_BATON
   and the path of the reverted item. NOTIFY_FUNC may be NULL if this
   notification is not needed.  */
svn_error_t *
svn_wc_revert (svn_stringbuf_t *path, 
               svn_boolean_t recursive, 
               svn_wc_notify_func_t notify_func,
               void *notify_baton,
               apr_pool_t *pool);



/*** Authentication files ***/

/* Get the *CONTENTS of FILENAME in the authentcation area of PATH's
   administrative directory, allocated in POOL.  PATH must be a
   working copy directory. If file does not exist,
   SVN_ERR_WC_PATH_NOT_FOUND is returned. */
svn_error_t *
svn_wc_get_auth_file (svn_stringbuf_t *path,
                      const char *filename,
                      svn_stringbuf_t **contents,
                      apr_pool_t *pool);


/* Store a file named FILENAME with CONTENTS in the authentication
   area of PATH's administrative directory.  PATH must be a working
   copy directory.  If no such file exists, it will be created.  If
   the file exists already, it will be completely overwritten with the
   new contents.  If RECURSE is set, this file will be stored in every
   administrative area below PATH as well. */
svn_error_t *
svn_wc_set_auth_file (svn_stringbuf_t *path,
                      svn_boolean_t recurse,
                      const char *filename,
                      svn_stringbuf_t *contents,
                      apr_pool_t *pool);



/*** Tmp files ***/

/* Create a unique temporary file in administrative tmp/ area of
   directory PATH.  Return a handle in *FP.
   
   The flags will be APR_WRITE | APR_CREATE | APR_EXCL and
   optionally APR_DELONCLOSE (if the delete_on_close argument is set TRUE).

   This means that as soon as FP is closed, the tmp file will vanish.  */
svn_error_t *
svn_wc_create_tmp_file (apr_file_t **fp,
                        svn_stringbuf_t *path,
                        svn_boolean_t delete_on_close,
                        apr_pool_t *pool);



/*** Eol conversion and keyword expansion. ***/

/* Values used in keyword expansion. */
typedef struct svn_wc_keywords_t
{
  const svn_string_t *revision;
  const svn_string_t *date;
  const svn_string_t *author;
  const svn_string_t *url;
} svn_wc_keywords_t;


/* Return TRUE if A and B do not hold the same keywords.
 *
 * If COMPARE_VALUES is set, "same" means that the A and B contain
 * exactly the same set of keywords, and the values of corresponding
 * keywords match as well.  Else if COMPARE_VALUES is not set, then
 * "same" merely means that A and B hold the same set of keywords,
 * although those keywords' values might differ.
 *
 * A and/or B may be NULL; for purposes of comparison, NULL is
 * equivalent to holding no keywords.
 */
svn_boolean_t svn_wc_keywords_differ (svn_wc_keywords_t *a,
                                      svn_wc_keywords_t *b,
                                      svn_boolean_t compare_values);


/* Copy the contents of SRC to DST, overwriting DST if it exists,
   possibly performing line ending and keyword translations.

   If EOL_STR is non-NULL, replace whatever bytestring SRC uses to
   denote line endings with EOL_STR in the output.  If SRC has an
   inconsistent line ending style, then: if REPAIR is FALSE, return
   SVN_ERR_IO_INCONSISTENT_EOL and remove DST, else if REPAIR is TRUE,
   convert any line ending in SRC to EOL_STR in DST.  Recognized line
   endings are: "\n", "\r", and "\r\n".

   Expand and contract keywords using the contents of KEYWORDS as the
   new values.  If EXPAND is TRUE, expand contracted keywords and
   re-expand expanded keywords.  If EXPAND is FALSE, contract expanded
   keywords and ignore contracted ones.  NULL for any of the keyword
   values (KEYWORDS->revision, e.g.) indicates that that keyword
   should be ignored (not contracted or expanded).  If the
   KEYWORDS structure itself is NULL, keyword substition will be
   altogether ignored.

   Detect only keywords that are no longer than SVN_IO_MAX_KEYWORD_LEN
   bytes, including the delimiters and the keyword itself.

   If anything goes wrong during the copy, attempt to delete DST (if
   it exists).

   Recommendation: if EXPAND is false, then you don't care about the
   keyword values, so pass empty strings as non-null signifiers.

   Notes: 

   See svn_wc__get_keywords() and svn_wc__get_eol_style() for a
   convenient way to get EOL_STR and KEYWORDS if in libsvn_wc.

   If EOL_STR and KEYWORDS are NULL, behavior is just a byte-for-byte
   copy.  */
svn_error_t *svn_wc_copy_and_translate (const char *src,
                                        const char *dst,
                                        const char *eol_str,
                                        svn_boolean_t repair,
                                        svn_wc_keywords_t *keywords,
                                        svn_boolean_t expand,
                                        apr_pool_t *pool);



/* Set *XLATED_P to a path to a possibly translated copy of versioned
 * file VFILE, or to VFILE itself if no translation is necessary.
 * That is, if VFILE's properties indicate newline conversion or
 * keyword expansion, point *XLATED_P to a copy of VFILE whose
 * newlines are unconverted and keywords contracted, in whatever
 * manner is indicated by VFILE's properties; otherwise, set *XLATED_P
 * to VFILE.
 *
 * Caller is responsible for detecting if they are different (pointer
 * comparison is sufficient), and for removing *XLATED_P if
 * necessary.
 *
 * This function is generally used to get a file that can be compared
 * meaningfully against VFILE's text base.
 *
 * If *XLATED_P is different from VFILE, then choose *XLATED_P's name
 * using svn_io_open_unique_file() with SVN_WC__TMP_EXT, and allocate
 * it in POOL.  Also use POOL for any temporary allocation.
 *
 * If an error is returned, the effect on *XLATED_P is undefined.
 */
svn_error_t *svn_wc_translated_file (svn_stringbuf_t **xlated_p,
                                     svn_stringbuf_t *vfile,
                                     apr_pool_t *pool);



/*** Locking. ***/

/* Lock the working copy administrative area.
   Wait for WAIT_FOR seconds if encounter another lock, trying again every
   second, then return 0 if success or an SVN_ERR_WC_LOCKED error if
   failed to obtain the lock. */
svn_error_t *svn_wc_lock (svn_stringbuf_t *path, 
                          int wait_for, 
                          apr_pool_t *pool);

/* Unlock PATH, or error if can't. */
svn_error_t *svn_wc_unlock (svn_stringbuf_t *path, 
                            apr_pool_t *pool);

/* Set *LOCKED to non-zero if PATH is locked, else set it to zero. */
svn_error_t *svn_wc_locked (svn_boolean_t *locked, 
                            svn_stringbuf_t *path,
                            apr_pool_t *pool);



/*** Text/Prop Deltas Using an Editor ***/


/* Given a PATH (with FILE_BATON) representing a file with local
   textual modifications, transmit those modifications using EDITOR,
   closing the FILE BATON after the textual mod has been transmitted.
   Use POOL for all allocations.

   If FULLTEXT, the text of the file will be sent through the editor
   interface as full-text, else a diff between the file and its
   current text-base will be transmitted.

   If a temporary file remains after this function is finished, the
   path to that file is returned in *TEMPFILE (so the caller can clean
   this up if it wishes to do so).

   This in intended to be suitable for use with both infix and postfix
   text-delta styled editor drivers.  */
svn_error_t *svn_wc_transmit_text_deltas (svn_stringbuf_t *path,
                                          svn_boolean_t fulltext,
                                          const svn_delta_editor_t *editor,
                                          void *file_baton,
                                          svn_stringbuf_t **tempfile,
                                          apr_pool_t *pool);


/* Given a PATH of a given node KIND, transmit all local property
   modifications using the appropriate EDITOR method (in conjunction
   with BATON).  Use POOL for all allocations.

   If a temporary file remains after this function is finished, the
   path to that file is returned in *TEMPFILE (so the caller can clean
   this up if it wishes to do so).  */
svn_error_t *svn_wc_transmit_prop_deltas (svn_stringbuf_t *path,
                                          svn_node_kind_t kind,
                                          const svn_delta_editor_t *editor,
                                          void *baton,
                                          svn_stringbuf_t **tempfile,
                                          apr_pool_t *pool);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* SVN_WC_H */

/* ----------------------------------------------------------------
 * local variables:
 * eval: (load-file "../../tools/dev/svn-dev.el")
 * end: 
 */
