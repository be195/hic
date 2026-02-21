#include "watchdog.hpp"

#include <cstring>
#include <string>
#include <vector>

#include "utils/util.hpp"

#ifdef _WIN32
#include <windows.h>
#include <process.h>

#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#endif

int spawnProcess(const char* exe, const std::vector<std::string>& args) {
#ifdef _WIN32
  std::string cmdLine = "\"" + std::string(exe) + "\"";
  for (const auto& arg : args)
    cmdLine += " \"" + arg + "\"";

  STARTUPINFOA startupInfo;
  PROCESS_INFORMATION procInfo;
  memset(&startupInfo, 0, sizeof(startupInfo));
  startupInfo.cb = sizeof(startupInfo);
  memset(&procInfo, 0, sizeof(procInfo));

  std::vector buffer(cmdLine.begin(), cmdLine.end());
  buffer.push_back('\0');

  if (!CreateProcessA(nullptr, buffer.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startupInfo, &procInfo))
    return -1;

  WaitForSingleObject(procInfo.hProcess, INFINITE);

  DWORD exitCode;
  GetExitCodeProcess(procInfo.hProcess, &exitCode);

  CloseHandle(procInfo.hProcess);
  CloseHandle(procInfo.hThread);

  return exitCode;

#else
  pid_t pid = fork();

  if (pid < 0)
    return -1;

  if (pid == 0) {
    // child
    std::vector<char*> argvExec;
    argvExec.push_back(const_cast<char*>(exe));
    for (const auto& arg : args)
      argvExec.push_back(const_cast<char*>(arg.c_str()));
    argvExec.push_back(nullptr);

    execv(exe, argvExec.data());

    _exit(127);
  }

  // parent
  int status;
  waitpid(pid, &status, 0);

  if (WIFEXITED(status))
    return WEXITSTATUS(status);
  else if (WIFSIGNALED(status))
    return 128 + WTERMSIG(status);

  return -1;
#endif
}

#define CHILD_ARG "--hic-child"

namespace hic {

bool watchdog(const int argc, char **argv) {
  bool isChild = false;

  for (int i = 0; i < argc; i++)
    if (strcmp(argv[i], CHILD_ARG) == 0)
      isChild = true;

  if (isChild)
    return false;

  std::vector<std::string> args;
  for (int i = 1; i < argc; i++)
    if (strcmp(argv[i], CHILD_ARG) != 0)
      args.push_back(argv[i]);
  args.push_back(CHILD_ARG);

  if (const int exitCode = spawnProcess(argv[0], args); exitCode != 0)
    panic("child process most likely crashed");

  return true;
}

}