"""
#
# Copyright 2022 Nuna inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

This is an implementation of dataset interface using bean.
"""
import apache_beam
import apache_beam.io.parquetio
import apache_beam.io.textio
import apache_beam.pvalue
import apache_beam.transforms.combiners
from apache_beam.pipeline import PipelineVisitor
from apache_beam.metrics.metric import Metrics
from apache_beam.options.pipeline_options import PipelineOptions
from apache_beam.runners.direct import direct_runner
from apache_beam.runners.interactive import interactive_runner
from apache_beam.runners.interactive import interactive_beam
from apache_beam.runners.interactive import interactive_environment

import collections.abc
import csv
import dataclasses
import dataclass_csv
import enum
import io
import logging
import nudl.dataset
import time
import typing


class BeamDataset:

    def __init__(self, step: nudl.dataset.DatasetStep,
                 pcollection: apache_beam.pvalue.PValue):
        self.step = step
        self.pcollection = pcollection
        logging.info("BeamDataset created for %s w/ %s", step, pcollection)


class _CsvConvert(apache_beam.DoFn):

    def __init__(self, seed: typing.Type[object]):
        # This comes from Beam codebase, for all DoFn constructors,
        # instead of super().__init__()
        # TODO(BEAM-6158): Revert the workaround.
        apache_beam.DoFn.__init__(self)
        self.num_parse_errors = Metrics.counter(
            self.__class__, f"num_csv_parse_errors_{seed.__name__}")
        self._seed = seed
        self._string_io = io.StringIO()
        self._reader = None
        self._fields = [field.name for field in dataclasses.fields(seed)]

    def process(self, elem):
        if not self._reader:
            self._reader = dataclass_csv.DataclassReader(
                self._string_io, self._seed)
        try:
            row = {
                field: val for field, val in zip(self._fields,
                                                 list(csv.reader([elem]))[0])
            }
            yield self._reader._process_row(row)
        except Exception:
            self.num_parse_errors.inc()
            logging.error("Parse error on '%s'", elem)


class _ParquetConvert(apache_beam.DoFn):

    def __init__(self, seed: type):
        # TODO(BEAM-6158): Revert the workaround once we can pickle super() on py3.
        apache_beam.DoFn.__init__(self)
        self.seed = seed

    def process(self, indict):
        yield self.seed(**indict)


class _Filter(apache_beam.DoFn):

    def __init__(self, fun: collections.abc.Callable[[typing.Any], bool]):
        # TODO(BEAM-6158): Revert the workaround once we can pickle super() on py3.
        apache_beam.DoFn.__init__(self)
        self.fun = fun

    def process(self, elem):
        if self.fun(elem):
            yield elem


class _FlatMap(apache_beam.DoFn):

    def __init__(self, fun: collections.abc.Callable[[typing.Any], typing.Any]):
        # TODO(BEAM-6158): Revert the workaround once we can pickle super() on py3.
        apache_beam.DoFn.__init__(self)
        self.fun = fun

    def process(self, elem):
        for result in self.fun(elem):
            yield result


def _aggregate_map(t: typing.Any, names: typing.List[str],
                   builder: collections.abc.Callable[[typing.Any],
                                                     typing.Tuple]):
    return apache_beam.Row(
        **{
            x: nudl.dataset.agg_value(y)
            for (x, y) in zip(names, nudl.dataset.agg_values(builder(t)))
        })


T = typing.TypeVar('T')


@apache_beam.typehints.with_input_types(T)
@apache_beam.typehints.with_output_types(typing.Set[T])
class CountDistinctCombineFn(apache_beam.CombineFn):
    """A Beam combiner which counts the distinct elements."""

    def create_accumulator(self):
        return set()

    def add_input(self, accumulator, element):
        accumulator.add(element)
        return accumulator

    def merge_accumulators(self, accumulators):
        return set.union(*accumulators)

    def extract_output(self, accumulator):
        return len(accumulator)


