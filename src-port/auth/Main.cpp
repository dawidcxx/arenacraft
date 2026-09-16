/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file main.cpp
 * @brief Authentication Server main program
 *
 * This file contains the main program for the
 * authentication server
 */

#include "AppenderDB.h"
#include "AuthSocketMgr.h"
#include "Banner.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DatabaseLoader.h"
#include "DeadlineTimer.h"
#include "GitRevision.h"
#include "IPLocation.h"
#include "IoContext.h"
#include "Log.h"
#include "MySQLThreading.h"
#include "OpenSSLCrypto.h"
#include "ProcessPriority.h"
#include "RealmList.h"
#include "SecretMgr.h"
#include "SharedDefines.h"
#include "Util.h"
#include <argparse/argparse.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/version.hpp>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <openssl/crypto.h>
#include <openssl/opensslv.h>

#ifndef _ACORE_REALM_CONFIG
#define _ACORE_REALM_CONFIG "authserver.conf"
#endif

using boost::asio::ip::tcp;
namespace fs = std::filesystem;

bool StartDB();
void StopDB();
void SignalHandler(std::weak_ptr<Acore::Asio::IoContext> ioContextRef, boost::system::error_code const& error,
                   int signalNumber);
void KeepDatabaseAliveHandler(std::weak_ptr<Acore::Asio::DeadlineTimer> dbPingTimerRef, int32 dbPingInterval,
                              boost::system::error_code const& error);
void BanExpiryHandler(std::weak_ptr<Acore::Asio::DeadlineTimer> banExpiryCheckTimerRef, int32 banExpiryCheckInterval,
                      boost::system::error_code const& error);

struct ConsoleArguments
{
    bool help = false;
    bool version = false;
};

ConsoleArguments GetConsoleArguments(int argc, char** argv, fs::path& configFile);

