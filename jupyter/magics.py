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

from IPython import display, get_ipython
from IPython.core.display import HTML
from IPython.core.magic import (cell_magic, line_magic, line_cell_magic,
                                needs_local_scope)

import livy
from nuna_nudl.jupyter import livy_session


class IpythonDisplay:
    """Helper for writing string to magic output."""

    def __init__(self):
        self._ipython_shell = get_ipython()

    def display(self, to_display):
        display.display(to_display)

    def html(self, to_display):
        self.display(HTML(to_display))

    def stderr_flush(self):
        sys.stderr.flush()

    def stdout_flush(self):
        sys.stdout.flush()

    def write(self, msg):
        sys.stdout.write(msg)
        self.stdout_flush()

    def writeln(self, msg):
        self.write(f'{msg}\n')

    def send_error(self, error):
        sys.stderr.write(f'{error}\n')
        self.stderr_flush()


def handle_spark_exceptions(fun):
    """Helper to display meaningful message on spark exception."""

    @functools.wraps(fun)
    def wrapped(self, *args, **kwargs):
        try:
            out = fun(self, *args, **kwargs)
        except livy.models.SparkRuntimeError as e:
            tb = ''
            if e.traceback:
                tb = '\n' + '\n'.join(e.traceback)
            self.display.send_error(
                f'Spark Error encountered [{e.ename}]: {e.evalue}{tb}')
            return None
        else:
            return out

    return wrapped


class AsyncAction:
    """An asynchronous action started by a magic function."""

    def __init__(self, runner: Callable):
        self.cancel_event = threading.Event()
        self.thread = threading.Thread(target=runner)

    def _setup_progress(self, idisplay: IpythonDisplay,
                        context: livy_session.LivyExecutionContext,
                        finalize_progress: Callable):
        progress = ipywidgets.FloatProgress(min=0,
                                            max=1.0,
                                            description='Progress')
        cancel_button = ipywidgets.Button(description='Cancel')
        confirm_button = ipywidgets.Button(description='Confirm',
                                           button_style='danger',
                                           layout={'visibility': 'hidden'})
        done_button = ipywidgets.Button(description='Done',
                                        layout={'visibility': 'hidden'})
        output = ipywidgets.Output(layout={
            'height': '400px',
            'overflow': 'visible'
        })
        idisplay.display(
            ipywidgets.Box(
                [progress, cancel_button, confirm_button, done_button]))
        idisplay.display(output)

        def on_cancel(_):
            if confirm_button.layout.visibility == 'hidden':
                confirm_button.layout.visibility = 'visible'
            else:
                confirm_button.layout.visibility = 'hidden'

        cancel_button.on_click(on_cancel)

        def on_confirm(_):
            self.cancel_event.set()

        confirm_button.on_click(on_confirm)

        def done(state):
            confirm_button.layout.visibility = 'hidden'
            cancel_button.layout.visibility = 'hidden'
            progress.layout.visibility = 'hidden'
            done_button.description = state
            if state != 'SUCCESS':
                done_button.button_style = 'danger'
            else:
                done_button.button_style = 'success'
            done_button.layout.visibility = 'visible'

        return finalize_progress(context, output, progress, done)

    def start(self):
        self.thread.start()

    def wait(self):
        self.thread.join()


class StatementProgressLogger(livy_session.StatementProgressUpdater):
    """Logs progress of a statement run."""

    def __init__(self, context: livy_session.LivyExecutionContext,
                 output: ipywidgets.Output, progress: ipywidgets.FloatProgress,
                 done: Callable):
        super().__init__()
        self.context = context
        self.output = output
        self.progress = progress
        self.done = done
        self.progress_float = 0
        self.log_line = self.context.client.last_session_log_line(
            self.context.session_id)
        self.num_log_lines = 0

    def start(self, code: str, statement: livy.models.Statement):
        self.progress.value = 0
        self.output.append_stdout(f'>>> Starting:\n{code}')

    def update(self, statement: livy.models.Statement) -> bool:
        self.progress.value = statement.progress
        self.update_log(statement.session_id)
        return False

    def ended(self, statement: livy.models.Statement, canceled: bool):
        self.update_log(statement.session_id)
        if self.num_log_lines < 20:
            self.output.layout.height = f'{20*self.num_log_lines}px'
        if canceled:
            self.done('CANCELED')
        elif (statement.output and
              statement.output.status == livy.models.OutputStatus.OK):
            self.done('SUCCESS')
        else:
            self.done('ERROR')

    def update_log(self, session_id: int):
        log_lines = self.context.client.session_log(session_id, self.log_line,
                                                    50)
        self.log_line += len(log_lines)
        for log_line in _filter_log_lines(log_lines):
            self.num_log_lines += 1
            self.output.append_stdout(f'{log_line}\n')


