/*
 * cmdline.c :  Helpers for command-line programs.
 *
 * ====================================================================
 * Copyright (c) 2003-2008 CollabNet.  All rights reserved.
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


#include <stdlib.h>             /* for atexit() */
#include <stdio.h>              /* for setvbuf() */
#include <locale.h>             /* for setlocale() */

#ifndef WIN32
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include <apr_errno.h>          /* for apr_strerror */
#include <apr_general.h>        /* for apr_initialize/apr_terminate */
#include <apr_atomic.h>         /* for apr_atomic_init */
#include <apr_strings.h>        /* for apr_snprintf */
#include <apr_pools.h>

#include "svn_cmdline.h"
#include "svn_dso.h"
#include "svn_path.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_nls.h"
#include "svn_auth.h"
#include "svn_version.h"
#include "utf_impl.h"
#include "svn_config.h"

#include "svn_private_config.h"

#include "win32_crashrpt.h"

/* The stdin encoding. If null, it's the same as the native encoding. */
static const char *input_encoding = NULL;

/* The stdout encoding. If null, it's the same as the native encoding. */
static const char *output_encoding = NULL;


int
svn_cmdline_init(const char *progname, FILE *error_stream)
{
  apr_status_t status;
  apr_pool_t *pool;

#ifndef WIN32
  {
    struct stat st;

    /* The following makes sure that file descriptors 0 (stdin), 1
       (stdout) and 2 (stderr) will not be "reused", because if
       e.g. file descriptor 2 would be reused when opening a file, a
       write to stderr would write to that file and most likely
       corrupt it. */
#ifdef AS400_UTF8
    /* Even with the UTF support in V5R4 a few functions on OS400 don't
     * support UTF-8 string arguments and require EBCDIC paths, including
     * open(). */
#pragma convert(0)
#endif
    if ((fstat(0, &st) == -1 && open("/dev/null", O_RDONLY) == -1) ||
        (fstat(1, &st) == -1 && open("/dev/null", O_WRONLY) == -1) ||
        (fstat(2, &st) == -1 && open("/dev/null", O_WRONLY) == -1))
#ifdef AS400_UTF8
#pragma convert(1208)
#endif
      {
        if (error_stream)
          fprintf(error_stream, "%s: error: cannot open '/dev/null'\n",
                  progname);
        return EXIT_FAILURE;
      }
  }
#endif

  /* Ignore any errors encountered while attempting to change stream
     buffering, as the streams should retain their default buffering
     modes. */
  if (error_stream)
    setvbuf(error_stream, NULL, _IONBF, 0);
#ifndef WIN32
  setvbuf(stdout, NULL, _IOLBF, 0);
#endif

#ifdef WIN32
#if _MSC_VER < 1400
  /* Initialize the input and output encodings. */
  {
    static char input_encoding_buffer[16];
    static char output_encoding_buffer[16];

    apr_snprintf(input_encoding_buffer, sizeof input_encoding_buffer,
                 "CP%u", (unsigned) GetConsoleCP());
    input_encoding = input_encoding_buffer;

    apr_snprintf(output_encoding_buffer, sizeof output_encoding_buffer,
                 "CP%u", (unsigned) GetConsoleOutputCP());
    output_encoding = output_encoding_buffer;
  }
#endif /* _MSC_VER < 1400 */

#ifdef SVN_USE_WIN32_CRASHHANDLER
  /* Attach (but don't load) the crash handler */
  SetUnhandledExceptionFilter(svn__unhandled_exception_filter);
#endif

#endif /* WIN32 */

  /* C programs default to the "C" locale. But because svn is supposed
     to be i18n-aware, it should inherit the default locale of its
     environment.  */
  if (!setlocale(LC_ALL, "")
      && !setlocale(LC_CTYPE, ""))
    {
      if (error_stream)
        {
          const char *env_vars[] = { "LC_ALL", "LC_CTYPE", "LANG", NULL };
          const char **env_var = &env_vars[0], *env_val = NULL;
          while (*env_var)
            {
              env_val = getenv(*env_var);
              if (env_val && env_val[0])
                break;
              ++env_var;
            }

          if (!*env_var)
            {
              /* Unlikely. Can setlocale fail if no env vars are set? */
              --env_var;
              env_val = "not set";
            }

          fprintf(error_stream,
                  "%s: warning: cannot set LC_CTYPE locale\n"
                  "%s: warning: environment variable %s is %s\n"
                  "%s: warning: please check that your locale name is correct\n",
                  progname, progname, *env_var, env_val, progname);
        }
    }

  /* Initialize the APR subsystem, and register an atexit() function
     to Uninitialize that subsystem at program exit. */
  status = apr_initialize();
  if (status)
    {
      if (error_stream)
        {
          char buf[1024];
          apr_strerror(status, buf, sizeof(buf) - 1);
          fprintf(error_stream,
                  "%s: error: cannot initialize APR: %s\n",
                  progname, buf);
        }
      return EXIT_FAILURE;
    }

  /* This has to happen before any pools are created. */
  svn_dso_initialize();

  if (0 > atexit(apr_terminate))
    {
      if (error_stream)
        fprintf(error_stream,
                "%s: error: atexit registration failed\n",
                progname);
      return EXIT_FAILURE;
    }

  /* Create a pool for use by the UTF-8 routines.  It will be cleaned
     up by APR at exit time. */
  pool = svn_pool_create(NULL);
  svn_utf_initialize(pool);

  {
    svn_error_t *err = svn_nls_init();
    if (err)
      {
        if (error_stream && err->message)
          fprintf(error_stream, "%s", err->message);

        svn_error_clear(err);
        return EXIT_FAILURE;
      }
  }

  return EXIT_SUCCESS;
}


