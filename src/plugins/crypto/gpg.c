/**
 * @file
 *
 * @brief module for calling the GPG binary
 *
 * @copyright BSD License (see doc/LICENSE.md or http://www.libelektra.org)
 *
 */

#include "gpg.h"
#include <assert.h>
#include <errno.h>
#include <kdberrors.h>
#include <kdbhelper.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static inline void closePipe (int * pipe)
{
	close (pipe[0]);
	close (pipe[1]);
}

/**
 * @brief checks whether or not a given file exists and is executable.
 * @param file holds the path to the file that should be checked
 * @param errorKey holds an error description if the file does not exist or if it is not executable. Ignored if set to NULL.
 * @retval 1 if the file exists and is executable
 * @retval -1 if the file can not be found
 * @retval -2 if the file exsits but it can not be executed
*/
static int isExecutable (const char * file, Key * errorKey)
{
	if (access (file, F_OK))
	{
		if (errorKey)
		{
			ELEKTRA_SET_ERRORF (ELEKTRA_ERROR_CRYPTO_GPG, errorKey, "gpg binary %s not found", file);
		}
		return -1;
	}

	if (access (file, X_OK))
	{
		if (errorKey)
		{
			ELEKTRA_SET_ERRORF (ELEKTRA_ERROR_CRYPTO_GPG, errorKey, "gpg binary %s has no permission to execute", file);
		}
		return -2;
	}

	return 1;
}

/**
 * @brief lookup if the test mode for unit testing is enabled.
 * @param conf KeySet holding the plugin configuration.
 * @retval 0 test mode is not enabled
 * @retval 1 test mode is enabled
 */
static int inTestMode (KeySet * conf)
{
	Key * k = ksLookupByName (conf, ELEKTRA_CRYPTO_PARAM_GPG_UNIT_TEST, 0);
	if (k && !strcmp (keyString (k), "1"))
	{
		return 1;
	}
	return 0;
}

/**
 * @brief concatenates dir and file.
 * @param errorKey holds an error description in case of failure.
 * @param dir contains the path to a directory
 * @param file contains a file name
 * @returns an allocated string containing "dir/file" which must be freed by the caller or NULL in case of error.
 */
static char * genGpgCandidate (Key * errorKey, char * dir, const char * file)
{
	const size_t resultLen = strlen (dir) + strlen (file) + 2;
	char * result = elektraMalloc (resultLen);
	if (!result)
	{
		ELEKTRA_SET_ERROR (87, errorKey, "Memory allocation failed");
		return NULL;
	}
	snprintf (result, resultLen, "%s/%s", dir, file);
	return result;
}

/**
 * @brief lookup binary file bin in the PATH environment variable.
 * @param errorKey holds an error description in case of failure.
 * @param bin the binary file to look for
 * @param result holds an allocated string containing the full path to the binary file or NULL in case of error. Must be freed by the caller.
 * @retval -1 if an error occurred. See errorKey for a description.
 * @retval 0 if the binary could not be found within PATH.
 * @retval 1 if the binary was found and the full path was stored in result.
 */
static int searchPathForBin (Key * errorKey, const char * bin, char ** result)
{
	*result = NULL;

	const char * envPath = getenv ("PATH");
	if (envPath)
	{
		const size_t envPathLen = strlen (envPath) + 1;
		char * dir;

		char * path = elektraMalloc (envPathLen);
		if (!path)
		{
			ELEKTRA_SET_ERROR (87, errorKey, "Memory allocation failed");
			return -1;
		}
		memcpy (path, envPath, envPathLen);
		// save start of path as strsep() modifies path while splitting it up
		char * pathBegin = path;
		while ((dir = strsep (&path, ":")) != NULL)
		{
			char * candidate = genGpgCandidate (errorKey, dir, bin);
			if (!candidate)
			{
				elektraFree (pathBegin);
				return -1;
			}
			if (access (candidate, X_OK) == 0)
			{
				*result = candidate;
				elektraFree (pathBegin);
				return 1;
			}
			elektraFree (candidate);
		}
		elektraFree (pathBegin);
	}
	return 0;
}