class CurrentStatement(AsyncAction):
    """Wrapper for running a statement in a side thread."""

    def __init__(self, idisplay: IpythonDisplay,
                 context: livy_session.LivyExecutionContext, runner: Callable):
        super().__init__(runner)
        self.statement_progress = self._setup_progress(idisplay, context,
                                                       StatementProgressLogger)


@magics_class
class NudlMagics(Magics):
    """Implements our magics for nudl execution."""

    def __init__(self, shell):
        super().__init__(shell)
        schemas.set_base_schema_provider_builder(_get_schema_provider_builder())
        self.display = IpythonDisplay()
        self.livy_context = None
        self.current_statement = None
        self.shell.events.register('pre_run_cell', self.spark_wait)

    @magic_arguments()
    @argument('-last',
              '--last_session',
              type=bool,
              default=False,
              nargs='?',
              const=True,
              help='If true, we try to reconnect to the same session for '
              'the user.')
    @argument('-id',
              '--session_id',
              type=int,
              default=-1,
              help='Connect to an existing session (maybe opened in a previous '
              'run of your explorations. Use sparsely for testing.')
    @argument('-u',
              '--url',
              type=str,
              default='',
              help='Url of the livy instance to connect.')
    @argument('-dm',
              '--driver_memory',
              type=str,
              default=None,
              help='Driver memory to allocate for the session. Example `256m`')
    @argument('-dc',
              '--driver_cores',
              type=int,
              default=None,
              help='Driver number of cores to use.')
    @argument('-em',
              '--executor_memory',
              type=str,
              default=None,
              help='Executor memory to allocate for the session. Example `256m`'
             )
    @argument('-ec',
              '--executor_cores',
              type=int,
              default=None,
              help='Executor number of cores to use.')
    @argument('-ne',
              '--num_executors',
              type=int,
              default=None,
              help='Number of executors to launch for this session.')
    @argument('-hb',
              '--heartbeat_timeout',
              type=int,
              default=None,
              help='Heartbeat timeout for session in seconds.')
    @argument('-n',
              '--name',
              type=str,
              default=None,
              help='Name of the session.')
    @argument('-py',
              '--py_files',
              type=str,
              default=None,
              help='Location of the python zip file, including nudl core')
    @line_cell_magic
    def spark_local_connect(self, line, cell=None):
        args = parse_argstring(self.spark_local_connect, line)
        connect_info = livy_session.LivyConnectInfo(
            livy_url=args.url if args.url else 'http://localhost:8998/')
        spark_conf = None
        if cell:
            spark_conf = self._eval(cell)
            if not isinstance(spark_conf, dict):
                raise ValueError(
                    'Expecting cell that describes the spark config '
                    'to provide a dict. Found: {type(spark_conf)}')
        spark_info = livy_session.SparkExecInfo(
            driver_memory=args.driver_memory,
            driver_cores=args.driver_cores,
            executor_memory=args.executor_memory,
            executor_cores=args.executor_cores,
            num_executors=args.num_executors,
            spark_conf=spark_conf,
            heartbeat_timeout=args.heartbeat_timeout)
        spark_files = livy_session.SparkExecFiles(args.py_files)
        if (self.livy_context and
                self.livy_context.session_id != args.session_id):
            self.spark_disconnect('')
        self.livy_context = livy_session.LivyExecutionContext.connect(
            name=args.name,
            connect_info=connect_info,
            spark_info=spark_info,
            spark_files=spark_files,
            session_id=args.session_id,
            use_last_session=args.last_session,
            logger=none)
        self.livy_context.run_and_check_statement(
            "import nudl.dataset\n"
            "import nudl.datase_spark\n"
            "nudl.dataset.set_default_engine("
            "nudl.dataset_spark.SparkEngineImpl(spark))")

    @line_magic
    def spark_disconnect(self, line=None):
        """Disconnects from livy and closes the session.
        Good for releasing resources when done with a session."""
        del line
        if self.livy_context:
            self.display.writeln(
                f'Closing session id: {self.livy_context.session_id}')
            self.livy_context.close_session()
        del self.livy_context
        self.livy_context = None
        self._setup_runner()

    @line_magic
    def spark_wait(self, line=None):
        """Waits for any outstanding process to complete."""
        del line
        if self.current_statement:
            self.display.writeln('Waiting for outstanding statement...')
            self.current_statement.wait()
            del self.current_statement
            self.display.writeln('.. Done')
            self.current_statement = None