svn_error_t *
svn_cmdline_cstring_from_utf8(const char **dest,
                              const char *src,
                              apr_pool_t *pool)
{
  if (output_encoding == NULL)
    return svn_utf_cstring_from_utf8(dest, src, pool);
  else
    return svn_utf_cstring_from_utf8_ex2(dest, src, output_encoding, pool);
}


const char *
svn_cmdline_cstring_from_utf8_fuzzy(const char *src,
                                    apr_pool_t *pool)
{
  return svn_utf__cstring_from_utf8_fuzzy(src, pool,
                                          svn_cmdline_cstring_from_utf8);
}


svn_error_t *
svn_cmdline_cstring_to_utf8(const char **dest,
                            const char *src,
                            apr_pool_t *pool)
{
  if (input_encoding == NULL)
    return svn_utf_cstring_to_utf8(dest, src, pool);
  else
    return svn_utf_cstring_to_utf8_ex2(dest, src, input_encoding, pool);
}


svn_error_t *
svn_cmdline_path_local_style_from_utf8(const char **dest,
                                       const char *src,
                                       apr_pool_t *pool)
{
  return svn_cmdline_cstring_from_utf8(dest,
                                       svn_path_local_style(src, pool),
                                       pool);
}

svn_error_t *
svn_cmdline_printf(apr_pool_t *pool, const char *fmt, ...)
{
  const char *message;
  va_list ap;

  /* A note about encoding issues:
   * APR uses the execution character set, but here we give it UTF-8 strings,
   * both the fmt argument and any other string arguments.  Since apr_pvsprintf
   * only cares about and produces ASCII characters, this works under the
   * assumption that all supported platforms use an execution character set
   * with ASCII as a subset.
   */

  va_start(ap, fmt);
  message = apr_pvsprintf(pool, fmt, ap);
  va_end(ap);

  return svn_cmdline_fputs(message, stdout, pool);
}

svn_error_t *
svn_cmdline_fprintf(FILE *stream, apr_pool_t *pool, const char *fmt, ...)
{
  const char *message;
  va_list ap;

  /* See svn_cmdline_printf () for a note about character encoding issues. */

  va_start(ap, fmt);
  message = apr_pvsprintf(pool, fmt, ap);
  va_end(ap);

  return svn_cmdline_fputs(message, stream, pool);
}