/**
 * @brief lookup the path to the gpg binary in conf.
 * @param gpgBin holds allocated path to the gpg binary to be used or NULL in case of an error. Must bee freed by the caller.
 * @param conf KeySet holding the plugin configuration.
 * @param errorKey holds an error description if something goes wrong.
 * @retval 1 on success.
 * @retval -1 on error. In this case errorkey holds an error description.
 */
static int getGpgBinary (char ** gpgBin, KeySet * conf, Key * errorKey)
{
	*gpgBin = NULL;

	// plugin configuration has highest priority
	Key * k = ksLookupByName (conf, ELEKTRA_CRYPTO_PARAM_GPG_BIN, 0);
	if (k)
	{
		const char * configPath = keyString (k);
		const size_t configPathLen = strlen (configPath);
		if (configPathLen > 0)
		{
			*gpgBin = elektraMalloc (configPathLen + 1);
			if (!(*gpgBin))
			{
				ELEKTRA_SET_ERROR (87, errorKey, "Memory allocation failed");
				return -1;
			}
			strncpy (*gpgBin, configPath, configPathLen);
			return 1;
		}
	}

	// search PATH for gpg and gpg2 binaries
	switch (searchPathForBin (errorKey, "gpg2", gpgBin))
	{
	case 1: // success
		return 1;

	case -1: // error
		return -1;

	default: // not found
		break;
	}

	switch (searchPathForBin (errorKey, "gpg", gpgBin))
	{
	case 1: // success
		return 1;

	case -1: // error
		return -1;

	default: // not found
		break;
	}


	// last resort number one - check for gpg2 at /usr/bin/gpg2
	// NOTE this might happen if the PATH variable is empty
	if (isExecutable (ELEKTRA_CRYPTO_DEFAULT_GPG2_BIN, NULL) == 1)
	{
		*gpgBin = elektraStrDup (ELEKTRA_CRYPTO_DEFAULT_GPG2_BIN);
		if (!(*gpgBin))
		{
			ELEKTRA_SET_ERROR (87, errorKey, "Memory allocation failed");
			return -1;
		}
		return 1;
	}

	// last last resort - check for /usr/bin/gpg
	if (isExecutable (ELEKTRA_CRYPTO_DEFAULT_GPG1_BIN, NULL) == 1)
	{
		*gpgBin = elektraStrDup (ELEKTRA_CRYPTO_DEFAULT_GPG1_BIN);
		if (!(*gpgBin))
		{
			ELEKTRA_SET_ERROR (87, errorKey, "Memory allocation failed");
			return -1;
		}
		return 1;
	}

	// no GPG for us :-(
	ELEKTRA_SET_ERROR (ELEKTRA_ERROR_CRYPTO_GPG, errorKey, "no gpg binary found. Please make sure GnuPG is installed and executable.");
	return -1;
}

/**
 * @brief call the gpg binary to encrypt the random master password.
 *
 * @param conf holds the backend/plugin configuration
 * @param errorKey holds the error description in case of failure
 * @param msgKey holds the master password to be encrypted
 *
 * @retval 1 on success
 * @retval -1 on failure
 */