/// Launch the auth server
int authserver_main(int argc, char** argv)
{
  Acore::Impl::CurrentServerProcessHolder::_type = SERVER_PROCESS_AUTHSERVER;
  signal(SIGABRT, &Acore::AbortHandler);

  // Command line parsing
  auto configFile = fs::path(sConfigMgr->GetConfigPath() + std::string(_ACORE_REALM_CONFIG));
  auto args       = GetConsoleArguments(argc, argv, configFile);

  // exit if help or version is enabled
  if (args.help)
    return 0;
  if (args.version)
  {
    std::cout << GitRevision::GetFullVersion() << std::endl;
    return 0;
  }

  // Add file and args in config
  sConfigMgr->Configure(configFile.generic_string(), std::vector<std::string>(argv, argv + argc));

  if (!sConfigMgr->LoadAppConfigs())
    return 1;

  // Init logging
  sLog->RegisterAppender<AppenderDB>();
  sLog->Initialize(nullptr);

  Acore::Banner::Show(
      "authserver", [](std::string_view text) { LOG_INFO("server.authserver", text); },
      []()
      {
        LOG_INFO("server.authserver", "> Using configuration file       {}", sConfigMgr->GetFilename());
        LOG_INFO("server.authserver", "> Using SSL version:             {} (library: {})", OPENSSL_VERSION_TEXT,
                 OpenSSL_version(OPENSSL_VERSION));
        LOG_INFO("server.authserver", "> Using Boost version:           {}.{}.{}", BOOST_VERSION / 100000,
                 BOOST_VERSION / 100 % 1000, BOOST_VERSION % 100);
      });

  OpenSSLCrypto::threadsSetup();

  std::shared_ptr<void> opensslHandle(nullptr, [](void*) { OpenSSLCrypto::threadsCleanup(); });

  // authserver PID file creation
  std::string pidFile = sConfigMgr->GetOption<std::string>("PidFile", "");
  if (!pidFile.empty())
  {
    if (uint32 pid = CreatePIDFile(pidFile))
      LOG_INFO("server.authserver", "Daemon PID: {}\n", pid); // outError for red color in console
    else
    {
      LOG_ERROR("server.authserver", "Cannot create PID file {} (possible error: permission)\n", pidFile);
      return 1;
    }
  }

  // Initialize the database connection
  if (!StartDB())
    return 1;

  sSecretMgr->Initialize();

  // Load IP Location Database
  sIPLocation->Load();

  std::shared_ptr<void> dbHandle(nullptr, [](void*) { StopDB(); });

  std::shared_ptr<Acore::Asio::IoContext> ioContext = std::make_shared<Acore::Asio::IoContext>();

  // Get the list of realms for the server
  sRealmList->Initialize(*ioContext, sConfigMgr->GetOption<int32>("RealmsStateUpdateDelay", 20));

  std::shared_ptr<void> sRealmListHandle(nullptr, [](void*) { sRealmList->Close(); });

  if (sRealmList->GetRealms().empty())
  {
    LOG_ERROR("server.authserver", "No valid realms specified.");
    return 1;
  }

  // Stop auth server if dry run
  if (sConfigMgr->isDryRun())
  {
    LOG_INFO("server.authserver", "Dry run completed, terminating.");
    return 0;
  }

  // Start the listening port (acceptor) for auth connections
  int32 port = sConfigMgr->GetOption<int32>("RealmServerPort", 3724);
  if (port < 0 || port > 0xFFFF)
  {
    LOG_ERROR("server.authserver", "Specified port out of allowed range (1-65535)");
    return 1;
  }

  std::string bindIp = sConfigMgr->GetOption<std::string>("BindIP", "0.0.0.0");

  if (!sAuthSocketMgr.StartNetwork(*ioContext, bindIp, port))
  {
    LOG_ERROR("server.authserver", "Failed to initialize network");
    return 1;
  }

  std::shared_ptr<void> sAuthSocketMgrHandle(nullptr, [](void*) { sAuthSocketMgr.StopNetwork(); });

  // Set signal handlers
  boost::asio::signal_set signals(*ioContext, SIGINT, SIGTERM);
#if AC_PLATFORM == AC_PLATFORM_WINDOWS
  signals.add(SIGBREAK);
#endif
  signals.async_wait(std::bind(&SignalHandler, std::weak_ptr<Acore::Asio::IoContext>(ioContext), std::placeholders::_1,
                               std::placeholders::_2));

  // Set process priority according to configuration settings
  SetProcessPriority("server.authserver", sConfigMgr->GetOption<int32>(CONFIG_PROCESSOR_AFFINITY, 0),
                     sConfigMgr->GetOption<bool>(CONFIG_HIGH_PRIORITY, false));

  // Enabled a timed callback for handling the database keep alive ping
  int32                                       dbPingInterval = sConfigMgr->GetOption<int32>("MaxPingTime", 30);
  std::shared_ptr<Acore::Asio::DeadlineTimer> dbPingTimer    = std::make_shared<Acore::Asio::DeadlineTimer>(*ioContext);
  dbPingTimer->expires_from_now(boost::posix_time::minutes(dbPingInterval));
  dbPingTimer->async_wait(std::bind(&KeepDatabaseAliveHandler, std::weak_ptr<Acore::Asio::DeadlineTimer>(dbPingTimer),
                                    dbPingInterval, std::placeholders::_1));

  int32 banExpiryCheckInterval = sConfigMgr->GetOption<int32>("BanExpiryCheckInterval", 60);
  std::shared_ptr<Acore::Asio::DeadlineTimer> banExpiryCheckTimer =
      std::make_shared<Acore::Asio::DeadlineTimer>(*ioContext);
  banExpiryCheckTimer->expires_from_now(boost::posix_time::seconds(banExpiryCheckInterval));
  banExpiryCheckTimer->async_wait(std::bind(&BanExpiryHandler,
                                            std::weak_ptr<Acore::Asio::DeadlineTimer>(banExpiryCheckTimer),
                                            banExpiryCheckInterval, std::placeholders::_1));

  // Start the io service worker loop
  ioContext->run();

  banExpiryCheckTimer->cancel();
  dbPingTimer->cancel();

  LOG_INFO("server.authserver", "Halting process...");

  signals.cancel();

  return 0;
}

/// Initialize connection to the database
bool StartDB()
{
  MySQL::Library_Init();

  // Load databases
  // NOTE: While authserver is singlethreaded you should keep synch_threads == 1.
  // Increasing it is just silly since only 1 will be used ever.
  DatabaseLoader loader("server.authserver");
  loader.AddDatabase(LoginDatabase, "Login");

  if (!loader.Load())
    return false;

  LOG_INFO("server.authserver", "Started auth database connection pool.");
  sLog->SetRealmId(0); // Enables DB appenders when realm is set.
  return true;
}

