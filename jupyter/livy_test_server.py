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
"""This is a more-or-less functional Livy server for our test purposes.

NOTES:
Single threaded only - ie. have just one client sending requests sequentially.
Batches don't work - need probably to hook up spark-submit in a side-process.
"""
# pylint: disable=invalid-name

import code
import dataclasses
import http.server
import io
import json
import logging
import os
import pyspark
import re
import sys
import socketserver
import threading
import traceback
import time
import urllib.parse

from typing import Any, Callable, Dict, List, Optional, Tuple

# Per: https://livy.incubator.apache.org/docs/latest/rest-api.html


@dataclasses.dataclass
class SessionData:
    """Representation of a Session in Livy"""
    id_: int = 0
    appId: int = 0
    owner: str = None
    proxyUser: str = None
    kind: str = None
    log: List[str] = None
    state: str = 'starting'
    appInfo: Dict[str, Any] = None


@dataclasses.dataclass
class StatementOutput:
    status: str = None  # 'ok' or 'error'
    execution_count: int = 0
    data: Dict[str, Any] = None  # e.g. {'text/plain': '...'}
    ename: str = None
    evalue: str = None
    traceback: List[str] = None


@dataclasses.dataclass
class StatementData:
    """Representation of a Statement in Livy"""
    id_: int = 0
    code: str = None
    state: str = None
    output: Dict[str, Any] = None  # a StatementOutput as dict
    progress: float = 0.0
    started: int = 0
    completed: int = 0


@dataclasses.dataclass
class BatchData:
    """Representation of a Batch in Livy"""
    id_: int = 0
    appId: str = 0
    appInfo: Dict[str, Any] = None
    log: List[str] = None
    state: str = 'starting'


@dataclasses.dataclass
class GetSessionsReq:
    """GET '/sessions' => [GetSessionsReq] GetSessionsResp"""
    from_: int = 0
    size: int = 100


@dataclasses.dataclass
class GetSessionsResp:
    """GET '/sessions' => [GetSessionsReq] GetSessionsResp"""
    from_: int = 0
    total: int = 0
    sessions: List[SessionData] = None


@dataclasses.dataclass
class PostSessionReq:
    """POST '/sessions' [PostSessionReq] => SessionData"""
    kind: str = None
    proxyUser: str = None
    jars: List[str] = None
    pyFiles: List[str] = None
    files: List[str] = None
    driverMemory: str = None
    driverCores: str = None
    executorMemory: str = None
    executorCores: str = None
    numExecutors: int = None
    archives: List[str] = None
    queue: str = None
    name: str = None
    conf: Dict[str, Any] = None
    heartbeatTimeoutInSecond: int = None


@dataclasses.dataclass
class GetSessionStateResp:
    """GET '/sessions/{sessionId}/state' => GetSessionStateResp"""
    id_: int = None
    state: str = None


@dataclasses.dataclass
class GetSessionLogReq:
    """GET '/sessions/{sessionId}/log' [GetSessionLogReq] => GetSessionLogResp"""
    from_: int = 0
    size: int = 100


@dataclasses.dataclass
class GetSessionLogResp:
    """GET '/sessions/{sessionId}/log' [GetSessionLogReq] => GetSessionLogResp"""
    id_: int = None
    from_: int = 0
    size: int = 100
    log: List[str] = 0


@dataclasses.dataclass
class GetSessionStatementsResp:
    """GET '/sessions/{sessionId}/statements' => GetSessionStatementsResp"""
    statements: List[StatementData]


@dataclasses.dataclass
class PostStatementReq:
    """POST '/sessions/{sessionId}/statements' [PostStatementReq] => StatementData"""
    code: str = None
    kind: str = None


@dataclasses.dataclass
class PostStatementCancelResp:
    """POST '/sessions/{sessionId}/statements/{statementId}/cancel'
        [] => PostStatementCancelResp"""
    msg: str = 'canceled'


@dataclasses.dataclass
class PostSessionCompletionReq:
    """POST '/sessions/{sessionId}/completion'
        [PostSessionCompletionReq] => PostSessionCompletionResp"""
    code: str = None
    kind: str = None
    cursor: str = None