int CRYPTO_PLUGIN_FUNCTION (gpgEncryptMasterPassword) (KeySet * conf, Key * errorKey, Key * msgKey)
{
	// [0]: <path to binary>, [argc-3]: --batch, [argc-2]: -e, [argc-1]: NULL-terminator
	static const kdb_unsigned_short_t staticArgumentsCount = 4;
	Key * k;

	// determine the number of total GPG keys to be used
	kdb_unsigned_short_t recipientCount = 0;
	kdb_unsigned_short_t testMode = 0;
	Key * root = ksLookupByName (conf, ELEKTRA_CRYPTO_PARAM_GPG_KEY, 0);

	// check root key crypto/key
	if (root && strlen (keyString (root)) > 0)
	{
		recipientCount++;
	}

	// check for key beneath crypto/key (like crypto/key/#0 etc)
	ksRewind (conf);
	while ((k = ksNext (conf)) != 0)
	{
		if (keyIsBelow (k, root))
		{
			recipientCount++;
		}
	}

	if (recipientCount == 0)
	{
		ELEKTRA_SET_ERRORF (ELEKTRA_ERROR_CRYPTO_CONFIG_FAULT, errorKey,
				    "Missing GPG key (specified as %s) in plugin configuration.", ELEKTRA_CRYPTO_PARAM_GPG_KEY);
		return -1;
	}

	if (inTestMode (conf))
	{
		// add two parameters for unit testing
		testMode = 2;
	}

	// initialize argument vector for gpg call
	const kdb_unsigned_short_t argc = (2 * recipientCount) + staticArgumentsCount + testMode;
	kdb_unsigned_short_t i = 1;
	char * argv[argc];

	// append root (crypto/key) as gpg recipient
	if (root && strlen (keyString (root)) > 0)
	{
		argv[i] = "-r";
		// NOTE argv[] values will not be modified, so const can be discarded safely
		argv[i + 1] = (char *)keyString (root);
		i = i + 2;
	}

	// append keys beneath root (crypto/key/#_) as gpg recipients
	ksRewind (conf);
	while ((k = ksNext (conf)) != 0)
	{
		if (keyIsBelow (k, root))
		{
			argv[i] = "-r";
			// NOTE argv[] values will not be modified, so const can be discarded safely
			argv[i + 1] = (char *)keyString (k);
			i = i + 2;
		}
	}

	// append option for unit tests
	if (testMode)
	{
		argv[argc - 5] = "--trust-model";
		argv[argc - 4] = "always";
	}

	argv[argc - 3] = "--batch";
	argv[argc - 2] = "-e";

	// call gpg
	return CRYPTO_PLUGIN_FUNCTION (gpgCall) (conf, errorKey, msgKey, argv, argc);
}

/**
 * @brief call the gpg binary to decrypt the random master password.
 *
 * @param conf holds the backend/plugin configuration
 * @param errorKey holds the error description in case of failure
 * @param msgKey holds the master password to be decrypted. Note that the content of this key will be modified.
 *
 * @retval 1 on success
 * @retval -1 on failure
 */
int CRYPTO_PLUGIN_FUNCTION (gpgDecryptMasterPassword) (KeySet * conf, Key * errorKey, Key * msgKey)
{
	if (inTestMode (conf))
	{
		char * argv[] = { "", "--batch", "--trust-model", "always", "-d", NULL };
		return CRYPTO_PLUGIN_FUNCTION (gpgCall) (conf, errorKey, msgKey, argv, 6);
	}
	else
	{
		char * argv[] = { "", "--batch", "-d", NULL };
		return CRYPTO_PLUGIN_FUNCTION (gpgCall) (conf, errorKey, msgKey, argv, 4);
	}
}

/**
 * @brief call the gpg binary to perform the requested operation.
 *
 * @param conf holds the backend/plugin configuration
 * @param errorKey holds the error description in case of failure
 * @param msgKey holds the message to be transformed. Ignored if set to NULL, i.e. you do not want to send a stdin message to gpg.
 * @param argv array holds the arguments passed on to the gpg process
 * @param argc contains the number of elements in argv, i.e. the size of argv
 *
 * @retval 1 on success
 * @retval -1 on failure
 */
