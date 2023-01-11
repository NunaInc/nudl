#
# Copyright 2022 Nuna inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https:#www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
"""Starts the livy test server as a stand alone process."""

from absl import app
from absl import flags
from nuna_nudl.jupyter.test import livy_test_server

FLAGS = flags.FLAGS
flags.DEFINE_integer(
    'port', 0, 'Runs server on this port. If 0, picks an unused local port.')


def main(argv):
    del argv
    (server_thread, server,
     port) = livy_test_server.start_test_server(FLAGS.port)
    print(f'Livy test server running on port: {port}')
    try:
        server_thread.join()
    except KeyboardInterrupt:
        print('Shutting down the server')
        server.shutdown()
    print('DONE')


if __name__ == '__main__':
    app.run(main)