@dataclasses.dataclass
class PostSessionCompletionResp:
    """POST '/sessions/{sessionId}/completion'
        [PostSessionCompletionReq] => PostSessionCompletionResp"""
    candidates: List[str] = None


# @dataclasses.dataclass
# class GetBatchesReq:
#     from_: int = 0
#     size: int = 50


@dataclasses.dataclass
class GetBatchesResp:
    """GET '/batches' => GetBatchesResp"""
    from_: int = None
    total: int = None
    sessions: List[BatchData] = None


@dataclasses.dataclass
class PostBatchesReq:
    """POST '/batches' [PostBatchesReq] => BatchData"""
    file_: str = None
    proxyUser: str = None
    className: str = None
    args: str = None
    jars: List[str] = None
    pyFiles: List[str] = None
    files: List[str] = None
    driverMemory: str = None
    driverCores: str = None
    executorMemory: str = None
    executorCores: str = None
    archives: List[str] = None
    queue: str = None
    name: str = None
    conf: Dict[str, Any] = None


@dataclasses.dataclass
class GetBatchesStateResp:
    """GET '/batches/{batchId}/state' => GetBatchesStateResp"""
    id_: int = None
    state: str = None


@dataclasses.dataclass
class GetBatchesLogReq:
    """GET '/batches/{batchId}/log' [GetBatchesLogReq] => GetBatchesLogResp"""
    from_: int = 0
    size: int = 0


@dataclasses.dataclass
class GetBatchesLogResp:
    """GET '/batches/{batchId}/log' [GetBatchesLogReq] => GetBatchesLogResp"""
    id_: int = None
    from_: int = None
    size: int = 0
    log: List[str] = 0


@dataclasses.dataclass
class VersionResp:
    """GET /version - Mocking things up"""
    url: str = 'https://github.com/apache/incubator-livy.git'
    branch: str = 'master'
    revision: str = '4d8a912699683b973eee76d4e91447d769a0cb0d'
    version: str = '0.8.0-incubating-SNAPSHOT'
    date: str = '2022-04-28T01:00:16Z'
    user: str = os.environ['USER']


def _rename_dict_keys(data, transforms):
    """As some reserved keywords in python are used in livy,
    transform names."""
    for k, v in transforms.items():
        if k in data:
            data[v] = data[k]
            del data[k]
    for k, v in data.items():
        if isinstance(v, dict):
            _rename_dict_keys(v, transforms)
        elif isinstance(v, list):
            for elem in v:
                if isinstance(elem, dict):
                    _rename_dict_keys(elem, transforms)
    return data


_REQ_KEYS = {
    'from': 'from_',
    'file': 'file_',
    'id': 'id_',
}


def rename_req_keys(data):
    return _rename_dict_keys(data, _REQ_KEYS)


_RESP_KEYS = {
    'from_': 'from',
    'file_': 'file',
    'id_': 'id',
}


def rename_resp_keys(data):
    return _rename_dict_keys(data, _RESP_KEYS)


class LivyError(RuntimeError):
    """Error encountered in the protocol or calls."""

    def __init__(self, msg: str, error_code: int):
        super().__init__(msg)
        self.msg = msg
        self.error_code = error_code


@dataclasses.dataclass
class LivyRequest:
    """Error encountered in the protocol or calls."""
    method: str = None
    path: str = None
    req: Any = None
    resp: Any = None
    error: LivyError = None


class IdGenerator:
    """Generates sequential ids."""

    def __init__(self, start=0):
        self.index = start

    def next(self):
        result = self.index
        self.index += 1
        return result


