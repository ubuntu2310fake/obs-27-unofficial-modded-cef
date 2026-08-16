/*
 * Copyright (c) 2014 Hugh Bailey <obs.jim@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "platform.h"
#include "bmem.h"
#include "pipe.h"

struct os_process_pipe {
	bool read_pipe;
	HANDLE handle;
	HANDLE handle_err;
	HANDLE process;
};

static bool create_pipe(HANDLE *input, HANDLE *output)
{
	SECURITY_ATTRIBUTES sa = {0};

	sa.nLength = sizeof(sa);
	sa.bInheritHandle = true;

	if (!CreatePipe(input, output, &sa, 0)) {
		return false;
	}

	return true;
}

/* Creates a process. On Windows 8.1 STARTF_USESTDHANDLES is silently
 * stripped for GUI subprocesses, so we duplicate the stdin handle into the
 * child's process space and append "--pipe-handle <value>" to cmd_line.
 * The child reads this argument and uses it directly. */
static inline bool create_process(const char *cmd_line, HANDLE stdin_handle,
				  HANDLE stdout_handle, HANDLE stderr_handle,
				  HANDLE *process, HANDLE *out_dup_stdin)
{
	PROCESS_INFORMATION pi = {0};
	wchar_t *cmd_line_w = NULL;
	STARTUPINFOW si = {0};
	bool success = false;

	si.cb = sizeof(si);

	DWORD flags = CREATE_SUSPENDED;
#ifndef SHOW_SUBPROCESSES
	flags |= CREATE_NO_WINDOW;
#endif

	os_utf8_to_wcs_ptr(cmd_line, 0, &cmd_line_w);
	if (cmd_line_w) {
		success = !!CreateProcessW(NULL, cmd_line_w, NULL, NULL, FALSE,
					   flags, NULL, NULL, &si, &pi);
		bfree(cmd_line_w);
	}

	if (!success)
		return false;

	/* Duplicate stdin_handle into the child process so handle value is
	 * valid in child's address space even when inheritance is disabled. */
	HANDLE dup = NULL;
	if (stdin_handle) {
		DuplicateHandle(GetCurrentProcess(), stdin_handle,
				pi.hProcess, &dup,
				0, FALSE, DUPLICATE_SAME_ACCESS);
	}

	/* Also duplicate stderr into child */
	HANDLE dup_err = NULL;
	if (stderr_handle) {
		DuplicateHandle(GetCurrentProcess(), stderr_handle,
				pi.hProcess, &dup_err,
				0, FALSE, DUPLICATE_SAME_ACCESS);
	}

	/* Append --pipe-handle and --pipe-handle-err to child's env block is
	 * not feasible; instead we restart child with augmented command line. */

	/* Build new command line with handle arguments.
	 * We need to terminate the suspended process and restart with args. */
	TerminateProcess(pi.hProcess, 0);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	/* Close duplicated handles that are now orphaned */
	if (dup) CloseHandle(dup);
	if (dup_err) CloseHandle(dup_err);

	/* Duplicate stdin into OUR process (inheritable) so child can
	 * inherit it the normal way — but we ALSO pass it as --pipe-handle */
	HANDLE inh_stdin = NULL;
	if (stdin_handle) {
		DuplicateHandle(GetCurrentProcess(), stdin_handle,
				GetCurrentProcess(), &inh_stdin,
				0, TRUE, DUPLICATE_SAME_ACCESS);
	}
	HANDLE inh_stderr = NULL;
	if (stderr_handle) {
		DuplicateHandle(GetCurrentProcess(), stderr_handle,
				GetCurrentProcess(), &inh_stderr,
				0, TRUE, DUPLICATE_SAME_ACCESS);
	}

	/* Build augmented command line with --pipe-handle <handle_value>
	 * and --pipe-handle-err <stderr_handle_value> */
	char new_cmd[4096];
	if (inh_stdin) {
		snprintf(new_cmd, sizeof(new_cmd), "%s --pipe-handle %llu --pipe-handle-err %llu",
			 cmd_line,
			 (unsigned long long)(uintptr_t)inh_stdin,
			 (unsigned long long)(uintptr_t)inh_stderr);
	} else {
		snprintf(new_cmd, sizeof(new_cmd), "%s", cmd_line);
	}

	wchar_t *new_cmd_w = NULL;
	os_utf8_to_wcs_ptr(new_cmd, 0, &new_cmd_w);

	/* Setup STARTUPINFO with inheritable handles via STARTF_USESTDHANDLES.
	 * We provide NUL for stdout. This may or may not work on Win8.1, but
	 * the child will also have --pipe-handle as a fallback. */
	HANDLE dev_null = CreateFileW(L"NUL", GENERIC_WRITE,
					FILE_SHARE_READ | FILE_SHARE_WRITE,
					NULL, OPEN_EXISTING, 0, NULL);
	HANDLE inh_null = INVALID_HANDLE_VALUE;
	if (dev_null != INVALID_HANDLE_VALUE) {
		DuplicateHandle(GetCurrentProcess(), dev_null,
				GetCurrentProcess(), &inh_null,
				0, TRUE, DUPLICATE_SAME_ACCESS);
		CloseHandle(dev_null);
	}

	STARTUPINFOW si2 = {0};
	si2.cb = sizeof(si2);
	si2.dwFlags = STARTF_USESTDHANDLES | STARTF_FORCEOFFFEEDBACK;
	si2.hStdInput  = inh_stdin  ? inh_stdin  : NULL;
	si2.hStdOutput = (inh_null != INVALID_HANDLE_VALUE) ? inh_null : NULL;
	si2.hStdError  = inh_stderr ? inh_stderr : NULL;

	PROCESS_INFORMATION pi2 = {0};
	success = false;
	if (new_cmd_w) {
		success = !!CreateProcessW(NULL, new_cmd_w, NULL, NULL, TRUE,
					   (flags & ~CREATE_SUSPENDED),
					   NULL, NULL, &si2, &pi2);
		bfree(new_cmd_w);
	}

	if (inh_null != INVALID_HANDLE_VALUE) CloseHandle(inh_null);
	if (inh_stdin) CloseHandle(inh_stdin);
	if (inh_stderr) CloseHandle(inh_stderr);

	if (success) {
		*process = pi2.hProcess;
		CloseHandle(pi2.hThread);
	}

	return success;
}

