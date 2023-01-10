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
"""Contains utilities to create a user livy/spark session."""
import dataclasses
import livy
import logging
import os
import requests
import threading
import time

from nuna_nudl.jupyter import codeutil
from typing import Any, Dict, List, Optional, Set, Tuple, Union


@dataclasses.dataclass
class LivyConnectInfo:
    """Contains parameters for connecting to the Livy server.

    Please check livy.client.LivyClient constructor for the meaning of it.
    """
    livy_url: str = 'http://localhost:8998/'
    auth: Optional[Union[requests.auth.AuthBase, Tuple[str, str]]] = None
    verify: Union[bool, str] = True
    requests_session: Optional[requests.Session] = None
    username: Optional[str] = None


@dataclasses.dataclass
class SparkExecInfo:
    """Execution info for Spark sessions create through livy."""
    # Amount of memory to use for driver process (e.g. '256m').
    driver_memory: Optional[str] = None
    # Number of cores to use for driver process
    driver_cores: Optional[int] = None
    # Amount of memory to use per executor process (e.g. '512m').
    executor_memory: Optional[str] = None
    # Number of cores per each executor.
    executor_cores: Optional[int] = None
    # Number of executors to launch.
    num_executors: Optional[int] = None
    # Spark configuration properties.
    spark_conf: Optional[Dict[str, Any]] = None
    # Timeout in seconds to which session is orphaned if no heartbeat
    # is received.
    heartbeat_timeout: Optional[int] = None


@dataclasses.dataclass
class SparkExecFiles:
    """Files to be brought in a spark session/batch created through livy."""
    # URLs of jars to be used in this session.
    jars: Optional[List[str]] = None
    # URLs of Python files to be used in this session.
    py_files: Optional[List[str]] = None
    # URLs of files to be used in this session.
    files: Optional[List[str]] = None
    # URLs of archives to be used in this session.
    archives: Optional[List[str]] = None


def get_username(use_shell_user: bool = False) -> str:
    """Returns the end-username for the notebook."""
    if 'JUPYTERHUB_USER' in os.environ:
        user = os.environ['JUPYTERHUB_USER']
    elif use_shell_user and 'USER' in os.environ:
        user = os.environ['USER']
    else:
        return ''
    if "'" in user or '"' in user or '`' in user or '@' in user:
        raise Exception(
            'JupyterHub username should never contain quotes or at signs!')
    return user


def ensure_username(use_shell_user: bool = False) -> str:
    user = get_username(use_shell_user)
    if not user:
        user = input('Enter username: ')
    return user


def _rekey(data, old_key, new_key):
    if old_key not in data:
        data[new_key] = None
    else:
        data[new_key] = data[old_key]
        del data[old_key]
    return data


class StatementProgressUpdater:
    """Processes updates during statemnet execution."""

    def __init__(self):
        pass

    def start(self, code: str, statement: livy.models.Statement):
        """A statement has just started."""
        pass

    def update(self, statement: livy.models.Statement) -> bool:
        """An update for statement execution.
        Returns if statement should be canceled."""
        del statement
        return False

    def ended(self, statement: livy.models.Statement, canceled: bool):
        """A statement has ended."""
        pass


class BatchProgressUpdater:
    """Processes updates during batch execution."""

    def __init__(self):
        pass

    def start(self, batch: livy.models.Batch):
        """Called when a batch just started."""
        pass

    def update(self, batch: livy.models.Batch) -> bool:
        """An update for batch execution.
        Returns if batch should be canceled."""
        del batch
        return False

    def ended(self, batch: livy.models.Batch, canceled: bool):
        """A batch has ended."""
        pass


class LivyClient(livy.client.LivyClient):
    """Livy client we use, with some extra methods."""

    def session_state(self, session_id: int):
        return self._client.get(f'/sessions/{session_id}/state')['state']

    def last_session_log_line(self, session_id: int):
        return self._client.get(f'/sessions/{session_id}/log', {
            'from': -1,
            'size': 1
        })['from']

    def session_log(self,
                    session_id: int,
                    from_index: int = 0,
                    size: int = 100):
        return self._client.get(f'/sessions/{session_id}/log', {
            'from': from_index,
            'size': size
        })['log']

    def batch_log(self, batch_id: int, from_index: int = 0, size: int = 100):
        return self._client.get(f'/batches/{batch_id}/log', {
            'from': from_index,
            'size': size
        })['log']

    def cancel_statement(self, session_id: int, statement_id: int):
        return self._client.post(
            f'/sessions/{session_id}/statements/{statement_id}/cancel',
            {})['msg']