class StatementRunner(code.InteractiveInterpreter):
    """Runs python statements in a side interpreter."""

    def __init__(self):
        super().__init__()
        self.interp = ()
        self.saved_stdout = sys.stdout
        self.saved_stderr = sys.stderr
        self.run_id = IdGenerator()
        self.last_error_type = None
        self.last_error_value = None
        self.last_error_traceback = None
        self.last_error_lines = None

    def showtraceback(self):
        (self.last_error_type, self.last_error_value,
         self.last_error_traceback) = sys.exc_info()
        self.last_error_lines = traceback.format_exception(
            self.last_error_type, self.last_error_value,
            self.last_error_traceback)

    def clear_error(self):
        self.last_error_type = None
        self.last_error_value = None
        self.last_error_traceback = None
        self.last_error_lines = None

    def has_error(self):
        return self.last_error_type is not None

    def format_error(self):
        if not self.has_error():
            return None
        return (f'Error: {self.last_error_type}, '
                f'Value: {self.last_error_value} '
                f'Lines: {self.last_error_lines}')

    def runcode(self, py_code: str):  # pylint: disable=arguments-renamed
        self.clear_error()
        with io.StringIO() as run_stdout, io.StringIO() as run_stderr:
            sys.stdout = run_stdout
            sys.stderr = run_stderr
            try:
                super().runcode(py_code)
            finally:
                sys.stdout = self.saved_stdout
                sys.stderr = self.saved_stderr
            return (run_stdout.getvalue(), run_stderr.getvalue())


class Statement:
    """A livy statement, represented on the server side."""

    def __init__(self, statement_id: int, py_code: str):
        self.statement_id = statement_id
        self.py_code = py_code
        self.stdout = None
        self.stderr = None
        self.progress = 0.0
        self.started = None
        self.completed = None
        self.output: StatementOutput = None

    def statement_data(self) -> StatementData:
        return StatementData(
            id_=self.statement_id,
            code=self.py_code,
            state='available' if self.output else 'running',
            output=(dataclasses.asdict(self.output) if self.output else None),
            progress=1.0,
            started=self.started,
            completed=self.completed)

    def run(self, runner: StatementRunner):
        self.started = int(time.time())
        self.output = StatementOutput()
        self.stdout, self.stderr = runner.runcode(self.py_code)
        if runner.has_error():
            self.output.status = 'error'
            self.output.data = {'text/plain': self.stderr}
            self.output.ename = (f'{runner.last_error_type.__module__}'
                                 f'.{runner.last_error_type.__qualname__}')
            self.output.evalue = str(runner.last_error_value)
            self.output.traceback = runner.last_error_lines
        else:
            self.output.status = 'ok'
            self.output.data = {'text/plain': self.stdout}
            self.output.evalue = self.stderr
        self.output.execution_count = runner.run_id.next()
        self.progress = 1.0
        self.completed = int(time.time())


class Session:
    """A livy session, represented on the server side."""

    def __init__(self, session_id: int, req: PostSessionReq):
        self.session_id = session_id
        self.req = req
        self.kind = req.kind
        self.proxyUser = req.proxyUser
        self.next_statement_id = IdGenerator()
        self.state = 'starting'
        self.runner = StatementRunner()
        self.statements = []
        self.log = []
        self.init_spark()

    def init_spark(self):
        self.runner.runcode(f"""
from pyspark.sql import SparkSession
spark = SparkSession.builder.master("local[*]").appName(
    "test-{self.session_id}").getOrCreate()
""")
        assert not self.runner.has_error(), self.runner.format_error()
        print('Spark session initialized for {self.session_id}')

    def session_data(self) -> SessionData:
        return SessionData(id_=self.session_id,
                           kind=self.kind,
                           proxyUser=self.proxyUser,
                           log=self.log,
                           state=self.state)

    def session_state(self) -> GetSessionStateResp:
        return GetSessionStateResp(id_=self.session_id, state=self.state)

    def session_log(self, req: GetSessionLogReq) -> GetSessionLogResp:
        return GetSessionLogResp(id_=self.session_id,
                                 from_=req.from_,
                                 size=len(self.log),
                                 log=self.log[req.from_:][:req.size])

    def get_statements(self) -> GetSessionStatementsResp:
        return GetSessionStatementsResp(statements=[
            statement.statement_data() for statement in self.statements
        ])

    def find_statement(self, statement_id: int) -> Statement:
        for statement in self.statements:
            if statement.statement_id == statement_id:
                return statement
        raise LivyError(
            f'Statement {statement_id} not found '
            f'in session {self.session_id}', 404)

    def post_statement(self, req: PostStatementReq) -> StatementData:
        statement = Statement(self.next_statement_id.next(), req.code)
        self.statements.append(statement)
        # Force two RPCs on this - execute after building response
        result = statement.statement_data()
        statement.run(self.runner)
        return result

    def cancel_statement(self, statement_id: int) -> PostStatementCancelResp:
        del_statement = self.find_statement(statement_id)
        self.statements = [
            statement for statement in self.statements
            if statement.statement_id != statement_id
        ]
        del del_statement
        return PostStatementCancelResp()


