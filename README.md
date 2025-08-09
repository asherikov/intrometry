Intrometry
==========

<table>
  <tr>
    <td align="center">
        CI status
    </td>
  </tr>
  <tr>
    <td align="center">
        <a href="https://github.com/asherikov/intrometry/actions/workflows/main.yml">
        <img src="https://github.com/asherikov/intrometry/actions/workflows/main.yml/badge.svg" alt="Build Status">
        </a>
    </td>
  </tr>
</table>


Introduction
------------

Telemetry collection utility that addresses the same problem as
<https://github.com/pal-robotics/pal_statistics> and
<https://github.com/PickNikRobotics/data_tamer/>, but in a different way. The
main API difference is that `intrometry` relies on `ariles` serialization
library <https://github.com/asherikov/ariles> instead of macro, and keeps track
of updated data instead of taking global snapshots.


Design
------

- Intrometry consist of multiple modules: one frontend and multiple backends.
  Frontend defines common API, and backends implement various storage options.
  Typically application code should depend on a single backend, explicit
  dependency on the frontend is not needed in this case.

- `intrometry` tries to interfere as little as possible with the execution of
  other code, in particular this means that:
    - `intrometry` would rather lose data than block calling thread for more
      time than is needed for copying it;
    - `intrometry` would rather lose data than run out of memory;
    - `intrometry` ignores failures, e.g., due to failed initialization, which
      allows to disable it by simply providing an empty id.

- Frontend is meant to be agnostic of any possible data storage or transmission
  dependencies, in particular ROS. Backends may depend on various libraries in
  order to handle data.


Comparison
----------

### `pal_statistics`
```
double my_variable = 3.0;
REGISTER_VARIABLE(node, "/statistics_topic", "my_variable_name", &my_variable);
...
for(...)
{
    PUBLISH_STATISTICS(node, "/statistics_topic");
}
...
UNREGISTER_VARIABLE(node, "/statistics_topic", "my_variable_name");
```

### `intrometry`
```
class ArilesDebug : public ariles2::DefaultBase
{
#define ARILES2_DEFAULT_ID "ArilesDebug"
#define ARILES2_ENTRIES(v)                              \
    ARILES2_TYPED_ENTRY_(v, duration, double)           \
    ARILES2_TYPED_ENTRY_(v, size, std::size_t)          \
    ARILES2_TYPED_ENTRY_(v, vec, std::vector<float>)
#include ARILES2_INITIALIZE
}
...
intrometry::pjmsg_topic::Sink sink("my_sink");
ArilesDebug debug;
...
sink.initialize();
sink.assign(debug);
...
for(...)
{
    sink.write(debug);
}
sink.retract(debug);
```

### Examples of a published messages

`plotjuggler_msgs/msg/statistics_names`:

```
header:
  stamp:
    sec: XXX
    nanosec: XXX
  frame_id: ""
names:
- "ArilesDebug.duration"
- "ArilesDebug.size"
- "ArilesDebug.vec_0"
- "ArilesDebug.vec_1"
- "ArilesDebug.vec_2"
names_version: 1852399462
```

`plotjuggler_msgs/msg/statistics_values`:

```
header:
  stamp:
    sec: XXX
    nanosec: XXX
  frame_id: ""
values:
- 0.00000
- 0.00000
- 3.40000
- 2.20000
- 2.10000
names_version: 1852399462
```

### Key features

- `intrometry` is less verbose when working with a large number of metrics due
  to their grouping in classes, e.g., there is no need to explicitly specify
  topic and node;
- `intrometry` avoids potential memory violations due to API misuse by not
  using pointers to variables;
- `intrometry` hides implementation details from the user, so there is no need
  to manually control publishing threads as in
  <http://wiki.ros.org/pal_statistics/Tutorials/Registering%20and%20publishing%20variables#Real_Time_usage_example>;
- automatic generation of variable names and support for various types suchs as
  `Eigen` matrices, stl vectors, maps, etc is provided by `ariles`.

### Methods

- `initialize()`, `assign()`, and `retract()` methods are "heavy" and are meant
  to be used sparingly.
- `write()` is a "light" method that should be suitable for soft real time
  applications.


Backends
--------

### `pjmsg_topic`

Creates a dedicated ROS2 node and spawns a publishing thread that takes care of
sending data using `plotjuggler_msgs` at a given frequency. Recorded ROS bags
can be viewed with `PlotJuggler` <https://plotjuggler.io/>. Keep in mind that
`PlotJuggler` has a flaw that may result in a collision of metric names
<https://github.com/facontidavide/PlotJuggler/pull/339> -- `intrometry` makes
an effort to avoid this, but it is still possible.

### `pjmsg_mcap`

Serializes metrics to `plotjuggler_msgs` and writes them directly to `mcap`
files. All serialization logic and schemas are compiled in, so this backend
does NOT depend on any ROS components. The resulting files can also be viewed
by `PlotJuggler`.


Using library
-------------

### cmake

```
find_package(intrometry_<BACKEND> REQUIRED)
target_link_libraries(my_library intrometry::<BACKEND>)
```

### C++

```
#include <intrometry/<BACKEND>/all.h>
```


Dependencies
------------

### Frontend
- `C++17` compatible compiler
- `cmake`
- `ariles` (`ariles2_core_ws`) <https://github.com/asherikov/ariles/tree/pkg_ws_2>

### Backends

`pjmsg_topic`

- `thread_supervisor` <https://github.com/asherikov/thread_supervisor>
- `rclcpp` / `plotjuggler_msgs`
- `ariles` (`ariles2_namevalue2_ws`) <https://github.com/asherikov/ariles/tree/pkg_ws_2>

`pjmsg_mcap`

- `thread_supervisor` <https://github.com/asherikov/thread_supervisor>
- `ariles` (`ariles2_namevalue2_ws`) <https://github.com/asherikov/ariles/tree/pkg_ws_2>


TODO
====

- Do data preprocessing in the sink thread, e.g., with `ariles` finalize
  visitor. That would require locking of the data, one option is to keep two
  copies of the class and swapping them.
