/**
 * Copyright (C) 2007 Doug Judd (Zvents, Inc.)
 * 
 * This file is part of Hypertable.
 * 
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or any later version.
 * 
 * Hypertable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

extern "C" {
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
}

#include <boost/thread/thread.hpp>

#include "Common/Error.h"
#include "Common/FileUtils.h"
#include "Common/TestHarness.h"
#include "Common/System.h"
#include "Common/Usage.h"

#include "AsyncComm/Comm.h"
#include "AsyncComm/ConnectionManager.h"
#include "AsyncComm/Event.h"
#include "AsyncComm/ReactorFactory.h"

#include "DfsBroker/Lib/DfsBrokerClient.h"

#include "dfsTestThreadFunction.h"

using namespace hypertable;

namespace {
  const short DEFAULT_PORT = 38546;
  const char *usage[] = {
    "usage: dfsTest",
    "",
    "  This program tests the operation of the DFS and DFS broker",
    "  by copying the file /usr/share/dict/words to the DFS via the",
    "  broker, then copying it back and making sure the returned copy",
    "  matches the original.  It assumes the DFS broker is listenting",
    "  at localhost:38546",
    (const char *)0
  };
}


int main(int argc, char **argv) {
  boost::thread  *thread1, *thread2;
  struct sockaddr_in addr;
  Comm *comm;
  ConnectionManager *connManager;
  DfsBrokerClient *client;

  if (argc != 1)
    Usage::DumpAndExit(usage);

  System::Initialize(argv[0]);
  ReactorFactory::Initialize(1);

  memset(&addr, 0, sizeof(struct sockaddr_in));
  {
    struct hostent *he = gethostbyname("localhost");
    if (he == 0) {
      herror("gethostbyname()");
      return 1;
    }
    memcpy(&addr.sin_addr.s_addr, he->h_addr_list[0], sizeof(uint32_t));
  }
  addr.sin_family = AF_INET;
  addr.sin_port = htons(DEFAULT_PORT);

  comm = new Comm();
  connManager = new ConnectionManager(comm);
  client = new DfsBrokerClient(connManager, addr, 15);

  if (!client->WaitForConnection(15)) {
    LOG_ERROR("Unable to connect to DFS");
    return 1;
  }

  dfsTestThreadFunction threadFunc(client, "/usr/share/dict/words");

  threadFunc.SetDfsFile("/RegressionTests/hdfs/output.a");
  threadFunc.SetOutputFile("output.a");
  thread1 = new boost::thread(threadFunc);

  threadFunc.SetDfsFile("/RegressionTests/hdfs/output.b");
  threadFunc.SetOutputFile("output.b");
  thread2 = new boost::thread(threadFunc);

  thread1->join();
  thread2->join();

  if (system("diff /usr/share/dict/words output.a"))
    return 1;

  if (system("diff output.a output.b"))
    return 1;

  return 0;
}