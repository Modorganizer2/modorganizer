#include "commandline.h"
#include "env.h"
#include "shared/util.h"

namespace cl
{

std::string pad_right(std::string s, std::size_t n, char c=' ')
{
  if (s.size() < n)
    s.append(n - s.size() , c);

  return s;
}

std::string table(
  const std::vector<std::pair<std::string, std::string>>& v,
  std::size_t indent, std::size_t spacing)
{
  std::size_t longest = 0;

  for (auto&& p : v)
    longest = std::max(longest, p.first.size());

  std::string s;

  for (auto&& p : v)
  {
    if (!s.empty())
      s += "\n";

    s +=
      std::string(indent, ' ') +
      pad_right(p.first, longest) + " " +
      std::string(spacing, ' ') +
      p.second;
  }

  return s;

}


CommandLine::CommandLine()
{
  createOptions();
  m_commands.push_back(std::make_unique<ExeCommand>());
  m_commands.push_back(std::make_unique<RunCommand>());
  m_commands.push_back(std::make_unique<CrashDumpCommand>());
  m_commands.push_back(std::make_unique<LaunchCommand>());
}

std::optional<int> CommandLine::run(const std::wstring& line)
{
  try
  {
    auto args = po::split_winmain(line);
    if (!args.empty()) {
      // remove program name
      args.erase(args.begin());
    }

    auto parsed = po::wcommand_line_parser(args)
      .options(m_allOptions)
      .positional(m_positional)
      .allow_unregistered()
      .run();

    po::store(parsed, m_vm);
    po::notify(m_vm);

    auto opts = po::collect_unrecognized(
      parsed.options, po::include_positional);


    if (m_vm.count("command")) {
      const auto commandName = m_vm["command"].as<std::string>();

      for (auto&& c : m_commands) {
        if (c->name() == commandName) {
          // remove the command name itself
          opts.erase(opts.begin());

          try
          {
            po::wcommand_line_parser parser(opts);

            auto co = c->allOptions();
            parser.options(co);

            if (c->allow_unregistered()) {
              parser.allow_unregistered();
            }

            auto pos = c->positional();
            parser.positional(pos);

            parsed = parser.run();

            po::store(parsed, m_vm);
            po::notify(m_vm);

            if (m_vm.count("help")) {
              env::Console console;
              std::cout << usage(c.get()) << "\n";
              return 0;
            }

            return c->run(line, m_vm, opts);
          }
          catch(po::error& e)
          {
            env::Console console;

            std::cerr
              << e.what() << "\n"
              << usage(c.get()) << "\n";

            return 1;
          }
        }
      }
    }

    if (m_vm.count("help")) {
      env::Console console;
      std::cout << usage() << "\n";
      return 0;
    }

    if (!opts.empty()) {
      const auto qs = QString::fromStdWString(opts[0]);
      m_shortcut = qs;

      if (!m_shortcut.isValid()) {
        if (isNxmLink(qs)) {
          m_nxmLink = qs;
        } else {
          m_executable = qs;
        }
      }

      // remove the shortcut/nxm/executable
      opts.erase(opts.begin());

      for (auto&& o : opts) {
        m_untouched.push_back(QString::fromStdWString(o));
      }
    }

    return {};
  }
  catch(po::error& e)
  {
    env::Console console;

    std::cerr
      << e.what() << "\n"
      << usage() << "\n";

    return 1;
  }
}

void CommandLine::clear()
{
  m_vm.clear();
  m_shortcut = {};
  m_nxmLink = {};
}

void CommandLine::createOptions()
{
  m_visibleOptions.add_options()
    ("help",      "show this message")
    ("multiple",  "allow multiple MO processes to run; see below")
    ("instance,i", po::value<std::string>(), "use the given instance (defaults to last used)")
    ("profile,p", po::value<std::string>(), "use the given profile (defaults to last used)");

  po::options_description options;
  options.add_options()
    ("command", po::value<std::string>(), "command")
    ("subargs", po::value<std::vector<std::string> >(), "args");

  m_positional
    .add("command", 1)
    .add("subargs", -1);

  m_allOptions.add(m_visibleOptions);
  m_allOptions.add(options);
}

std::string CommandLine::usage(const Command* c) const
{
  std::ostringstream oss;

  oss
    << "\n"
    << "Usage:\n";

  if (c) {
    oss
      << "  ModOrganizer.exe [global-options] " << c->usageLine() << "\n"
      << "\n"
      << "Command options:\n"
      << c->visibleOptions() << "\n";
  } else {
    oss
      << "  ModOrganizer.exe [options] [[command] [command-options]]\n"
      << "\n"
      << "Commands:\n";

    std::vector<std::pair<std::string, std::string>> v;
    for (auto&& c : m_commands) {
      v.push_back({c->name(), c->description()});
    }

    oss
      << table(v, 2, 4) << "\n"
      << "\n";
  }

  oss
    << "Global options:\n"
    << m_visibleOptions << "\n";

  if (!c) {
    oss << "\n" << more() << "\n";
  }

  return oss.str();
}

bool CommandLine::multiple() const
{
  return (m_vm.count("multiple") > 0);
}

std::optional<QString> CommandLine::profile() const
{
  if (m_vm.count("profile")) {
    return QString::fromStdString(m_vm["profile"].as<std::string>());
  }

  return {};
}

std::optional<QString> CommandLine::instance() const
{
  if (m_shortcut.isValid() && m_shortcut.hasInstance()) {
    return m_shortcut.instance();
  } else if (m_vm.count("instance")) {
    return QString::fromStdString(m_vm["instance"].as<std::string>());
  }

  return {};
}

const MOShortcut& CommandLine::shortcut() const
{
  return m_shortcut;
}

std::optional<QString> CommandLine::nxmLink() const
{
  return m_nxmLink;
}

std::optional<QString> CommandLine::executable() const
{
  return m_executable;
}

const QStringList& CommandLine::untouched() const
{
  return m_untouched;
}

std::string CommandLine::more() const
{
  return
    "Multiple processes\n"
    "  A note on terminology: 'instance' can either mean an MO process\n"
    "  that's running on the system, or a set of mods and profiles managed\n"
    "  by MO. To avoid confusion, the term 'process' is used below for the\n"
    "  former.\n"
    "  \n"
    "  --multiple can be used to allow multiple MO processes to run\n"
    "  simultaneously. This is unsupported and can create all sorts of weird\n"
    "  problems. To minimize these:\n"
    "  \n"
    "    1) Never have multiple MO processes running that manage the same\n"
    "       game instance.\n"
    "    2) If an executable is launched from an MO process, only this\n"
    "       process may launch executables until all processes are \n"
    "       terminated.\n"
    "  \n"
    "  It is recommended to close _all_ MO processes as soon as multiple\n"
    "  processes become unnecessary.";
}


std::string Command::name() const
{
  return meta().name;
}

std::string Command::description() const
{
  return meta().description;
}

std::string Command::usageLine() const
{
  return name() + " " + getUsageLine();
}

bool Command::allow_unregistered() const
{
  return false;
}

po::options_description Command::allOptions() const
{
  po::options_description d;

  d.add(visibleOptions());
  d.add(getInternalOptions());

  return d;
}

po::options_description Command::visibleOptions() const
{
  po::options_description d(getVisibleOptions());

  d.add_options()
    ("help", "shows this message");

  return d;
}

po::positional_options_description Command::positional() const
{
  return getPositional();
}

std::string Command::getUsageLine() const
{
  return "[options]";
}

po::options_description Command::getVisibleOptions() const
{
  // no-op
  return {};
}

po::options_description Command::getInternalOptions() const
{
  // no-op
  return {};
}

po::positional_options_description Command::getPositional() const
{
  // no-op
  return {};
}


std::string Command::usage() const
{
  std::ostringstream oss;

  oss
    << "\n"
    << "Usage:\n"
    << "    ModOrganizer.exe [options] [[command] [command-options]]\n"
    << "\n"
    << "Options:\n"
    << visibleOptions() << "\n";

  return oss.str();
}

std::optional<int> Command::run(
  const std::wstring& originalLine,
  po::variables_map vm,
  std::vector<std::wstring> untouched)
{
  m_original = originalLine;
  m_vm = vm;
  m_untouched = untouched;

  return doRun();
}

const std::wstring& Command::originalCmd() const
{
  return m_original;
}

const po::variables_map& Command::vm() const
{
  return m_vm;
}

const std::vector<std::wstring>& Command::untouched() const
{
  return m_untouched;
}


po::options_description CrashDumpCommand::getVisibleOptions() const
{
  po::options_description d;

  d.add_options()
    ("type", po::value<std::string>()->default_value("mini"), "mini|data|full");

  return d;
}

Command::Meta CrashDumpCommand::meta() const
{
  return {"crashdump", "writes a crashdump for a running process of MO"};
}

std::optional<int> CrashDumpCommand::doRun()
{
  env::Console console;

  const auto typeString = vm()["type"].as<std::string>();
  const auto type = env::coreDumpTypeFromString(typeString);

  // dump
  const auto b = env::coredumpOther(type);
  if (!b) {
    std::wcerr << L"\n>>>> a minidump file was not written\n\n";
  }

  std::wcerr << L"Press enter to continue...";
  std::wcin.get();

  return (b ? 0 : 1);
}


bool LaunchCommand::allow_unregistered() const
{
  return true;
}

Command::Meta LaunchCommand::meta() const
{
  return {"launch", "(internal, do not use)"};
}

std::optional<int> LaunchCommand::doRun()
{
  // needs at least the working directory and process name
  if (untouched().size() < 2) {
    return 1;
  }

  std::vector<std::wstring> arg;
  auto args = UntouchedCommandLineArguments(2, arg);

  return SpawnWaitProcess(arg[1].c_str(), args);
}

int LaunchCommand::SpawnWaitProcess(LPCWSTR workingDirectory, LPCWSTR commandLine)
{
  PROCESS_INFORMATION pi{ 0 };
  STARTUPINFO si{ 0 };
  si.cb = sizeof(si);
  std::wstring commandLineCopy = commandLine;

  if (!CreateProcessW(NULL, &commandLineCopy[0], NULL, NULL, FALSE, 0, NULL, workingDirectory, &si, &pi)) {
    // A bit of a problem where to log the error message here, at least this way you can get the message
    // using a either DebugView or a live debugger:
    std::wostringstream ost;
    ost << L"CreateProcess failed: " << commandLine << ", " << GetLastError();
    OutputDebugStringW(ost.str().c_str());
    return -1;
  }

  WaitForSingleObject(pi.hProcess, INFINITE);

  DWORD exitCode = (DWORD)-1;
  ::GetExitCodeProcess(pi.hProcess, &exitCode);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return static_cast<int>(exitCode);
}

// Parses the first parseArgCount arguments of the current process command line and returns
// them in parsedArgs, the rest of the command line is returned untouched.
LPCWSTR LaunchCommand::UntouchedCommandLineArguments(
  int parseArgCount, std::vector<std::wstring>& parsedArgs)
{
  LPCWSTR cmd = GetCommandLineW();
  LPCWSTR arg = nullptr; // to skip executable name
  for (; parseArgCount >= 0 && *cmd; ++cmd)
  {
    if (*cmd == '"') {
      int escaped = 0;
      for (++cmd; *cmd && (*cmd != '"' || escaped % 2 != 0); ++cmd)
        escaped = *cmd == '\\' ? escaped + 1 : 0;
    }
    if (*cmd == ' ') {
      if (arg)
        if (cmd-1 > arg && *arg == '"' && *(cmd-1) == '"')
          parsedArgs.push_back(std::wstring(arg+1, cmd-1));
        else
          parsedArgs.push_back(std::wstring(arg, cmd));
      arg = cmd + 1;
      --parseArgCount;
    }
  }
  return cmd;
}


std::string ExeCommand::getUsageLine() const
{
  return "[options] exe-name";
}

po::options_description ExeCommand::getVisibleOptions() const
{
  po::options_description d;

  d.add_options()
    ("arguments,a", po::value<std::string>()->default_value(""), "override arguments")
    ("cwd,c",       po::value<std::string>()->default_value(""), "override working directory");

  return d;
}

po::options_description ExeCommand::getInternalOptions() const
{
  po::options_description d;

  d.add_options()
    ("exe-name", po::value<std::string>()->required(), "executable name");

  return d;
}

po::positional_options_description ExeCommand::getPositional() const
{
  po::positional_options_description d;

  d.add("exe-name", 1);

  return d;
}

Command::Meta ExeCommand::meta() const
{
  return {"exe", "launches a configured executable"};
}

std::optional<int> ExeCommand::doRun()
{
  const auto exe = vm()["exe-name"].as<std::string>();
  const auto args = vm()["arguments"].as<std::string>();
  const auto cwd = vm()["cwd"].as<std::string>();



  return 0;
}


po::options_description RunCommand::getOptions() const
{
  return {};
}

Command::Meta RunCommand::meta() const
{
  return {"run", "launches an arbitrary program"};
}

std::optional<int> RunCommand::doRun()
{
  return {};
}

} // namespace