class Batch:
    """A livy batchA, represented on the server side."""

    def __init__(self, batch_id: int, req: PostBatchesReq):
        self.batch_id = batch_id
        self.req = req
        self.state = 'running'
        self.log = []

    def batch_data(self) -> BatchData:
        return BatchData(id_=self.batch_id,
                         appId=self.req.name,
                         appInfo={},
                         log=[],
                         state=self.state)

    def batch_state(self) -> GetBatchesStateResp:
        return GetBatchesStateResp(id_=self.batch_id, state=self.state)

    def batch_log(self, req: GetBatchesLogReq) -> GetBatchesLogResp:
        return GetBatchesLogResp(id_=self.batch_id,
                                 from_=req.from_,
                                 size=len(self.log),
                                 log=self.log[req.from_:][:req.size])


class LivyServer(socketserver.TCPServer):
    """Livy Server protocol implementation. For python code only.
    Partial implementation, and single threaded only, with no
    batch execution."""

    def __init__(self, address, loglevel=logging.WARNING):
        super().__init__(address, LivyRequestHandler)
        self.logger = logging.getLogger('LivyServer')
        self.logger.setLevel(loglevel)
        # Force using the bazel-linked pyspark location
        self.logger.info('Setting spark home to: %s', pyspark.__path__)
        os.environ['SPARK_HOME'] = ':'.join(pyspark.__path__)
        self.next_session_id = IdGenerator()
        self.next_batch_id = IdGenerator()
        self.sessions = []
        self.batches = []
        self.requests_log: List[LivyRequest] = []

    def append_log(self, req: LivyRequest):
        self.logger.info('Request processed: %s', req)
        self.requests_log.append(req)

    def _find_session(self, session_id: int):
        for session in self.sessions:
            if session.session_id == session_id:
                return session
        raise LivyError(f'Seesion not found {session_id}', 404)

    def _find_batch(self, batch_id: int):
        for batch in self.batches:
            if batch.batch_id == batch_id:
                return batch
        raise LivyError(f'Batch not found {batch_id}', 404)

    def get_sessions(self, req: GetSessionsReq) -> GetSessionsResp:
        return GetSessionsResp(
            from_=req.from_,
            total=len(self.sessions),
            sessions=[
                session.session_data()
                for session in self.sessions[req.from_:][:req.size]
            ])

    def get_session(self, session_id: int) -> SessionData:
        return self._find_session(session_id).session_data()

    def get_session_state(self, session_id: int) -> GetSessionStateResp:
        return self._find_session(session_id).session_state()

    def get_session_log(self, session_id: int,
                        req: GetSessionLogReq) -> GetSessionLogResp:
        return self._find_session(session_id).session_log(req)

    def get_sesssion_statements(self,
                                session_id: int) -> GetSessionStatementsResp:
        return self._find_session(session_id).get_statements()

    def get_session_statement(self, session_id: int,
                              statement_id: int) -> StatementData:
        return self._find_session(session_id).find_statement(
            statement_id).statement_data()

    def get_batches(self) -> GetBatchesResp:
        return GetBatchesResp(
            from_=0,
            total=len(self.batches),
            sessions=[batch.batch_data() for batch in self.batches])

    def get_batch(self, batch_id: int) -> BatchData:
        return self._find_batch(batch_id).batch_data()

    def get_batch_state(self, batch_id: int) -> GetBatchesStateResp:
        return self._find_batch(batch_id).batch_state()

    def get_batch_log(self, batch_id: int,
                      req: GetBatchesLogReq) -> GetBatchesLogResp:
        return self._find_batch(batch_id).batch_log(req)

    def delete_session(self, session_id: int) -> str:
        del_session = self._find_session(session_id)
        self.sessions = [
            session for session in self.sessions
            if session.session_id != session_id
        ]
        del del_session
        return '{}'

    def delete_batch(self, batch_id: int) -> str:
        del_batch = self._find_batch(batch_id)
        self.batches = [
            batch for batch in self.batches if batch.batch_id != batch_id
        ]
        del del_batch
        return '{}'

    def post_sessions(self, req: PostSessionReq) -> SessionData:
        if req.kind not in ('pyspark', 'sql'):
            raise LivyError(
                f'We support only pyspark or sql sessions, not {req.kind}', 400)
        session = Session(self.next_session_id.next(), req)
        self.sessions.append(session)
        result = session.session_data()
        session.state = 'idle'
        return result

    def post_statements(self, session_id: int,
                        req: PostStatementReq) -> StatementData:
        if req.kind != 'pyspark':
            raise LivyError(
                f'We support only pyspark statements, not {req.kind}', 400)
        return self._find_session(session_id).post_statement(req)

    def post_statements_cancel(self, session_id: int,
                               statement_id: int) -> PostStatementCancelResp:
        return self._find_session(session_id).cancel_statement(statement_id)

    def post_session_completion(
            self, session_id: int,
            req: PostSessionCompletionReq) -> PostSessionCompletionResp:
        del session_id, req
        return PostSessionCompletionResp(candidates=[])

    def post_batches(self, req: PostBatchesReq) -> BatchData:
        batch = Batch(self.next_batch_id.next(), req=req)
        self.batches.append(batch)
        result = batch.batch_data()
        if req.file_.startswith('error-'):
            batch.state = 'error'
        else:
            batch.state = 'success'
        return result