_AGGREGATE_MAP = {
    "count": (apache_beam.transforms.combiners.CountCombineFn, True),
    "min": (min, False),
    "max": (max, False),
    "sum": (sum, False),
    "mean": (apache_beam.transforms.combiners.MeanCombineFn, True),
    "to_set": (apache_beam.transforms.combiners.ToSetCombineFn, True),
    "to_array": (apache_beam.transforms.combiners.ToListCombineFn, True),
    "count_distinct": (CountDistinctCombineFn, True),
}


class _AggregateBuilder:
    """Builds structure, group by fields and aggregations."""

    def __init__(self, spec: typing.List[typing.Tuple[str, str]]):
        self.spec = spec
        self.field_names = []
        self.agg_spec = []
        self.group_names = []

    def build(self):
        struct_fields = []
        for (agg_type, field_name) in self.spec:
            self.field_names.append(field_name)
            if agg_type == "group_by":
                self.group_names.append(field_name)
            else:
                self.agg_spec.append((field_name, self._agg_fun(agg_type)))
        return self

    def _agg_fun(self, agg_type: str):
        if agg_type not in _AGGREGATE_MAP:
            raise ValueError(f"Unknown aggregate name ${agg_type}")
        return _AGGREGATE_MAP[agg_type]


class JoinKind(enum.Enum):
    NONE = 0
    LEFT = 1
    RIGHT_SINGLE = 2
    RIGHT_MULTI = 3
    RIGHT_MULTI_ARRAY = 4


@dataclasses.dataclass
class _JoinField:
    join_type: JoinKind = JoinKind.NONE
    field_name: typing.Optional[str] = None
    index_field_name: typing.Optional[str] = None
    index: int = 0


class _JoinProcess(apache_beam.DoFn):

    def __init__(self, fields: typing.List[_JoinField], seed: type, label: str):
        # TODO(BEAM-6158): Revert the workaround once we can pickle super() on py3.
        apache_beam.DoFn.__init__(self)
        self.seed = seed
        self.fields = fields
        self.num_missing_left = Metrics.counter(
            self.__class__, f"num_left_join_missing_left_{label}")
        self.num_multi_left = Metrics.counter(
            self.__class__, f"num_left_join_multiple_left_{label}")

    def process(self, elem):
        if not elem[1][0]:
            self.num_missing_left.inc()
        if len(elem[1][0]) > 1:
            self.num_multi_left.inc()
        for root in elem[1][0]:
            result = self.seed(*dataclasses.asdict(root))
            for crt, field in zip(elem[1][1:], self.fields[1:]):
                if not crt:
                    continue
                assert field.field_name is not None
                if field.join_type == JoinKind.RIGHT_SINGLE:
                    setattr(result, field.field_name, crt[0])
                elif field.join_type == JoinKind.RIGHT_MULTI:
                    getattr(result, field.field_name).extend(crt)
                elif field.join_type == JoinKind.RIGHT_MULTI_ARRAY:
                    getattr(result, field.field_name).extend(crt)
                    assert field.index_field_name is not None
                    getattr(result, field.index_field_name).extend(
                        [field.index] * len(crt))
            yield result


