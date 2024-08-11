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

ROS2 telemetry collection utility that addresses the same problem as
<https://github.com/pal-robotics/pal_statistics>, but in a different way. The
main API difference is that `intrometry` relies on `ariles` serialization
library <https://github.com/asherikov/ariles> instead of macro.

Doxygen documentation: <https://asherikov.github.io/intrometry/doxygen/group__API.html>


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
intrometry::Publisher publisher;
ArilesDebug debug;
...
publisher.initialize("my_publisher");
publisher.assign(debug);
...
for(...)
{
    publisher.write(debug);
}
publisher.retract(debug);
```

Example of a published message (`pal_statistics_msgs/msg/statistics_names`)
```
header:
  stamp:
    sec: 0
    nanosec: 0
  frame_id: ""
names:
- "ArilesDebug.duration"
- "ArilesDebug.size"
- "ArilesDebug.vec_0"
- "ArilesDebug.vec_1"
- "ArilesDebug.vec_2"
```

Key features:

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


Dependencies
------------

- `C++17` compatible compiler
- `cmake`
- `ariles` (`ariles2_namevalue2_ws`) <https://github.com/asherikov/ariles/tree/pkg_ws_2>
- `thread_supervisor` <https://github.com/asherikov/thread_supervisor>
- `rclcpp` / `pal_statistics_msgs`


Design
------

- Publisher creates a dedicated ROS2 node and spawns a publishing thread that
  takes care of sending data using `pal_statistics_msgs` at a given frequency.
  Consequently, data can be viewed with `PlotJuggler` <https://plotjuggler.io/>
  as if it was sent by `pal_statistics` package. Keep in mind that
  `PlotJuggler` has a flaw that may result in a collision of metric names
  <https://github.com/facontidavide/PlotJuggler/pull/339> -- `intrometry` makes
  an effort to avoid this, but it is still possible.

- `intrometry` tries to interfere as little as possible with the execution of
  other code, in particular this means that:
    - `intrometry` would rather lose data than block calling thread for more
      time than is needed for copying it;
    - `intrometry` ignores failures, e.g., due to failed initialization, which
      allows to disable it by simply providing an empty id.

- API:
    - Public API is ROS2 / `pal_statistics_msgs` agnostic, other backends may
      be implemented in the future.
    - `initialize()`, `assign()`, and `retract()` methods are "heavy" and are
      meant to be used sparingly.
    - `write()` is a "light" method that should be suitable for soft real time
      applications.

TODO
====

- Do data preprocessing in the publisher thread, e.g., with `ariles` finalize
  visitor. That would require locking of the data, one option is to keep two
  copies of the class and swapping them.
