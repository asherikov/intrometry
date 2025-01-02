```
from mcap.reader import make_reader

with open('filename', "rb") as f:
    reader = make_reader(f)
    for schema, channel, message in reader.iter_messages(topics=["sample_topic"]):
        print(f"{channel.topic} ({schema.name}): {message.data}")
```