class BeamPipeline:

    def __init__(self,
                 collect_options: nudl.dataset.CollectOptions,
                 runner=None,
                 options=None):
        self.collect_options = collect_options
        self.pipeline = apache_beam.Pipeline(runner=runner, options=options)
        self.steps = {}

    def step_dataset(self, step: nudl.dataset.DatasetStep) -> BeamDataset:
        if step.step_id in self.steps:
            return self.steps[step.step_id]
        dataset = self._convert_step(step)
        self.steps[step.step_id] = dataset
        return dataset

    def _step_label(self, step: nudl.dataset.DatasetStep, info: str):
        return f"{step} - {step.step_id} - {info}"

    def _convert_step(self, step) -> BeamDataset:
        if isinstance(step, nudl.dataset.EmptyStep):
            return self._empty_dataset(step)
        elif isinstance(step, nudl.dataset.ReadCsvStep):
            return self._read_csv(step)
        elif isinstance(step, nudl.dataset.ReadParquetStep):
            return self._read_parquet(step)
        elif isinstance(step, nudl.dataset.FilterStep):
            return self._filter(step)
        elif isinstance(step, nudl.dataset.MapStep):
            return self._map(step)
        elif isinstance(step, nudl.dataset.FlatMapStep):
            return self._flat_map(step)
        elif isinstance(step, nudl.dataset.LimitStep):
            return self._limit(step)
        elif isinstance(step, nudl.dataset.AggregateStep):
            return self._aggregate(step)
        elif isinstance(step, nudl.dataset.JoinLeftStep):
            return self._join_left(step)
        raise ValueError(f"Unknown dataset step kind: {step}")

    def _empty_dataset(self, step: nudl.dataset.EmptyStep) -> BeamDataset:
        return BeamDataset(
            step, self.pipeline |
            self._step_label(step, "Create") >> apache_beam.Create([]))

    def _read_csv(self, step: nudl.dataset.ReadCsvStep) -> BeamDataset:
        # TODO(catalin): want to reshuffle ?
        return BeamDataset(
            step, self.pipeline | self._step_label(step, "Read") >>
            apache_beam.io.textio.ReadFromText(step.filespec,
                                               skip_header_lines=True) |
            self._step_label(step, "Convert") >> apache_beam.ParDo(
                _CsvConvert(step.seed_type())))

    def _read_parquet(self, step: nudl.dataset.ReadParquetStep) -> BeamDataset:
        # TODO(catalin): no filter support in this reader, may want to define
        #   another reader w/ proper pushdown filter support.
        # TODO(catalin): want to reshuffle ?
        used_fields = None
        if not self.collect_options.disable_column_prunning:
            used_fields = step.used_fields()
        if used_fields is not None:
            logging.info("Restricting Parquet read %s columns to: %s", step,
                         used_fields)
        return BeamDataset(
            step, self.pipeline | self._step_label(step, "Read") >>
            apache_beam.io.parquetio.ReadFromParquet(step.filespec,
                                                     columns=used_fields) |
            self._step_label(step, "Convert") >> apache_beam.ParDo(
                _ParquetConvert(step.seed_type())))

    def _filter(self, step: nudl.dataset.FilterStep) -> BeamDataset:
        assert step.source is not None
        src = self.step_dataset(step.source)
        return BeamDataset(
            step, src.pcollection | self._step_label(step, "Filtering") >>
            apache_beam.ParDo(_Filter(step.fun)))

    def _map(self, step: nudl.dataset.MapStep) -> BeamDataset:
        assert step.source is not None
        src = self.step_dataset(step.source)
        # TODO(catalin): want to reshuffle ?
        return BeamDataset(
            step, src.pcollection |
            self._step_label(step, "Map") >> apache_beam.Map(step.fun))

    def _flat_map(self, step: nudl.dataset.FlatMapStep) -> BeamDataset:
        assert step.source is not None
        src = self.step_dataset(step.source)
        # TODO(catalin): want to reshuffle ?
        return BeamDataset(
            step, src.pcollection | self._step_label(step, "FlatMap") >>
            apache_beam.ParDo(_FlatMap(step.fun)))

    def _limit(self, step: nudl.dataset.LimitStep) -> BeamDataset:
        assert step.source is not None
        src = self.step_dataset(step.source)
        return BeamDataset(
            step,
            src.pcollection | self._step_label(step, f"Limit {step.limit}") >>
            apache_beam.transforms.combiners.Sample.FixedSizeGlobally(
                step.limit))

    def _aggregate(self, step: nudl.dataset.AggregateStep) -> BeamDataset:
        assert step.source is not None
        src = self.step_dataset(step.source)
        agg_builder = _AggregateBuilder(step.build_spec()).build()
        spec = apache_beam.GroupBy(*agg_builder.group_names)
        for (field_name, (agg, is_class)) in agg_builder.agg_spec:
            spec = spec.aggregate_field(field_name,
                                        agg() if is_class else agg, field_name)
        mapper = (lambda t, names=agg_builder.field_names, builder=step.builder:
                  _aggregate_map(t, names, builder))
        return BeamDataset(
            step, src.pcollection |
            self._step_label(step, "GroupBy Map") >> apache_beam.Map(mapper) |
            self._step_label(step, "GroupBy + Aggregate") >> spec)

    def _join_left(self, step: nudl.dataset.JoinLeftStep) -> BeamDataset:
        assert step.source is not None
        left_src = self.step_dataset(step.source)
        right_join_fields = step.right_join_fields(
            self.collect_options.disable_column_prunning)
        if not step.right_spec or not right_join_fields:
            logging.info("Skiping join left for %s, per no joins needed", step)
            return left_src
        fields = []
        pcollections = []
        left_join = (
            left_src.pcollection | self._step_label(step, "Prejoin map Left") >>
            apache_beam.Map(lambda l, left_key=step.left_key: (left_key(l), l)))
        fields.append(_JoinField(join_type=JoinKind.LEFT))
        pcollections.append(left_join)
        for right in step.right_spec:
            spec = nudl.dataset.JoinSpecDecoder(right)
            if spec.field_name not in right_join_fields:
                logging.info("In %s, skiping join column: %s as it is unused",
                             step, spec.field_name)
                continue
            label = self._step_label(
                step, f"Prejoin map {spec.join_spec} {spec.field_name}")
            if spec.join_spec == "right_multi_array":
                if not spec.spec_src:
                    continue
                index_field_name = f"{spec.field_name}_index"
                index = 0
                for src in spec.spec_src:
                    right_src = self.step_dataset(src)
                    crt_label = f"{label} - {index}"
                    right_join = (
                        right_src.pcollection | crt_label >>
                        apache_beam.Map(lambda r, right_key=spec.spec_key:
                                        (right_key(r), r)))
                    fields.append(
                        _JoinField(join_type=JoinKind.RIGHT_MULTI_ARRAY,
                                   field_name=spec.field_name,
                                   index_field_name=index_field_name,
                                   index=index))
                    pcollections.append(right_join)
                    index += 1
            else:
                right_src = self.step_dataset(spec.spec_src)
                right_join = (
                    right_src.pcollection |
                    label >> apache_beam.Map(lambda r, right_key=spec.spec_key:
                                             (right_key(r), r)))
                pcollections.append(right_join)
                if spec.join_spec == "right_multi":
                    fields.append(
                        _JoinField(join_type=JoinKind.RIGHT_MULTI,
                                   field_name=spec.field_name))
                else:
                    fields.append(
                        _JoinField(join_type=JoinKind.RIGHT_SINGLE,
                                   field_name=spec.field_name))
        return BeamDataset(
            step, pcollections |
            self._step_label(step, "Merging") >> apache_beam.CoGroupByKey() |
            self._step_label(step, "Extraction") >> apache_beam.ParDo(
                _JoinProcess(fields, step.seed_type(),
                             f"join_process_{step.step_id}")))


class BeamEngineImpl(nudl.dataset.DatasetEngine):

    def __init__(self, runner=None, options=None):
        super().__init__("Beam")
        self.runner = runner
        self.options = options

    def collect(
            self, step: nudl.dataset.DatasetStep,
            options: nudl.dataset.CollectOptions) -> typing.List[typing.Any]:
        if not isinstance(self.runner, interactive_runner.InteractiveRunner):
            raise ValueError(
                "Cannot collect(..) on non interactive beam engine")
        pipeline = BeamPipeline(options, self.runner, self.options)
        collector = nudl.dataset.FieldUsageCollector()
        step.update_field_usage(collector, True)
        interactive_beam.watch({'pipeline': pipeline.pipeline})
        dataset = pipeline.step_dataset(step)
        interactive_environment.current_env().track_user_pipelines()
        interactive_beam.watch({'pcollection': dataset.pcollection})
        result = pipeline.pipeline.run()
        assert isinstance(result, interactive_runner.PipelineResult)
        result.wait_until_finish()
        return list(result.get(dataset.pcollection))


def default_local_runner():
    return interactive_runner.InteractiveRunner(direct_runner.DirectRunner())
