"""Tests livy_session."""

import unittest
import livy
import threading

from nuna_nudl.jupyter import livy_test_server as lts
from nuna_nudl.jupyter import livy_session


class Updater(livy_session.StatementProgressUpdater):
    """Statement progress recording for test."""

    def __init__(self):
        self.started = False
        self.updated = 0
        self.end = False
        self.canceled = None

    def start(self, code: str, statement: livy.models.Statement):
        del code, statement
        self.started = True

    def update(self, statement: livy.models.Statement) -> bool:
        del statement
        self.updated += 1
        return False

    def ended(self, statement: livy.models.Statement, canceled: bool):
        del statement
        self.end = True
        self.canceled = canceled


class BatchUpdater(livy_session.BatchProgressUpdater):
    """Batch progress recording for test."""

    def __init__(self):
        self.started = False
        self.updated = 0
        self.end = False
        self.canceled = None

    def start(self, batch: livy.models.Batch):
        del batch
        self.started = True

    def update(self, batch: livy.models.Batch) -> bool:
        del batch
        self.updated += 1
        return False

    def ended(self, batch: livy.models.Batch, canceled: bool):
        del batch
        self.end = True
        self.canceled = canceled


class LivySessionTest(unittest.TestCase):

    def testUtils(self):
        self.assertEqual(
            livy_session._rekey(  # pylint: disable=protected-access
                {
                    'foo': 1,
                    'bar': 2,
                    'baz': 3
                }, 'bar', 'bar_'),
            {
                'foo': 1,
                'bar_': 2,
                'baz': 3
            })
        with self.assertRaises(InterruptedError):
            livy_session.check_livy_statement(None, 'Bad')
        stmt = livy.models.Statement(session_id=0,
                                     statement_id=1,
                                     state=livy.models.StatementState.AVAILABLE,
                                     code='',
                                     output=livy.models.Output(
                                         status=livy.models.OutputStatus.ERROR,
                                         text=None,
                                         json=None,
                                         ename='Foo',
                                         evalue='FooError',
                                         traceback=['a', 'b']),
                                     progress=1.0)
        with self.assertRaises(livy.models.SparkRuntimeError):
            livy_session.check_livy_statement(stmt, 'Bad')
        stmt.output = livy.models.Output(status=livy.models.OutputStatus.OK,
                                         text=None,
                                         json=None,
                                         ename=None,
                                         evalue=None,
                                         traceback=None)
        with self.assertRaises(RuntimeError):
            livy_session.check_livy_statement(stmt, 'Bad')
        livy_session.check_livy_statement(stmt, 'Bad', ok_no_output=True)

    def testSession(self):
        """A statement has ended."""
        st, server, port = lts.start_test_server()
        try:
            info = livy_session.LivyConnectInfo(
                livy_url=f'http://localhost:{port}/')
            info.username = 'foouser'
            # Connecting
            context = livy_session.LivyExecutionContext.connect(
                name='foo',
                connect_info=info,
                spark_info=livy_session.SparkExecInfo(
                    driver_memory='128m',
                    driver_cores=1,
                    executor_memory='140m',
                    executor_cores=2,
                    num_executors=1,
                    spark_conf={'spark.logConf': 'true'}))
            self.assertEqual(context.session_id, 0)
            self.assertEqual(context.session.session_id, 0)

            # Session / modules / spark Check:
            context.check_session()
            self.assertTrue(context.has_idle_session())
            self.assertEqual(context.ensure_spark_context_var(), 'spark')
            self.assertTrue(context.verify_module('os'))

            # Statements:
            updater = Updater()
            stmt = context.execute_statement('print("Foo Bar")',
                                             kind='py',
                                             progress_updater=updater,
                                             cancel_event=None)
            context.check_statement_ok(stmt, 'Bad')
            self.assertTrue(updater.started)
            self.assertGreater(updater.updated, 0)
            self.assertTrue(updater.end)
            self.assertFalse(updater.canceled)
            self.assertEqual(stmt.output.text, 'Foo Bar\n')

            # Canceled Statements:
            event = threading.Event()
            event.set()
            updater = Updater()
            stmt = context.execute_statement('print("Foo Bar")',
                                             kind='py',
                                             progress_updater=updater,
                                             cancel_event=event)
            self.assertTrue(stmt is None)
            self.assertTrue(updater.started)
            self.assertEqual(updater.updated, 0)
            self.assertTrue(updater.end)
            self.assertTrue(updater.canceled)

            # Batch: here we assume our test server does not actually
            #  execute batches..
            updater = BatchUpdater()
            batch = context.execute_batch(filename='_do_not_execute_',
                                          class_name='bar',
                                          args=['1', '2'],
                                          progress_updater=updater)
            context.check_batch('foo', 'bar', batch)
            self.assertTrue(updater.started)
            self.assertGreater(updater.updated, 0)
            self.assertTrue(updater.end)
            self.assertFalse(updater.canceled)

            event = threading.Event()
            updater = BatchUpdater()
            event.set()
            batch = context.execute_batch(filename='_do_not_execute_',
                                          class_name='bar',
                                          args=['1', '2'],
                                          progress_updater=updater,
                                          cancel_event=event)
            self.assertTrue(batch is None)
            self.assertTrue(updater.started)
            self.assertEqual(updater.updated, 0)
            self.assertTrue(updater.end)
            self.assertTrue(updater.canceled)

            # build_sql_code
            self.assertEqual(
                context.build_sql_code('Foo Bar', None),
                'for nudl_livy_internal_reserved in '
                'spark.sql("""Foo Bar""").toJSON().collect():\n'
                '    print(nudl_livy_internal_reserved)')
            self.assertEqual(
                context.build_sql_code(
                    'Foo Bar',
                    livy_session.SqlCommandArgs(samplemethod='sample',
                                                samplefraction=0.1,
                                                maxrows=10)),
                'for nudl_livy_internal_reserved in '
                'spark.sql("""Foo Bar""").toJSON().sample(False, 0.1).take(10):\n'
                '    print(nudl_livy_internal_reserved)')

            # Connecting to same session / user:
            context2 = livy_session.LivyExecutionContext.connect(
                connect_info=info, session_id=context.session_id)
            self.assertEqual(context2.session_id, 0)
            context3 = livy_session.LivyExecutionContext.connect(
                connect_info=info, use_last_session=True)
            self.assertEqual(context3.session_id, 0)

            # Closing the session
            context.close_session()
        finally:
            server.shutdown()
            st.join()
        print(f'Requests: {server.requests_log}')
        calls = [f'{req.method}-{req.path}' for req in server.requests_log]
        print(calls)
        self.assertEqual(
            calls,
            [
                'GET-/version',
                # First connect
                'POST-/sessions',
                'GET-/sessions/0',
                'GET-/sessions/0',
                # has_idle_session
                'GET-/sessions/0',
                # ensure_spark_context_var
                'POST-/sessions/0/statements',
                'GET-/sessions/0/statements/0',
                # verify_module
                'POST-/sessions/0/statements',
                'GET-/sessions/0/statements/1',
                # execute_statement
                'POST-/sessions/0/statements',
                'GET-/sessions/0/statements/2',
                # execute_statement
                'POST-/sessions/0/statements',
                'GET-/sessions/0/statements/3',
                # execute_batch
                'POST-/batches',
                'GET-/batches/0',
                # execute_batch => cancel
                'POST-/batches',
                'GET-/batches/1',
                'DELETE-/batches/1',
                # 2nd connect
                'GET-/sessions/0',
                'GET-/sessions/0',
                # List sessions for 3rd connect
                'GET-/sessions',
                # 3rd connect
                'GET-/sessions/0',
                'GET-/sessions/0',
                # close_session
                'DELETE-/sessions/0'
            ])
        # Ensure first connect is ok param-wise
        self.assertEqual(
            server.requests_log[1].req,
            lts.PostSessionReq(kind='sql',
                               proxyUser='foouser',
                               jars=None,
                               pyFiles=None,
                               files=None,
                               driverMemory='128m',
                               driverCores=1,
                               executorMemory='140m',
                               executorCores=2,
                               numExecutors=1,
                               archives=None,
                               queue=None,
                               name='foo',
                               conf={'spark.logConf': 'true'},
                               heartbeatTimeoutInSecond=None))


if __name__ == '__main__':
    unittest.main()