int CRYPTO_PLUGIN_FUNCTION (gpgCall) (KeySet * conf, Key * errorKey, Key * msgKey, char * argv[], size_t argc)
{
	pid_t pid;
	int status;
	int pipe_stdin[2];
	int pipe_stdout[2];
	int pipe_stderr[2];
	char errorBuffer[512] = "";
	kdb_octet_t * buffer = NULL;
	const ssize_t bufferSize = 2 * keyGetValueSize (msgKey);
	ssize_t outputLen;

	assert (argc > 1);

	// sanitize the argument vector
	if (getGpgBinary (&argv[0], conf, errorKey) != 1)
	{
		return -1;
	}
	argv[argc - 1] = NULL;

	// check that the gpg binary exists and that it is executable
	if (isExecutable (argv[0], errorKey) != 1)
	{
		elektraFree (argv[0]);
		return -1; // error set by isExecutable()
	}

	// initialize pipes
	if (pipe (pipe_stdin))
	{
		ELEKTRA_SET_ERROR (ELEKTRA_ERROR_CRYPTO_GPG, errorKey, "Pipe initialization failed");
		elektraFree (argv[0]);
		return -1;
	}

	if (pipe (pipe_stdout))
	{
		ELEKTRA_SET_ERROR (ELEKTRA_ERROR_CRYPTO_GPG, errorKey, "Pipe initialization failed");
		closePipe (pipe_stdin);
		elektraFree (argv[0]);
		return -1;
	}

	if (pipe (pipe_stderr))
	{
		ELEKTRA_SET_ERROR (ELEKTRA_ERROR_CRYPTO_GPG, errorKey, "Pipe initialization failed");
		closePipe (pipe_stdin);
		closePipe (pipe_stdout);
		elektraFree (argv[0]);
		return -1;
	}

	// allocate buffer for gpg output
	// estimated maximum output size = 2 * input (including headers, etc.)
	if (msgKey && !(buffer = elektraMalloc (bufferSize)))
	{
		ELEKTRA_SET_ERROR (87, errorKey, "Memory allocation failed");
		closePipe (pipe_stdin);
		closePipe (pipe_stdout);
		closePipe (pipe_stderr);
		elektraFree (argv[0]);
		return -1;
	}

	// fork into the gpg binary
	switch (pid = fork ())
	{
	case -1:
		// fork() failed
		ELEKTRA_SET_ERROR (ELEKTRA_ERROR_CRYPTO_GPG, errorKey, "fork failed");
		closePipe (pipe_stdin);
		closePipe (pipe_stdout);
		closePipe (pipe_stderr);
		elektraFree (buffer);
		elektraFree (argv[0]);
		return -1;

	case 0:
		// start of the forked child process
		close (pipe_stdin[1]);
		close (pipe_stdout[0]);
		close (pipe_stderr[0]);

		// redirect stdin to pipe
		if (msgKey)
		{
			close (STDIN_FILENO);
			dup (pipe_stdin[0]);
		}
		close (pipe_stdin[0]);

		// redirect stdout to pipe
		close (STDOUT_FILENO);
		dup (pipe_stdout[1]);
		close (pipe_stdout[1]);

		// redirect stderr to pipe
		close (STDERR_FILENO);
		dup (pipe_stderr[1]);
		close (pipe_stderr[1]);

		// finally call the gpg executable
		if (execv (argv[0], argv) < 0)
		{
			ELEKTRA_SET_ERRORF (ELEKTRA_ERROR_CRYPTO_GPG, errorKey, "failed to start the gpg binary: %s", argv[0]);
			return -1;
		}
		// end of the child process
	}

	// parent process
	close (pipe_stdin[0]);
	close (pipe_stdout[1]);
	close (pipe_stderr[1]);

	// pass the message to the gpg process
	if (msgKey)
	{
		write (pipe_stdin[1], keyValue (msgKey), keyGetValueSize (msgKey));
	}
	close (pipe_stdin[1]);

	// wait for the gpg process to finish
	waitpid (pid, &status, 0);

	// evaluate return code of finished child process
	int retval = -1;
	switch (status)
	{
	case 0:
		// everything ok - receive the output of the gpg process
		if (msgKey)
		{
			outputLen = read (pipe_stdout[0], buffer, bufferSize);
			keySetBinary (msgKey, buffer, outputLen);
		}
		retval = 1;
		break;

	case 1:
		// bad signature
		ELEKTRA_SET_ERROR (ELEKTRA_ERROR_CRYPTO_GPG, errorKey, "GPG reported a bad signature");
		break;

	default:
		// other errors
		outputLen = read (pipe_stderr[0], errorBuffer, sizeof (errorBuffer));
		if (outputLen < 1)
		{
			errorBuffer[0] = '\0';
		}
		ELEKTRA_SET_ERRORF (ELEKTRA_ERROR_CRYPTO_GPG, errorKey, "GPG failed with return value %d. %s", status, errorBuffer);
		break;
	}

	elektraFree (buffer);
	elektraFree (argv[0]);
	close (pipe_stdout[0]);
	close (pipe_stderr[0]);
	return retval;
}