/// Close the connection to the database
void StopDB()
{
  LoginDatabase.Close();
  MySQL::Library_End();
}

void SignalHandler(std::weak_ptr<Acore::Asio::IoContext> ioContextRef, boost::system::error_code const& error,
                   int /*signalNumber*/)
{
  if (!error)
  {
    if (std::shared_ptr<Acore::Asio::IoContext> ioContext = ioContextRef.lock())
    {
      ioContext->stop();
    }
  }
}

void KeepDatabaseAliveHandler(std::weak_ptr<Acore::Asio::DeadlineTimer> dbPingTimerRef, int32 dbPingInterval,
                              boost::system::error_code const& error)
{
  if (!error)
  {
    if (std::shared_ptr<Acore::Asio::DeadlineTimer> dbPingTimer = dbPingTimerRef.lock())
    {
      LOG_INFO("server.authserver", "Ping MySQL to keep connection alive");
      LoginDatabase.KeepAlive();

      dbPingTimer->expires_from_now(boost::posix_time::minutes(dbPingInterval));
      dbPingTimer->async_wait(
          std::bind(&KeepDatabaseAliveHandler, dbPingTimerRef, dbPingInterval, std::placeholders::_1));
    }
  }
}

void BanExpiryHandler(std::weak_ptr<Acore::Asio::DeadlineTimer> banExpiryCheckTimerRef, int32 banExpiryCheckInterval,
                      boost::system::error_code const& error)
{
  if (!error)
  {
    if (std::shared_ptr<Acore::Asio::DeadlineTimer> banExpiryCheckTimer = banExpiryCheckTimerRef.lock())
    {
      LoginDatabase.Execute(LoginDatabase.GetPreparedStatement(LOGIN_DEL_EXPIRED_IP_BANS));
      LoginDatabase.Execute(LoginDatabase.GetPreparedStatement(LOGIN_UPD_EXPIRED_ACCOUNT_BANS));

      banExpiryCheckTimer->expires_from_now(boost::posix_time::seconds(banExpiryCheckInterval));
      banExpiryCheckTimer->async_wait(
          std::bind(&BanExpiryHandler, banExpiryCheckTimerRef, banExpiryCheckInterval, std::placeholders::_1));
    }
  }
}

ConsoleArguments GetConsoleArguments(int argc, char** argv, fs::path& configFile)
{
  argparse::ArgumentParser parser("ac authserver", "1.0", argparse::default_arguments::none);
  parser.add_argument("-h", "--help").help("print usage message").flag();
  parser.add_argument("-v", "--version").help("print version build info").flag();
  parser.add_argument("-d", "--dry-run").help("Dry run").flag();
  parser.add_argument("-c", "--config")
      .help("use <arg> as configuration file")
      .default_value(std::string(sConfigMgr->GetConfigPath() + std::string(_ACORE_REALM_CONFIG)))
      .append();

  // mimic the old allow_unregistered behavior: unknown arguments are not
  // ours to parse - sConfigMgr->Configure() re-scans the full argv for
  // config overrides, so only pick out the known flags here
  std::vector<std::string> known;
  known.push_back(argv[0]);
  for (int i = 1; i < argc; ++i)
  {
    std::string_view const arg = argv[i];
    if (arg == "-h" || arg == "--help" || arg == "-v" || arg == "--version" || arg == "-d" || arg == "--dry-run")
      known.emplace_back(arg);
    else if (arg == "-c" || arg == "--config")
    {
      known.emplace_back("--config");
      if (i + 1 < argc)
        known.emplace_back(argv[++i]);
    }
    else if (arg.starts_with("--config="))
      known.emplace_back(arg);
    else if (arg.starts_with("-c="))
    {
      known.emplace_back("--config");
      known.emplace_back(arg.substr(3));
    }
  }

  ConsoleArguments out;
  try
  {
    parser.parse_args(known);

    configFile = fs::path(parser.get<std::string>("--config"));
    out.help = parser.get<bool>("--help");
    out.version = parser.get<bool>("--version");

    if (parser.get<bool>("--dry-run"))
      sConfigMgr->setDryRun(true);
  }
  catch (std::exception const& e)
  {
    std::cerr << e.what() << "\n";
  }

  if (out.help)
    std::cout << parser.help().str() << "\n";

  return out;
}