def check_livy_statement(livy_statement: livy.models.Statement,
                         message: str,
                         ok_no_output: bool = False):
    if livy_statement is None:
        raise InterruptedError(f'{message} - CANCELLED')
    livy_statement.output.raise_for_status()
    if not livy_statement.output.text and not ok_no_output:
        raise RuntimeError(f'{message} - produced no output')


@dataclasses.dataclass
class SqlCommandArgs:
    quiet: bool = False
    samplemethod: Optional[str] = None
    samplefraction: Optional[float] = None
    maxrows: Optional[int] = None


_LIVY_INTERNAL_VAR_NAME = 'nudl_livy_internal_reserved'


class LivyExecutionContext:
    """Contains a Livy client and session."""

    def __init__(self,
                 connect_info: LivyConnectInfo,
                 logger: Optional[logging.Logger] = None):
        self.connect_info = connect_info
        self.client = LivyClient(url=connect_info.livy_url,
                                 auth=connect_info.auth,
                                 verify=connect_info.verify,
                                 requests_session=connect_info.requests_session)
        self.logger: logging.Logger = self._logger(logger)
        self.session: Optional[livy.session.LivySession] = None
        self.session_id: Optional[int] = None
        self.session_var_name: Optional[str] = None
        self.verified_modules: Set[str] = set()

    def check_session(self):
        if not self.session:
            raise ValueError('Livy session not connected.')

    def has_idle_session(self):
        if self.session is None:
            return False
        return self.session.state == livy.models.SessionState.IDLE

    def _logger(self, logger: logging.Logger):
        if logger is None:
            logger = logging.getLogger('LivyExec')
            logger.setLevel(logging.INFO)
        return logger.getChild('LivyExec')

    def init_session(self,
                     name: Optional[str] = None,
                     spark_info: Optional[SparkExecInfo] = None,
                     spark_files: Optional[SparkExecFiles] = None,
                     session_id: Optional[int] = None,
                     use_last_session: bool = False,
                     wait_connection: bool = True):
        connect_to_session = session_id is not None and session_id >= 0
        if (not connect_to_session and use_last_session and
                self.connect_info.username):
            sessions = [
                session for session in self.client.list_sessions()
                if session.proxy_user == self.connect_info.username
            ]
            if sessions:
                session_id = sessions[-1].session_id
        if session_id is None or session_id < 0:
            if not spark_files:
                spark_files = SparkExecFiles()
            session_model = self.client.create_session(
                kind=livy.SessionKind.SQL,
                proxy_user=self.connect_info.username,
                jars=spark_files.jars,
                py_files=spark_files.py_files,
                files=spark_files.files,
                driver_memory=spark_info.driver_memory,
                driver_cores=spark_info.driver_cores,
                executor_memory=spark_info.executor_memory,
                executor_cores=spark_info.executor_cores,
                num_executors=spark_info.num_executors,
                archives=spark_files.archives,
                queue=None,
                name=name,
                spark_conf=spark_info.spark_conf,
                heartbeat_timeout=spark_info.heartbeat_timeout)
            self.logger.info('Created remote livy session model: %s',
                             session_model)
            session_id = session_model.session_id
        else:
            self.logger.info('Reconnecting to session is: %s', session_id)
        self.session = livy.session.LivySession(
            url=self.connect_info.livy_url,
            session_id=session_id,
            auth=self.connect_info.auth,
            verify=True,
            requests_session=self.connect_info.requests_session,
            kind=livy.SessionKind.SQL,
            echo=True,
            check=True)
        self.session_id = self.session.session_id
        if wait_connection:
            self.logger.info('Waiting for livy session id to become ready: %s',
                             self.session.session_id)
            self.session.wait()
            state = self.session.state
            if state != livy.models.SessionState.IDLE:
                self.logger.warning(
                    'Livy session %s is not idle yet, but in %s',
                    self.session_id, state)
            else:
                self.logger.info('Livy session ready: %s', self.session_id)
        else:
            self.logger.info('Livy session %s created in state %s',
                             self.session_id, self.session.state)
        return self.session

    def is_statement_ok(self, statement: livy.models.Statement) -> bool:
        """If the execution of provided statement ended OK."""
        return (statement is not None and statement.output and
                statement.output.status == livy.models.OutputStatus.OK)

    def check_statement_ok(self, statement: livy.models.Statement,
                           text: str) -> bool:
        """If the execution of provided statement ended OK, else raise error"""
        if not statement:
            raise RuntimeError(f'Statement was CANCELED for: {text}')
        if self.is_statement_ok(statement):
            return statement.output
        if not statement.output:
            raise RuntimeError(f'Statement has nor output for: {text}')
        statement.output.raise_for_status()

    def run_and_check_statement(self,
                                code: str,
                                kind: str = 'py',
                                text: str = 'Failed statement'):
        self.logger.debug('Running and checking statement (%s):\n%s', kind,
                          code)
        try:
            result = self.execute_statement(code, kind)
        except livy.models.SparkRuntimeError as e:
            raise RuntimeError(
                f'Error executing spark statment ({kind}):\n {code}') from e
        return self.check_statement_ok(result, text)

    def ensure_spark_context_var(self):
        """Finds out under which variable the spark context is available."""
        if self.session_var_name is not None:
            return self.session_var_name
        result = self.execute_statement('spark', 'py')
        if self.is_statement_ok(result):
            self.session_var_name = 'spark'
            return self.session_var_name
        result = self.execute_statement('sqlContext', 'py')
        if self.is_statement_ok(result):
            self.session_var_name = 'sqlContext'
            return self.session_var_name
        raise ValueError('Cannot determine Spark session variable name.')

    def verify_module(self, module_name: str) -> bool:
        """Tries to check if the provided module can be loaded on the spark session."""
        if module_name in self.verified_modules:
            return True
        self.check_statement_ok(
            self.execute_statement(f'import {module_name}', 'py'),
            f'Importing module `{module_name}` in spark context.')
        self.verified_modules.add(module_name)
        return True

    def build_sql_code(self,
                       sql_code: str,
                       args: Optional[SqlCommandArgs] = None):
        """Returns a python statement code for running a sql command w/ arguments."""
        self.ensure_spark_context_var()
        command = f'{self.session_var_name}.sql({codeutil.escape_doc(sql_code)}).toJSON()'
        if args:
            if args.samplemethod == 'sample' and (
                    args.samplefraction and 0.0 < args.samplefraction < 1.0):
                command += f'.sample(False, {args.samplefraction})'
            if args.maxrows and args.maxrows >= 0:
                command += f'.take({args.maxrows})'
            else:
                command += '.collect()'
        else:
            command += '.collect()'
        vname = _LIVY_INTERNAL_VAR_NAME
        return f'for {vname} in {command}:\n    print({vname})'

    def execute_statement(
        self,
        code: str,
        kind: str,
        progress_updater: Optional[StatementProgressUpdater] = None,
        cancel_event: Optional[threading.Event] = None
    ) -> Optional[livy.models.Statement]:
        """Executes a statement on the cluster, in the spark context."""
        if kind.upper() == 'SQL':
            stmt_kind = livy.models.StatementKind.SQL
        elif kind.upper() in ('PY', 'PYTHON', 'PYSPARK'):
            stmt_kind = livy.models.StatementKind.PYSPARK
        elif kind.upper() in ('SCALA', 'SPARK'):
            stmt_kind = livy.models.StatementKind.SPARK
        elif kind.upper() == 'R':
            stmt_kind = livy.models.StatementKind.SPARKR
        else:
            raise ValueError(f'Invalid statement kind: {kind}')
        self.check_session()
        statement = self.client.create_statement(self.session.session_id, code,
                                                 stmt_kind)
        if not progress_updater:
            progress_updater = StatementProgressUpdater()
        progress_updater.start(code, statement)

        def waiting_for_output(statement):
            not_finished = statement.state in {
                livy.models.StatementState.WAITING,
                livy.models.StatementState.RUNNING,
            }
            available = statement.state == livy.models.StatementState.AVAILABLE
            return not_finished or (available and statement.output is None)

        intervals = livy.utils.polling_intervals([0.1, 0.2, 0.3, 0.5, 1.0], 2.0)
        cancel = False
        while waiting_for_output(statement):
            if cancel:
                self.client.cancel_statement(statement.session_id,
                                             statement.statement_id)
            wait_interval = next(intervals)
            if cancel_event:
                cancel_event.wait(wait_interval)
                cancel = cancel_event.is_set()
            else:
                time.sleep(wait_interval)
            statement = self.client.get_statement(statement.session_id,
                                                  statement.statement_id)
            cancel = cancel or progress_updater.update(statement)
        progress_updater.ended(statement, cancel)
        if cancel:
            return None
        return statement

    def execute_batch(
        self,
        filename: str,
        class_name: Optional[str],
        args: Optional[List[str]] = None,
        spark_info: Optional[SparkExecInfo] = None,
        spark_files: Optional[SparkExecFiles] = None,
        progress_updater: Optional[BatchProgressUpdater] = None,
        cancel_event: Optional[threading.Event] = None
    ) -> Optional[livy.models.Batch]:
        if spark_info is None:
            spark_info = SparkExecInfo()
        if spark_files is None:
            spark_files = SparkExecFiles()
        batch = self.client.create_batch(
            file=filename,
            class_name=class_name,
            args=args,
            proxy_user=None,  # self.connect_info.username,
            jars=spark_files.jars,
            py_files=spark_files.py_files,
            files=spark_files.files,
            driver_memory=spark_info.driver_memory,
            driver_cores=spark_info.driver_cores,
            executor_memory=spark_info.executor_memory,
            executor_cores=spark_info.executor_cores,
            num_executors=spark_info.num_executors,
            archives=spark_files.archives,
            queue=None,
            name=None,
            spark_conf=spark_info.spark_conf)
        if not progress_updater:
            progress_updater = BatchProgressUpdater()
        progress_updater.start(batch)
        intervals = livy.utils.polling_intervals([0.1, 0.5, 1.0, 2.0], 5.0)
        cancel = False
        while batch.state not in {
                livy.models.SessionState.ERROR, livy.models.SessionState.DEAD,
                livy.models.SessionState.KILLED,
                livy.models.SessionState.SUCCESS
        }:
            if cancel:
                break
            wait_interval = next(intervals)
            if cancel_event:
                cancel_event.wait(wait_interval)
                cancel = cancel_event.is_set()
            else:
                time.sleep(wait_interval)
            new_batch = self.client.get_batch(batch.batch_id)
            if not new_batch:
                raise InterruptedError(
                    f'A Livy batch was deleted while waiting for completion. '
                    f'Batch id: {batch.batch_id}, filename: {filename}, '
                    f'class name: {class_name}')
            batch = new_batch
            cancel = cancel or progress_updater.update(batch)
        progress_updater.ended(batch, cancel)
        if cancel:
            self.client.delete_batch(batch.batch_id)
            return None
        return batch

    def check_batch(self, filename: str, class_name: Optional[str],
                    batch: Optional[livy.models.Batch]):
        if batch is None:
            raise InterruptedError(
                f'Execution of batch for {filename} : {class_name} was cancelled'
            )
        if batch.state == livy.models.SessionState.SUCCESS:
            return
        # TODO(catalin): include some log maybe..
        raise RuntimeError(
            f'Execution of batch for {filename} : {class_name} was unsuccessful: '
            f'{batch.state}')

    def close_session(self):
        if self.session:
            self.logger.info('Closing session: %s', self.session_id)
            try:
                self.session.close()
                del self.session
            except Exception as e:  # pylint: disable=broad-except
                # Random stuff can happen and we don't want to get stuck,
                # so just eat it and forget it
                self.logger.error(
                    f'Failed to close livy session: {e} - skipping')
            self.session = None
            self.session_id = None

    @classmethod
    def connect(cls,
                name: Optional[str] = None,
                connect_info: Optional[LivyConnectInfo] = None,
                spark_info: Optional[SparkExecInfo] = None,
                spark_files: Optional[SparkExecFiles] = None,
                session_id: Optional[int] = None,
                use_last_session: bool = False,
                logger: Optional[logging.Logger] = None):
        if connect_info is None:
            connect_info = LivyConnectInfo()
        if spark_info is None:
            spark_info = SparkExecInfo()
        context = cls(connect_info, logger)
        context.init_session(name=name,
                             spark_info=spark_info,
                             spark_files=spark_files,
                             session_id=session_id,
                             use_last_session=use_last_session,
                             wait_connection=True)
        return context
