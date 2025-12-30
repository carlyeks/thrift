# Thrift Stream Support (THRIFT-1948)

## Overview

This document describes the stream support feature added to Apache Thrift, which allows for sending chunks of data between the server and the client without having the whole message in memory at the start of the communication.

## Motivation

Stream support enables several important use cases:

1. **Server-side streaming**: Server can push updates to clients as they occur, rather than requiring constant polling
2. **Client-side streaming**: Client can send large amounts of data incrementally without buffering everything in memory
3. **Bidirectional streaming**: Both client and server can send streams of data concurrently
4. **Memory efficiency**: Large datasets can be processed incrementally rather than buffered entirely

## IDL Syntax

The `stream<T>` keyword is used to declare streaming parameters and return types:

```thrift
service LockService {
  // Server streaming: returns a stream of updates
  stream<Update> updates_for(1: string prefix)
}

service QueryProvider {
  // Server streaming: run query and stream results as they come in
  stream<Result> run_query(1: string query)
}

service RandomNumberService {
  // Bidirectional streaming: both sides stream data
  stream<i64> random_numbers(1: stream<i64> max)
}

service DataAggregator {
  // Client streaming: client sends stream, server returns single result
  i64 sum_values(1: stream<i64> values)
}
```

## Stream Primitives

Streams have three fundamental operations encoded in the `has_more` byte:

1. **next (has_more=1)**: Send the next value of type T in the stream
2. **end (has_more=0)**: Indicate successful stream completion and close the stream
3. **error (has_more=2)**: Send an exception and close the stream

Wire encoding:
```
// Normal element
writeByte(1)      // has_more = NEXT
writeElement(elem)

// End of stream
writeByte(0)      // has_more = END

// Error during stream
writeByte(2)      // has_more = ERROR
writeException(ex)
```

The `TStreamMessageType` enum values map to has_more byte values for error handling.

## Protocol Enhancements

### Stream Message Types

Three message types for stream communication:

- `T_STREAM_NEXT` (1): The message contains a stream element
- `T_STREAM_ERROR` (2): The message contains an exception; stream is now closed
- `T_STREAM_END` (3): The stream has finished successfully; stream is now closed

### Protocol Methods

Write methods:
- `writeStreamBegin(etype, streamid)`: Begin writing a stream
- `writeStreamNext(streamid, streamMessageType)`: Write a stream message header
- `writeStreamNextEnd()`: Complete writing a stream element
- `writeStreamErrorEnd()`: Complete writing a stream error

Read methods:
- `readStreamBegin() -> (etype, streamid)`: Begin reading a stream
- `readStreamNext() -> (streamid, streamMessageType)`: Read stream message header
- `readStreamNextEnd()`: Complete reading a stream element
- `readStreamErrorEnd()`: Complete reading a stream error

### Wire Protocol Example

Streams use a dedicated `T_STREAM` wire type with structured framing:

**Writing a stream (stream-aware code):**
```
// Field with stream return type
writeFieldBegin("result", T_STREAM, 0)  // T_STREAM (17)
writeStreamBegin(T_I64)                 // Element type
// Element 1
writeByte(T_STREAM_NEXT)                // has_more = NEXT (1)
writeI64(42)
// Element 2
writeByte(T_STREAM_NEXT)                // has_more = NEXT (1)
writeI64(43)
// ... time passes, more elements generated asynchronously ...
// Element N
writeByte(T_STREAM_NEXT)                // has_more = NEXT (1)
writeI64(44)
// End of stream
writeByte(T_STREAM_END)                 // has_more = END (0)
writeStreamEnd()
writeFieldEnd()
```

**Reading a stream (stream-aware code):**
```
name, fieldType, fieldId = readFieldBegin()
if (fieldType == T_STREAM) {
  elemType = readStreamBegin()
  while (true) {
    has_more = readByte()
    if (has_more == T_STREAM_END) break
    if (has_more == T_STREAM_ERROR) {
      readException()
      throw
    }
    elem = readI64()
    processElement(elem)  // Process incrementally, no buffering!
  }
  readStreamEnd()
}
readFieldEnd()
```

