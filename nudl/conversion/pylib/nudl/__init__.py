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

Accompanying utility library for nudl python builtins.
"""
import collections.abc
import datetime
import decimal
import itertools
import pytz
import pytz.exceptions
import random
import time
import types
import typing

from nudl import dataproc

UTC_TIMEZONE: datetime.tzinfo = pytz.utc
NUDL_TIMEZONE: datetime.tzinfo = pytz.utc

Numeric = typing.Union[int, float, decimal.Decimal]
Date = datetime.date
DateTime = datetime.datetime
Timestamp = float
Decimal = decimal.Decimal

_ZERO_DATETIME: DateTime = datetime.datetime.fromtimestamp(0.0, UTC_TIMEZONE)
_ZERO_DATE: Date = _ZERO_DATETIME.date()

# With this it is confused:
# typing.Union[str, int, float,
#              datetime.date, datetime.datetime, decimal.Decimal]
_Sortable = typing.Any

_AnyIterable = collections.abc.Iterable[typing.Any]
_AnyOptional = typing.Optional[typing.Any]
_AnyList = typing.List[typing.Any]

_AnyMapper = collections.abc.Callable[[typing.Any], typing.Any]
_SortMapper = collections.abc.Callable[[typing.Any], _Sortable]
_HashMapper = collections.abc.Callable[[typing.Any], collections.abc.Hashable]

_HashAggregateTuple = typing.Tuple[_Sortable, collections.abc.Hashable]
_HashAggregateTupleIterable = typing.Iterable[_HashAggregateTuple]
_AggregateTuple = typing.Tuple[_Sortable, typing.Any]
_AggregateTupleIterable = typing.Iterable[_AggregateTuple]


def _pick_first(e: _AggregateTuple) -> _Sortable:
    return e[0]


def _pick_first_hash(e: _HashAggregateTuple) -> _Sortable:
    return e[0]


def _pick_second(e: _AggregateTuple) -> typing.Any:
    return e[1]


def _pick_second_hash(e: _HashAggregateTuple) -> collections.abc.Hashable:
    return e[1]


def _apply_kv(l: typing.Any, k: _SortMapper, v: _AnyMapper) -> _AggregateTuple:
    return (k(l), v(l))


def _apply_hash_kv(l: typing.Any, k: _SortMapper,
                   v: _HashMapper) -> _HashAggregateTuple:
    return (k(l), v(l))


def aggregate_to_array(
    l: _AnyIterable, key: _SortMapper, value: _AnyMapper
) -> typing.Iterable[typing.Tuple[_Sortable, typing.List[typing.Any]]]:

    def joiner(e: typing.Any) -> _AggregateTuple:
        return _apply_kv(e, key, value)

    for k, g in itertools.groupby(sorted(map(joiner, l), key=_pick_first),
                                  key=_pick_first):
        yield (k, list(map(_pick_second, g)))


def aggregate_to_set(
    l: _AnyIterable, key: _SortMapper, value: _HashMapper
) -> typing.Iterable[typing.Tuple[typing.Any,
                                  typing.Set[collections.abc.Hashable]]]:

    def joiner(e: typing.Any) -> _HashAggregateTuple:
        return _apply_hash_kv(e, key, value)

    for k, g in itertools.groupby(sorted(map(joiner, l), key=_pick_first_hash),
                                  key=_pick_first_hash):
        yield (k, set(map(_pick_second_hash, g)))


def safe_max(l: _AnyIterable) -> _AnyOptional:
    try:
        return max(l)
    except ValueError:
        return None


def safe_min(l: _AnyIterable) -> _AnyOptional:
    try:
        return min(l)
    except ValueError:
        return None


def max_by(l: _AnyIterable, f: _SortMapper) -> _AnyOptional:
    try:
        # mypy gives some error here - pretty bogus:
        return max(map(lambda e, f=f: (f(e), e), l))[1]
    except ValueError:
        return None


def min_by(l: _AnyIterable, f: _SortMapper) -> _AnyOptional:
    try:
        # mypy gives some error here - pretty bogus:
        return min(map(lambda e, f=f: (f(e), e), l))[1]
    except ValueError:
        return None


def sort(l: _AnyList, key: _SortMapper, reverse: bool) -> _AnyList:
    l.sort(key=key, reverse=reverse)
    return l


def front(l: _AnyIterable) -> _AnyOptional:
    return front(list(itertools.islice(l, 1)))


def shuffle(l: _AnyList) -> _AnyList:
    random.shuffle(l)
    return l


def to_int(x: str, default: int = 0) -> int:
    try:
        return int(x)
    except ValueError:
        return default


def set_timezone(s: str) -> bool:
    global NUDL_TIMEZONE
    try:
        NUDL_TIMEZONE = pytz.timezone(s)
        return True
    except pytz.exceptions.UnknownTimeZoneError:
        return False


def timestamp_now() -> Timestamp:
    return time.time()


def timestamp_ms(utc_timestamp_ms: int) -> Timestamp:
    return 0.001 * utc_timestamp_ms


def timestamp_sec(utc_timestamp_sec: float) -> Timestamp:
    return utc_timestamp_sec


def date_today() -> Date:
    return DateTime.fromtimestamp(time.time(), NUDL_TIMEZONE).date()


def date_safe(year: int, month: int, day: int) -> typing.Optional[Date]:
    try:
        return Date(year, month, day)
    except ValueError:
        return None


def date_from_isoformat(s: str) -> typing.Optional[Date]:
    try:
        return Date.fromisoformat(s)
    except ValueError:
        return None


def date_from_timestamp(ts: Timestamp) -> Date:
    return DateTime.fromtimestamp(ts, NUDL_TIMEZONE).date()


def datetime_now() -> DateTime:
    return DateTime.fromtimestamp(time.time(), NUDL_TIMEZONE)


def datetime_safe(year: int, month: int, day: int, hour: int, minute: int,
                  second: int) -> typing.Optional[DateTime]:
    try:
        return DateTime(year,
                        month,
                        day,
                        hour,
                        minute,
                        second,
                        tzinfo=NUDL_TIMEZONE)
    except ValueError:
        return None


def datetime_from_timestamp(ts: Timestamp) -> DateTime:
    return DateTime.fromtimestamp(ts, NUDL_TIMEZONE)


def datetime_from_isoformat(s: str) -> typing.Optional[DateTime]:
    try:
        return DateTime.fromisoformat(s)
    except ValueError:
        return None


def default_decimal() -> decimal.Decimal:
    return decimal.Decimal()


def default_date() -> Date:
    return _ZERO_DATE


def default_datetime() -> DateTime:
    return _ZERO_DATETIME


def _collect(obj: _AnyIterable) -> _AnyIterable:
    for o in obj:
        if isinstance(o, types.GeneratorType):
            for x in _collect(o):
                yield x
        else:
            yield o


def collect(obj: _AnyIterable) -> _AnyList:
    return list(_collect(obj))