svn_error_t *
svn_cmdline_fputs(const char *string, FILE* stream, apr_pool_t *pool)
{
  svn_error_t *err;
  const char *out;

  err = svn_cmdline_cstring_from_utf8(&out, string, pool);

  if (err)
    {
      svn_error_clear(err);
      out = svn_cmdline_cstring_from_utf8_fuzzy(string, pool);
    }

  /* On POSIX systems, errno will be set on an error in fputs, but this might
     not be the case on other platforms.  We reset errno and only
     use it if it was set by the below fputs call.  Else, we just return
     a generic error. */
  errno = 0;

  if (fputs(out, stream) == EOF)
    {
      if (errno)
        return svn_error_wrap_apr(errno, _("Write error"));
      else
        return svn_error_create
          (SVN_ERR_IO_WRITE_ERROR, NULL, NULL);
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_cmdline_fflush(FILE *stream)
{
  /* See comment in svn_cmdline_fputs about use of errno and stdio. */
  errno = 0;
  if (fflush(stream) == EOF)
    {
      if (errno)
        return svn_error_wrap_apr(errno, _("Write error"));
      else
        return svn_error_create(SVN_ERR_IO_WRITE_ERROR, NULL, NULL);
    }

  return SVN_NO_ERROR;
}

const char *svn_cmdline_output_encoding(apr_pool_t *pool)
{
  if (output_encoding)
    return apr_pstrdup(pool, output_encoding);
  else
    return SVN_APR_LOCALE_CHARSET;
}

int
svn_cmdline_handle_exit_error(svn_error_t *err,
                              apr_pool_t *pool,
                              const char *prefix)
{
  svn_handle_error2(err, stderr, FALSE, prefix);
  svn_error_clear(err);
  if (pool)
    svn_pool_destroy(pool);
  return EXIT_FAILURE;
}

#if defined(SVN_HAVE_KWALLET) || defined(SVN_HAVE_GNOME_KEYRING)
/* Dynamically load authentication simple provider. */
static svn_boolean_t
get_auth_simple_provider(svn_auth_provider_object_t **provider,
                         const char *provider_name,
                         apr_pool_t *pool)
{
  apr_dso_handle_t *dso;
  apr_dso_handle_sym_t provider_symbol;
  const char *libname;
  const char *funcname;
  svn_boolean_t ret = FALSE;
  libname = apr_psprintf(pool,
                         "libsvn_auth_%s-%d.so.0",
                         provider_name,
                         SVN_VER_MAJOR);
  funcname = apr_psprintf(pool,
                          "svn_auth_get_%s_simple_provider",
                          provider_name);
  svn_error_clear(svn_dso_load(&dso, libname));
  if (dso)
    {
      if (! apr_dso_sym(&provider_symbol, dso, funcname))
        {
          svn_auth_simple_provider_func_t func;
          func = (svn_auth_simple_provider_func_t) provider_symbol;
          func(provider, pool);
          ret = TRUE;
        }
    }
  return ret;
}
#endif

svn_error_t *
svn_cmdline_setup_auth_baton(svn_auth_baton_t **ab,
                             svn_boolean_t non_interactive,
                             const char *auth_username,
                             const char *auth_password,
                             const char *config_dir,
                             svn_boolean_t no_auth_cache,
                             svn_config_t *cfg,
                             svn_cancel_func_t cancel_func,
                             void *cancel_baton,
                             apr_pool_t *pool)
{
  svn_boolean_t store_password_val = TRUE;
  svn_boolean_t store_auth_creds_val = TRUE;
  svn_auth_provider_object_t *provider;
  svn_cmdline_prompt_baton2_t *pb = NULL;

  /* The whole list of registered providers */
  apr_array_header_t *providers
    = apr_array_make(pool, 12, sizeof(svn_auth_provider_object_t *));

  /* If we have a cancellation function, cram it and the stuff it
     needs into the prompt baton. */
  if (cancel_func)
    {
      pb = apr_palloc(pool, sizeof(*pb));
      pb->cancel_func = cancel_func;
      pb->cancel_baton = cancel_baton;
      pb->config_dir = config_dir;
    }

  /* Disk-caching auth providers, for both
     'username/password' creds and 'username' creds,
     which store passwords encrypted.  */
#if defined(WIN32) && !defined(__MINGW32__)
  svn_auth_get_windows_simple_provider(&provider, pool);
  APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
#endif
#ifdef SVN_HAVE_KEYCHAIN_SERVICES
  svn_auth_get_keychain_simple_provider(&provider, pool);
  APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
#endif
#ifdef SVN_HAVE_KWALLET
  if (get_auth_simple_provider(&provider, "kwallet", pool))
  {
    APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
  }
#endif
#ifdef SVN_HAVE_GNOME_KEYRING
  if (get_auth_simple_provider(&provider, "gnome_keyring", pool))
  {
    APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
  }
#endif
  if (non_interactive == FALSE)
    {
      /* This provider is odd in that it isn't a prompting provider in
         the classic sense.  That is, it doesn't need to prompt in
         order to get creds, but it *does* need to prompt the user
         regarding the *cache storage* of creds. */
      svn_auth_get_simple_provider2(&provider,
                                    svn_cmdline_auth_plaintext_prompt,
                                    pb, pool);
    }
  else
    {
      svn_auth_get_simple_provider2(&provider, NULL, NULL, pool);
    }
  APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
  svn_auth_get_username_provider(&provider, pool);
  APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

  /* The server-cert, client-cert, and client-cert-password providers. */
#if defined(WIN32) && !defined(__MINGW32__)
  svn_auth_get_windows_ssl_server_trust_provider(&provider, pool);
  APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
#endif
  svn_auth_get_ssl_server_trust_file_provider(&provider, pool);
  APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
  svn_auth_get_ssl_client_cert_file_provider(&provider, pool);
  APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
  svn_auth_get_ssl_client_cert_pw_file_provider(&provider, pool);
  APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

  if (non_interactive == FALSE)
    {
      /* Two basic prompt providers: username/password, and just username. */
      svn_auth_get_simple_prompt_provider(&provider,
                                          svn_cmdline_auth_simple_prompt,
                                          pb,
                                          2, /* retry limit */
                                          pool);
      APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

      svn_auth_get_username_prompt_provider
        (&provider, svn_cmdline_auth_username_prompt, pb,
         2, /* retry limit */ pool);
      APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

      /* Three ssl prompt providers, for server-certs, client-certs,
         and client-cert-passphrases.  */
      svn_auth_get_ssl_server_trust_prompt_provider
        (&provider, svn_cmdline_auth_ssl_server_trust_prompt, pb, pool);
      APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

      svn_auth_get_ssl_client_cert_prompt_provider
        (&provider, svn_cmdline_auth_ssl_client_cert_prompt, pb, 2, pool);
      APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

      svn_auth_get_ssl_client_cert_pw_prompt_provider
        (&provider, svn_cmdline_auth_ssl_client_cert_pw_prompt, pb, 2, pool);
      APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
    }

  /* Build an authentication baton to give to libsvn_client. */
  svn_auth_open(ab, providers, pool);

  /* Place any default --username or --password credentials into the
     auth_baton's run-time parameter hash. */
  if (auth_username)
    svn_auth_set_parameter(*ab, SVN_AUTH_PARAM_DEFAULT_USERNAME,
                           auth_username);
  if (auth_password)
    svn_auth_set_parameter(*ab, SVN_AUTH_PARAM_DEFAULT_PASSWORD,
                           auth_password);

  /* Same with the --non-interactive option. */
  if (non_interactive)
    svn_auth_set_parameter(*ab, SVN_AUTH_PARAM_NON_INTERACTIVE, "");

  if (config_dir)
    svn_auth_set_parameter(*ab, SVN_AUTH_PARAM_CONFIG_DIR,
                           config_dir);

  /* Determine whether storing passwords in any form is allowed.
   * This is the deprecated location for this option, the new
   * location is SVN_CONFIG_CATEGORY_SERVERS. The RA layer may
   * override the value we set here. */
  SVN_ERR(svn_config_get_bool(cfg, &store_password_val,
                              SVN_CONFIG_SECTION_AUTH,
                              SVN_CONFIG_OPTION_STORE_PASSWORDS,
                              SVN_CONFIG_DEFAULT_OPTION_STORE_PASSWORDS));

  if (! store_password_val)
    svn_auth_set_parameter(*ab, SVN_AUTH_PARAM_DONT_STORE_PASSWORDS, "");

  /* Determine whether we are allowed to write to the auth/ area.
   * This is the deprecated location for this option, the new
   * location is SVN_CONFIG_CATEGORY_SERVERS. The RA layer may
   * override the value we set here. */
  SVN_ERR(svn_config_get_bool(cfg, &store_auth_creds_val,
                              SVN_CONFIG_SECTION_AUTH,
                              SVN_CONFIG_OPTION_STORE_AUTH_CREDS,
                              SVN_CONFIG_DEFAULT_OPTION_STORE_AUTH_CREDS));

  if (no_auth_cache || ! store_auth_creds_val)
    svn_auth_set_parameter(*ab, SVN_AUTH_PARAM_NO_AUTH_CACHE, "");

  return SVN_NO_ERROR;
}

svn_error_t *
svn_cmdline__getopt_init(apr_getopt_t **os,
                         int argc,
                         const char *argv[],
                         apr_pool_t *pool)
{
  apr_status_t apr_err;
#ifdef AS400_UTF8
  /* Convert command line args from EBCDIC to UTF-8. */
  int i;
  for (i = 0; i < argc; ++i)
    {
      char *arg_utf8;
      SVN_ERR(svn_utf_cstring_to_utf8_ex2(&arg_utf8, argv[i],
                                          (const char *)0, pool));
      argv[i] = arg_utf8;
    }
#endif
  apr_err = apr_getopt_init(os, pool, argc, argv);
  if (apr_err)
    return svn_error_wrap_apr(apr_err,
                              _("Error initializing command line arguments"));
  return SVN_NO_ERROR;
}