**Old client (doesn't support streams):**
```
name, fieldType, fieldId = readFieldBegin()
if (fieldType == T_STREAM) {
  // Unknown type - skip it!
  skip(T_STREAM)  // Uses protocol's skip() to safely skip unknown data
  // Returns: missing field (like optional field not set)
}
readFieldEnd()
// Graceful degradation: stream field is simply not present
```

This approach means:
- **Wire format**: Uses `T_STREAM` (17) - new, explicit type
- **Element framing**: Each element prefixed with `has_more` byte
- **Non-consecutive**: writeStreamBegin and writeStreamEnd can be far apart
- **Clean failure**: Old clients skip unknown type gracefully
- **Explicit contract**: Requires client/server upgrade for stream support

## Implementation Status

### Completed

- ✅ IDL grammar support for `stream<T>` syntax
- ✅ Compiler lexer and parser modifications
- ✅ `t_stream` type class for representing streams in the compiler
- ✅ `T_STREAM` added to TType enumeration
- ✅ `TStreamMessageType` enumeration defined
- ✅ Example `.thrift` files demonstrating various streaming patterns

### In Progress

- 🔄 Protocol layer implementation (TProtocol base class)
- 🔄 Documentation

### Pending

- ⏳ Protocol implementation for specific protocols (Binary, Compact, JSON)
- ⏳ Code generator updates for different language backends
- ⏳ Runtime library support (C++, Java, Python, etc.)
- ⏳ Test suite for streaming functionality
- ⏳ Performance benchmarks

## Backwards Compatibility

### Protocol-Level Compatibility

**Key Design Decision**: Streams use `T_STREAM` (17) as a dedicated wire type. This is an **explicit breaking change** that requires coordinated client/server upgrades.

**Why not fake backward compatibility?**
- Using `size=-1` in list format causes old clients to crash (try to allocate 4GB)
- If we're going to break, better to fail cleanly than crash
- `T_STREAM` can be properly skipped by old clients using `skip()`
- Clear, explicit protocol version requirement

### Compatibility Matrix

| Client | Server | Behavior |
|--------|--------|----------|
| Stream-aware | Stream-aware | ✅ Full streaming support |
| Stream-aware | Old (list-based) | ✅ Reads lists normally |
| Old (list-based) | Stream-aware | ⚠️ Skips stream fields (graceful degradation) |
| Old (list-based) | Old (list-based) | ✅ Normal operation |

### Graceful Degradation for Old Clients

When an old client encounters `T_STREAM`:
1. Protocol's `skip()` function handles unknown type
2. Stream field is treated as missing (like unset optional field)
3. No crash, no OOM, just missing data
4. Clear error if stream is required field

**Example**:
```cpp
// Old client reading response with stream<Result>
readFieldBegin()  // type=T_STREAM (17)
// Unknown type! Use skip()
skip(T_STREAM)    // Safely skips all stream data
readFieldEnd()
// Returns: field not set (graceful degradation)
```

### Migration Path

**Phase 1: Code Generation Only**
- Generators that don't support streams use `get_effective_type()`
- IDL `stream<T>` → generated code uses `List<T>`
- Wire protocol still uses `T_LIST`
- No protocol changes yet

**Phase 2: Stream-Aware Services**
- Services declare stream support capability
- Clients negotiate whether to use streams
- Stream-capable pairs use `T_STREAM`, others use `T_LIST`

**Phase 3: Full Migration**
- All clients/servers upgraded
- Can use `stream<T>` everywhere
- Optimal performance and memory efficiency

### Native Stream Support

Languages with stream runtime support will:
- Use `T_STREAM` wire type (17)
- Implement protocol methods: `writeStreamBegin/End`, `readStreamBegin/End`
- Provide stream-based APIs (iterators, async generators, etc.)
- Process data incrementally without buffering
- Support proper error handling mid-stream

### Code Generator Integration

Code generators use helper methods to handle stream/list fallback:

```cpp
// In a code generator class

// Override to indicate stream support
virtual bool supports_streams() const override {
  return false;  // or true if this language has stream support
}

// When generating type references
void generate_field_type(t_field* field) {
  t_type* effective_type = get_effective_type(field->get_type());
  // effective_type will be list<T> if original was stream<T> and !supports_streams()
  // Generate code using effective_type
}

// When generating read/write code
void generate_read_field(t_field* field) {
  if (is_effective_stream(field->get_type())) {
    // Generate stream reading code (only if supports_streams() == true)
    generate_read_stream();
  } else if (field->get_type()->get_true_type()->is_list() ||
             field->get_type()->get_true_type()->is_stream()) {
    // Generate list reading code (also for streams when !supports_streams())
    generate_read_list();
  }
}
```

This ensures that:
- Languages without stream support automatically fall back to list behavior
- The wire protocol matches the generated code (T_LIST vs T_STREAM)
- No manual translation is required in each generator

## Examples

See `/test/StreamTest.thrift` for comprehensive examples of:
- Server streaming (LockService, QueryProvider)
- Client streaming (DataAggregator)
- Bidirectional streaming (RandomNumberService)

## Error Handling

When an exception is thrown during stream materialization:
- The `error()` primitive is called with the exception
- The stream is immediately closed
- The exception should be one declared in the service's `throws` clause
- All values generated before the exception remain valid, though their meaning may depend on the stream being completed

## Stream Lifecycle

1. Stream is opened with `writeStreamBegin` / `readStreamBegin`
2. Zero or more elements are sent with `next` messages
3. Stream is closed with either:
   - `end` message (successful completion)
   - `error` message (exceptional termination)
4. After stream closure, `readMessageEnd` should be called

## Future Work

- Support for flow control and backpressure
- Cancellation semantics
- Timeout configuration
- Integration with async/await patterns in languages that support them

## References

- JIRA Issue: THRIFT-1948
- Compiler changes: See `compiler/cpp/src/thrift/parse/t_stream.h`
- Protocol types: See `lib/cpp/src/thrift/protocol/TEnum.h`