os_process_pipe_t *os_process_pipe_create(const char *cmd_line,
					  const char *type)
{
	os_process_pipe_t *pp = NULL;
	bool read_pipe;
	HANDLE process;
	HANDLE output;
	HANDLE err_input, err_output;
	HANDLE input;
	bool success;

	if (!cmd_line || !type) {
		return NULL;
	}
	if (*type != 'r' && *type != 'w') {
		return NULL;
	}
	if (!create_pipe(&input, &output)) {
		return NULL;
	}

	if (!create_pipe(&err_input, &err_output)) {
		return NULL;
	}

	read_pipe = *type == 'r';

	success = !!SetHandleInformation(read_pipe ? input : output,
					 HANDLE_FLAG_INHERIT, false);
	if (!success) {
		goto error;
	}

	success = !!SetHandleInformation(err_input, HANDLE_FLAG_INHERIT, false);
	if (!success) {
		goto error;
	}

	success = create_process(cmd_line, read_pipe ? NULL : input,
				 read_pipe ? output : NULL, err_output,
				 &process, NULL);
	if (!success) {
		goto error;
	}

	pp = bmalloc(sizeof(*pp));

	pp->handle = read_pipe ? input : output;
	pp->read_pipe = read_pipe;
	pp->process = process;
	pp->handle_err = err_input;

	CloseHandle(read_pipe ? output : input);
	CloseHandle(err_output);
	return pp;

error:
	CloseHandle(output);
	CloseHandle(input);
	return NULL;
}

int os_process_pipe_destroy(os_process_pipe_t *pp)
{
	int ret = 0;

	if (pp) {
		DWORD code;

		CloseHandle(pp->handle);
		CloseHandle(pp->handle_err);

		WaitForSingleObject(pp->process, INFINITE);
		if (GetExitCodeProcess(pp->process, &code))
			ret = (int)code;

		CloseHandle(pp->process);
		bfree(pp);
	}

	return ret;
}

size_t os_process_pipe_read(os_process_pipe_t *pp, uint8_t *data, size_t len)
{
	DWORD bytes_read;
	bool success;

	if (!pp) {
		return 0;
	}
	if (!pp->read_pipe) {
		return 0;
	}

	success = !!ReadFile(pp->handle, data, (DWORD)len, &bytes_read, NULL);
	if (success && bytes_read) {
		return bytes_read;
	}

	return 0;
}

size_t os_process_pipe_read_err(os_process_pipe_t *pp, uint8_t *data,
				size_t len)
{
	DWORD bytes_read;
	bool success;

	if (!pp || !pp->handle_err) {
		return 0;
	}

	success =
		!!ReadFile(pp->handle_err, data, (DWORD)len, &bytes_read, NULL);
	if (success && bytes_read) {
		return bytes_read;
	} else
		bytes_read = GetLastError();

	return 0;
}

size_t os_process_pipe_write(os_process_pipe_t *pp, const uint8_t *data,
			     size_t len)
{
	DWORD bytes_written;
	bool success;

	if (!pp) {
		return 0;
	}
	if (pp->read_pipe) {
		return 0;
	}

	success =
		!!WriteFile(pp->handle, data, (DWORD)len, &bytes_written, NULL);
	if (success && bytes_written) {
		return bytes_written;
	}

	return 0;
}
