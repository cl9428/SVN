/* noderevs.h --- FSFS node revision container
 *
 * ====================================================================
 *    Licensed to the Apache Software Foundation (ASF) under one
 *    or more contributor license agreements.  See the NOTICE file
 *    distributed with this work for additional information
 *    regarding copyright ownership.  The ASF licenses this file
 *    to you under the Apache License, Version 2.0 (the
 *    "License"); you may not use this file except in compliance
 *    with the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing,
 *    software distributed under the License is distributed on an
 *    "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *    KIND, either express or implied.  See the License for the
 *    specific language governing permissions and limitations
 *    under the License.
 * ====================================================================
 */

#ifndef SVN_LIBSVN_FS__NODEREVS_H
#define SVN_LIBSVN_FS__NODEREVS_H

#include "svn_io.h"
#include "fs.h"

/* A collection of related noderevs tends to be widely redundant (similar
 * paths, predecessor ID matching anothers ID, shared representations etc.)
 * Also, the binary representation of a noderev can be much shorter than
 * the ordinary textual variant.
 *
 * In its serialized form, the svn_fs_fs__noderevs_t container extracts
 * most of that redundancy and the run-time representation is also much
 * smaller than sum of the respective node_revision_t objects.
 *
 * As with other containers, this one has two modes: 'construction', in
 * which you may add data to it, and 'getter' in which there is only r/o
 * access to the data.
 */

/* An opaque collection of node revisions.
 */
typedef struct svn_fs_fs__noderevs_t svn_fs_fs__noderevs_t;

/* Create and populate noderev containers. */

/* Create and return a new noderevs container with an initial capacity of
 * INITIAL_COUNT node_revision_t objects.  Allocate the result in POOL.
 */
svn_fs_fs__noderevs_t *
svn_fs_fs__noderevs_create(apr_size_t initial_count,
                           apr_pool_t *pool);

/* Add NODEREV to the CONTAINER. Return the index that identifies the new
 * item in this container.
 */
apr_size_t
svn_fs_fs__noderevs_add(svn_fs_fs__noderevs_t *container,
                        node_revision_t *noderev);

/* Return a rough estimate in bytes for the serialized representation
 * of CONTAINER.
 */
apr_size_t
svn_fs_fs__noderevs_estimate_size(const svn_fs_fs__noderevs_t *container);

/* Read from noderev containers. */

/* From CONTAINER, extract the noderev with the given IDX.  Allocate
 * the result in POOL and return it in *NODEREV_P.
 */
svn_error_t *
svn_fs_fs__noderevs_get(node_revision_t **noderev_p,
                        const svn_fs_fs__noderevs_t *container,
                        apr_size_t idx,
                        apr_pool_t *pool);

/* I/O interface. */

/* Write a serialized representation of CONTAINER to STREAM.  Use POOL for
 * temporary allocations.
 */
svn_error_t *
svn_fs_fs__write_noderevs_container(svn_stream_t *stream,
                                    const svn_fs_fs__noderevs_t *container,
                                    apr_pool_t *pool);

/* Read a noderev container from its serialized representation in STREAM.
 * Allocate the result in RESULT_POOL and return it in *CONTAINER.  Use
 * SCRATCH_POOL for temporary allocations.
 */
svn_error_t *
svn_fs_fs__read_noderevs_container(svn_fs_fs__noderevs_t **container,
                                   svn_stream_t *stream,
                                   apr_pool_t *result_pool,
                                   apr_pool_t *scratch_pool);

#endif