class LivyRequestHandler(http.server.BaseHTTPRequestHandler):
    """Handles requests for our http server."""

    def __init__(self, request, client_address, server):
        assert isinstance(server, LivyServer), f'Server for request: {server}'
        self.logger = server.logger
        super().__init__(request, client_address, server)

    def _write_response(self, response, error_code=200):
        self.send_response(error_code)
        if dataclasses.is_dataclass(response):
            content_type = 'application/json'
            contents = json.dumps(rename_resp_keys(
                dataclasses.asdict(response))).encode('utf-8')
        else:
            content_type = 'text/plain'
            contents = response.encode('utf-8')
        self.logger.debug('Responding to %s with:\n%s', self.path, contents)
        self.send_header('Content-type', content_type)
        self.end_headers()
        self.wfile.write(contents)

    def _read_post_data(self, datacls: type) -> Any:
        length = int(self.headers['content-length'])
        data = self.rfile.read(length).decode('utf-8')
        data_dict = json.loads(data)
        return datacls(**rename_req_keys(data_dict))

    def _read_get_data(self, query: Optional[Dict[str, List[str]]],
                       datacls: type) -> Any:
        result = datacls()
        if not query:
            return result
        for field in dataclasses.fields(datacls):
            src_name = (_RESP_KEYS[field.name]
                        if field.name in _RESP_KEYS else field.name)
            if src_name in query and query[src_name]:
                value = query[src_name][0]
                setattr(result, field.name, field.type(value))
        self.logger.debug('Parsed get result %s: %s', self.path, result)
        return result

    def _process_get(self, query):
        req = None
        resp = None
        if self.path.startswith('/sessions'):
            if self.path == '/sessions':
                # '/sessions' => [GetSessionsReq] GetSessionsResp
                req = self._read_get_data(query, GetSessionsReq)
                resp = self.server.get_sessions(req)
            elif re.fullmatch(r'/sessions/[0-9]+', self.path):
                # '/sessions/{sessionId}' => SessionData
                resp = self.server.get_session(
                    int(
                        re.fullmatch(r'/sessions/([0-9]+)',
                                     self.path).group(1)))
            elif re.fullmatch(r'/sessions/[0-9]+/state', self.path):
                # '/sessions/{sessionId}/state' => GetSessionStateResp
                resp = self.server.get_session_state(
                    int(
                        re.fullmatch(r'/sessions/([0-9]+)/state',
                                     self.path).group(1)))
            elif re.fullmatch(r'/sessions/[0-9]+/log', self.path):
                # '/sessions/{sessionId}/log' [GetSessionLogReq] => GetSessionLogResp
                req = self._read_get_data(query, GetSessionLogReq)
                resp = self.server.get_session_log(
                    int(
                        re.fullmatch(r'/sessions/([0-9]+)/log',
                                     self.path).group(1)), req)
            elif re.fullmatch(r'/sessions/[0-9]+/statements', self.path):
                # '/sessions/{sessionId}/statements' => GetSessionStatementsResp
                resp = self.server.get_session_statements(
                    int(
                        re.fullmatch(r'/sessions/([0-9]+)/statements',
                                     self.path).group(1)))
            elif re.fullmatch(r'/sessions/[0-9]+/statements/[0-9]+', self.path):
                # '/sessions/{sessionId}/statements/{statementId}' => StatementData
                matcher = re.fullmatch(
                    r'/sessions/([0-9]+)/statements/([0-9]+)', self.path)
                resp = self.server.get_session_statement(
                    int(matcher.group(1)), int(matcher.group(2)))
        elif self.path.startswith('/batches'):
            if self.path == '/batches':
                # '/batches' => GetBatchesResp
                resp = self.server.get_batches()
            elif re.fullmatch(r'/batches/[0-9]+', self.path):
                # '/batches/{batchId}' => BatchData
                resp = self.server.get_batch(
                    int(re.fullmatch(r'/batches/([0-9]+)', self.path).group(1)))
            elif re.fullmatch(r'/batches/[0-9]+/state', self.path):
                # '/batches/{batchId}/state' => GetBatchesStateResp
                resp = self.server.get_batch_state(
                    int(
                        re.fullmatch(r'/batches/([0-9]+)/state',
                                     self.path).group(1)))
            elif re.fullmatch(r'/batches/[0-9]+/log', self.path):
                # '/batches/{batchId}/log' [GetBatchesLogReq] => GetBatchesLogResp
                req = self._read_get_data(query, GetBatchesLogReq)
                resp = self.server.get_batch_log(
                    int(
                        re.fullmatch(r'/batches/([0-9]+)/log',
                                     self.path).group(1)), req)
        elif self.path == '/version':
            resp = VersionResp()
        if resp is None:
            raise LivyError(f'Unknown GET request path {self.path}', 404)
        return req, resp

    def do_GET(self):
        if self.path == '/quitquitquit':
            self.logger.info('Shutting down the server')
            self._write_response('OK')
            self.server.shutdown()
            return
        path_comp = self.path.split('?', 1)
        self.path = path_comp[0]
        if len(path_comp) > 1:
            query = urllib.parse.parse_qs(path_comp[1])
        else:
            query = None
        try:
            req, resp = self._process_get(query)
        except LivyError as e:
            self.server.append_log(
                LivyRequest(method='GET', path=self.path, error=e))
            self._write_response(e.msg, e.error_code)
            return
        self.server.append_log(
            LivyRequest(method='GET', path=self.path, req=req, resp=resp))
        self._write_response(resp)

    def _process_delete(self):
        resp = None
        if self.path.startswith('/sessions'):
            if re.fullmatch(r'/sessions/[0-9]+', self.path):
                # '/sessions/{sessionId}'
                resp = self.server.delete_session(
                    int(
                        re.fullmatch(r'/sessions/([0-9]+)',
                                     self.path).group(1)))
        elif self.path.startswith('/batches'):
            # '/batches/{batchId}'
            if re.fullmatch(r'/batches/[0-9]+', self.path):
                resp = self.server.delete_batch(
                    int(re.fullmatch(r'/batches/([0-9]+)', self.path).group(1)))
        if resp is None:
            raise LivyError(f'Unknown DELETE request path {self.path}', 404)
        return None, resp

    def do_DELETE(self):
        try:
            req, resp = self._process_delete()
        except LivyError as e:
            self.server.append_log(
                LivyRequest(method='DELETE', path=self.path, error=e))
            self._write_response(e.msg, e.error_code)
            return
        self.server.append_log(
            LivyRequest(method='DELETE', path=self.path, req=req, resp=resp))
        self._write_response(resp)

    def _process_post(self):
        req = None
        resp = None
        if self.path.startswith('/sessions'):
            if self.path == '/sessions':
                # '/sessions' [PostSessionReq] => SessionData
                req = self._read_post_data(PostSessionReq)
                resp = self.server.post_sessions(req)
            elif re.fullmatch(r'/sessions/[0-9]+/statements', self.path):
                # '/sessions/{sessionId}/statements' [PostStatementReq] => StatementData
                req = self._read_post_data(PostStatementReq)
                resp = self.server.post_statements(
                    int(
                        re.fullmatch(r'/sessions/([0-9]+)/statements',
                                     self.path).group(1)), req)
            elif re.fullmatch(r'/sessions/[0-9]+/statements/[0-9]+/cancel',
                              self.path):
                # '/sessions/{sessionId}/statements/{statementId}/cancel'
                #    [] => PostStatementCancelResp
                matcher = re.fullmatch(
                    r'/sessions/([0-9]+)/statements/([0-9]+)/cancel', self.path)
                resp = self.server.post_statements_cancel(
                    int(matcher.group(1)), int(matcher.group(2)))
            elif re.fullmatch(r'/sessions/[0-9]+/completion', self.path):
                # '/sessions/{sessionId}/completion'
                #    [PostSessionCompletionReq] => PostSessionCompletionResp
                req = self._read_post_data(PostSessionCompletionReq)
                resp = self.server.post_session_completion(
                    int(
                        re.fullmatch(r'/sessions/([0-9]+)/completion',
                                     self.path).group(1)), req)
        elif self.path.startswith('/batches'):
            if self.path == '/batches':
                # '/batches' [PostBatchesReq] => BatchData
                req = self._read_post_data(PostBatchesReq)
                resp = self.server.post_batches(req)
        if resp is None:
            raise LivyError(f'Unknown post request path {self.path}', 404)
        return req, resp

    def do_POST(self):  # pylint: disable=invalid-name
        try:
            req, resp = self._process_post()
        except LivyError as e:
            self.server.append_log(
                LivyRequest(method='POST', path=self.path, error=e))
            self._write_response(e.msg, e.error_code)
            return
        self.server.append_log(
            LivyRequest(method='POST', path=self.path, req=req, resp=resp))
        self._write_response(resp)


_SERVER = None


def run_server(port: int = 0,
               loglevel: int = logging.WARNING,
               server_callback: Callable = None):
    """Starts the livy mock server - main entry point into the runnable.
    For port==0 - chooses a free port.
    """
    with LivyServer(('', port), loglevel) as livy_server:
        port = livy_server.socket.getsockname()[1]
        print(f'Serving at {port}')
        if server_callback:
            server_callback(livy_server, port)
        try:
            livy_server.serve_forever()
        except KeyboardInterrupt:
            pass


def start_test_server(
    port: int = 0,
    loglevel: int = logging.WARNING
) -> Tuple[threading.Thread, LivyServer, int]:
    """Starts the server on a side thread, using a free port.
    Returns the server thread & port.
    """
    event = threading.Event()
    callback_result = []

    def port_callback(livy_server, server_port):
        callback_result.append((livy_server, server_port))
        event.set()

    server_thread = threading.Thread(target=run_server,
                                     args=(port, loglevel, port_callback))
    server_thread.start()
    event.wait()
    return (server_thread, callback_result[0][0], callback_result[0][1])
